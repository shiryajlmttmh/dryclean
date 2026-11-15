#include "password.h"

#include <array>
#include <algorithm>
#include <string_view>

#include "exceptions.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace {

constexpr int kIterations = 600000;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kHashBytes = 32;
constexpr std::size_t kMinPasswordCharacters = 8;
constexpr std::size_t kMaxPasswordBytes = 256;
constexpr std::size_t kHexCharsPerByte = 2;
constexpr std::size_t kHashSeparatorLength = 1;
constexpr int kBitsPerHexDigit = 4;
constexpr unsigned char kHexDigitMask = 0x0f;
constexpr unsigned char kUtf8BytePrefixMask = 0xc0;
constexpr unsigned char kUtf8ContinuationPrefix = 0x80;
constexpr int kOpenSslSuccess = 1;
constexpr unsigned long kNoFlags = 0;
constexpr char kHashSeparator = '$';
const std::string kPrefix = "pbkdf2-sha256$" + std::to_string(kIterations) + kHashSeparator;
constexpr char kHex[] = "0123456789abcdef";
constexpr std::size_t kHexDigitCount = sizeof(kHex) - 1;

std::string hex(const unsigned char* bytes, std::size_t size) {
    std::string result;
    result.reserve(size * kHexCharsPerByte);
    for (std::size_t i = 0; i < size; ++i) {
        result += kHex[bytes[i] >> kBitsPerHexDigit];
        result += kHex[bytes[i] & kHexDigitMask];
    }
    return result;
}

std::array<unsigned char, kHashBytes> derive(const std::string& value,
                                           const std::array<unsigned char, kSaltBytes>& salt) {
    std::array<unsigned char, kHashBytes> result{};
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                     BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) {
        throw ShopException("Не удалось открыть алгоритм хеширования пароля");
    }
    const auto status = BCryptDeriveKeyPBKDF2(
        algorithm, reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())), static_cast<ULONG>(value.size()),
        const_cast<PUCHAR>(salt.data()), static_cast<ULONG>(salt.size()), kIterations,
        result.data(), static_cast<ULONG>(result.size()), kNoFlags);
    BCryptCloseAlgorithmProvider(algorithm, kNoFlags);
    if (status < 0) {
        throw ShopException("Не удалось вычислить хеш пароля");
    }
#else
    if (PKCS5_PBKDF2_HMAC(value.data(), static_cast<int>(value.size()), salt.data(),
                          static_cast<int>(salt.size()), kIterations, EVP_sha256(),
                          static_cast<int>(result.size()), result.data()) != kOpenSslSuccess) {
        throw ShopException("Не удалось вычислить хеш пароля");
    }
#endif
    return result;
}

}

bool password::valid_hash(const std::string& encoded) {
    constexpr std::size_t kSaltHexLength = kSaltBytes * kHexCharsPerByte;
    constexpr std::size_t kHashHexLength = kHashBytes * kHexCharsPerByte;
    const std::size_t separator_offset = kPrefix.size() + kSaltHexLength;
    const std::size_t encoded_length = separator_offset + kHashSeparatorLength + kHashHexLength;

    if (!encoded.starts_with(kPrefix) ||
        encoded.size() != encoded_length ||
        encoded[separator_offset] != kHashSeparator) {
        return false;
    }
    for (std::size_t i = kPrefix.size(); i < encoded.size(); ++i) {
        if (i == separator_offset) continue;
        if (std::string_view(kHex, kHexDigitCount).find(encoded[i]) == std::string_view::npos) return false;
    }
    return true;
}

std::string password::hash(const std::string& value) {
    const auto characters = std::count_if(value.begin(), value.end(), [](unsigned char c) {
        return (c & kUtf8BytePrefixMask) != kUtf8ContinuationPrefix;
    });
    if (characters < kMinPasswordCharacters || value.size() > kMaxPasswordBytes ||
        std::all_of(value.begin(), value.end(), [](unsigned char c) { return c == ' ' || c == '\t'; })) {
        throw InvalidInputException("password", "пароль должен содержать минимум 8 символов, не более 256 байт UTF-8 и не состоять из пробелов");
    }
    std::array<unsigned char, kSaltBytes> salt{};
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, salt.data(), static_cast<ULONG>(salt.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
#else
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != kOpenSslSuccess) {
#endif
        throw ShopException("Не удалось создать соль пароля");
    }
    const auto derived = derive(value, salt);
    return kPrefix + hex(salt.data(), salt.size()) + kHashSeparator + hex(derived.data(), derived.size());
}

bool password::verify(const std::string& value, const std::string& encoded) {
    if (value.size() > kMaxPasswordBytes || !valid_hash(encoded)) return false;
    std::array<unsigned char, kSaltBytes> salt{};
    for (std::size_t i = 0; i < salt.size(); ++i) {
        const auto offset = kPrefix.size() + i * kHexCharsPerByte;
        salt[i] = static_cast<unsigned char>(std::string_view(kHex, kHexDigitCount).find(encoded[offset]) * kHexDigitCount +
                                             std::string_view(kHex, kHexDigitCount).find(encoded[offset + 1]));
    }
    const auto derived = derive(value, salt);
    const std::string actual = hex(derived.data(), derived.size());
    const auto offset = kPrefix.size() + kSaltBytes * kHexCharsPerByte + kHashSeparatorLength;
    unsigned int difference = 0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        difference |= static_cast<unsigned char>(actual[i] ^ encoded[offset + i]);
    }
    return difference == 0;
}
