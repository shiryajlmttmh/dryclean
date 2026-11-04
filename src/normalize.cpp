#include "normalize.h"

#include "exceptions.h"

namespace {

constexpr std::size_t kMinPhoneDigits = 7;
constexpr std::size_t kMaxPhoneDigits = 15;
constexpr std::size_t kMaxLoginLength = 32;
constexpr const char* kBlanks = " \t\r\n";

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool is_phone_separator(char c) { return c == ' ' || c == '(' || c == ')' || c == '-'; }

bool is_login_char(char c) {
    return (c >= 'a' && c <= 'z') || is_digit(c) || c == '_' || c == '-';
}

}  // namespace

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
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        if (!is_login_char(c)) {
            throw InvalidInputException("login",
                                        "логин может содержать только латинские буквы, цифры, '_' и '-'");
        }
    }
    return login;
}

std::string normalize_text(std::string text) {
    for (char& c : text) {
        if (c == '\r' || c == '\n') {
            c = ' ';
        }
    }
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
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
