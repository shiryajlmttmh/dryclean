#include "normalize.h"

#include <string_view>

#include "exceptions.h"

namespace {

constexpr std::size_t kMinPhoneDigits = 7;
constexpr std::size_t kMaxPhoneDigits = 15;
constexpr std::size_t kMaxLoginLength = 32;
constexpr const char* kBlanks = " \t\r\n";

constexpr unsigned char kAsciiLimit = 0x80;
constexpr unsigned char kTwoByteMask = 0xE0;
constexpr unsigned char kTwoByteMarker = 0xC0;
constexpr unsigned char kTwoByteDataMask = 0x1F;
constexpr unsigned char kContinuationMask = 0xC0;
constexpr unsigned char kContinuationMarker = 0x80;
constexpr unsigned char kContinuationDataMask = 0x3F;
constexpr unsigned kContinuationBits = 6;

constexpr char32_t kCyrillicUpperFirst = 0x0410;
constexpr char32_t kCyrillicUpperLast = 0x042F;
constexpr char32_t kCyrillicUpperShift = 0x20;
constexpr char32_t kCyrillicExtendedFirst = 0x0400;
constexpr char32_t kCyrillicExtendedLast = 0x040F;
constexpr char32_t kCyrillicExtendedShift = 0x50;

bool is_blank(char c) { return std::string_view(kBlanks).find(c) != std::string_view::npos; }

char to_lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

char32_t to_lower_cyrillic(char32_t code_point) {
    if (code_point >= kCyrillicUpperFirst && code_point <= kCyrillicUpperLast) {
        return code_point + kCyrillicUpperShift;
    }
    if (code_point >= kCyrillicExtendedFirst && code_point <= kCyrillicExtendedLast) {
        return code_point + kCyrillicExtendedShift;
    }
    return code_point;
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool is_phone_separator(char c) { return c == ' ' || c == '(' || c == ')' || c == '-'; }

bool is_login_char(char c) {
    return (c >= 'a' && c <= 'z') || is_digit(c) || c == '_' || c == '-';
}

}

std::string normalize_phone(std::string phone) {
    std::string digits;
    bool plus_allowed = true;
    for (const char c : phone) {
        if (is_phone_separator(c)) {
            continue;
        }
        if (c == '+' && plus_allowed) {
            plus_allowed = false;
            continue;
        }
        if (!is_digit(c)) {
            throw InvalidInputException(
                "phone", "телефон может содержать только цифры, пробелы, скобки, дефисы и ведущий плюс");
        }
        plus_allowed = false;
        digits.push_back(c);
    }
    if (digits.size() < kMinPhoneDigits || digits.size() > kMaxPhoneDigits) {
        throw InvalidInputException("phone", "телефон должен содержать от " + std::to_string(kMinPhoneDigits) +
                                                 " до " + std::to_string(kMaxPhoneDigits) + " цифр");
    }
    return digits;
}

std::string normalize_login(std::string login) {
    const auto first = login.find_first_not_of(kBlanks);
    if (first == std::string::npos) {
        throw InvalidInputException("login", "логин не может быть пустым");
    }
    const auto last = login.find_last_not_of(kBlanks);
    login = login.substr(first, last - first + 1);
    if (login.size() > kMaxLoginLength) {
        throw InvalidInputException("login", "логин не может быть длиннее " + std::to_string(kMaxLoginLength) +
                                                 " символов");
    }
    for (char& c : login) {
        c = to_lower_ascii(c);
        if (!is_login_char(c)) {
            throw InvalidInputException("login",
                                        "логин может содержать только латинские буквы, цифры, '_' и '-'");
        }
    }
    return login;
}

std::string normalize_text(std::string text) {
    for (char& c : text) {
        if (is_blank(c)) {
            c = ' ';
        }
    }
    const auto first = text.find_first_not_of(kBlanks);
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(kBlanks);
    return text.substr(first, last - first + 1);
}

std::string to_lower_text(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        const bool has_pair = i + 1 < text.size();
        const auto second = has_pair ? static_cast<unsigned char>(text[i + 1]) : static_cast<unsigned char>(0);
        if (lead < kAsciiLimit) {
            result.push_back(to_lower_ascii(text[i]));
            ++i;
        } else if ((lead & kTwoByteMask) == kTwoByteMarker && has_pair &&
                   (second & kContinuationMask) == kContinuationMarker) {
            char32_t code_point = (static_cast<char32_t>(lead & kTwoByteDataMask) << kContinuationBits) |
                                  (second & kContinuationDataMask);
            code_point = to_lower_cyrillic(code_point);
            result.push_back(static_cast<char>(kTwoByteMarker | (code_point >> kContinuationBits)));
            result.push_back(static_cast<char>(kContinuationMarker | (code_point & kContinuationDataMask)));
            i += 2;
        } else {
            result.push_back(text[i]);
            ++i;
        }
    }
    return result;
}

void require_text(const char* field_name, const std::string& value) {
    if (value.empty()) {
        throw InvalidInputException(field_name, "значение не может быть пустым");
    }
    if (value.size() > kMaxStringBytes) {
        throw InvalidInputException(field_name, "значение длиннее " + std::to_string(kMaxStringBytes) + " байт");
    }
    for (const char c : value) {
        const auto code = static_cast<unsigned char>(c);
        if (code < ' ' || code == '\x7F') {
            throw InvalidInputException(field_name, "значение содержит управляющие символы");
        }
    }
}
