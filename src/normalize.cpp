#include "normalize.h"

#include <format>

#include "exceptions.h"

namespace {

constexpr std::string_view kCountryCode = "375";
constexpr std::string_view kPhoneSeparators = " -";
constexpr std::size_t kNationalPhoneLength = 9;
constexpr std::size_t kMobileCodeLength = 2;
constexpr std::size_t kSubscriberHeadLength = 3;
constexpr std::size_t kSubscriberPairLength = 2;
constexpr std::size_t kFirstSubscriberBoundary = kMobileCodeLength + kSubscriberHeadLength;
constexpr std::size_t kSecondSubscriberBoundary = kFirstSubscriberBoundary + kSubscriberPairLength;
constexpr std::size_t kMaxLoginLength = 32;
constexpr const char* kBlanks = " \t\r\n";

constexpr std::size_t kTwoByteLength = 2;
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

bool has_mobile_prefix(std::string_view national) {
    const auto code = national.substr(0, kMobileCodeLength);
    const char first_digit = national[kMobileCodeLength];
    // Active mobile ranges in the E.164 plan published by Beltelecom.
    // https://www.beltelecom.by/en/private/telephony/phone-codes
    return code == "33" || code == "44" ||
           (code == "25" && std::string_view("5679").find(first_digit) != std::string_view::npos) ||
           (code == "29" && std::string_view("12356789").find(first_digit) != std::string_view::npos);
}

[[noreturn]] void invalid_phone_parentheses() {
    throw InvalidInputException("phone", "скобки допустимы только вокруг двух цифр кода оператора, например (29).");
}

void check_phone_parentheses(std::string_view text) {
    std::size_t open_count = 0;
    std::size_t pair_count = 0;
    for (const char character : text) {
        if (character == '(') {
            ++open_count;
        } else if (character == ')') {
            if (open_count == 0) {
                throw InvalidInputException("phone", "лишняя закрывающая скобка «)».");
            }
            --open_count;
            ++pair_count;
        }
    }
    if (open_count != 0) {
        throw InvalidInputException("phone", "пропущена закрывающая скобка «)».");
    }
    if (pair_count > 1) {
        throw InvalidInputException("phone", "в номере допустима одна пара скобок вокруг кода оператора. Пример — (29).");
    }
}

[[noreturn]] void invalid_phone_hyphen() {
    throw InvalidInputException("phone", "дефис допустим только между частями номера, по одному.");
}

[[noreturn]] void invalid_phone_length() {
    throw InvalidInputException("phone", "после кода страны 375 должно быть ровно 9 цифр.");
}

void skip_phone_spaces(std::string_view& text) {
    while (text.starts_with(' ')) text.remove_prefix(1);
}

bool take_phone_separator(std::string_view& text) {
    const auto initial_size = text.size();
    skip_phone_spaces(text);
    if (text.starts_with('-')) {
        text.remove_prefix(1);
        skip_phone_spaces(text);
        if (text.empty() || text.starts_with('-')) invalid_phone_hyphen();
    }
    return text.size() != initial_size;
}

std::string_view take_phone_digits(std::string_view& text) {
    const auto digits = text.substr(0, text.find_first_not_of("0123456789"));
    text.remove_prefix(digits.size());
    return digits;
}

bool has_split_country_code(std::string_view text) {
    for (const char digit : kCountryCode) {
        const auto next = text.find_first_not_of(kPhoneSeparators);
        if (next == std::string_view::npos || text[next] != digit) return false;
        text.remove_prefix(next + 1);
    }
    return true;
}

std::string parse_mobile_number(std::string_view text) {
    skip_phone_spaces(text);
    while (text.ends_with(' ')) text.remove_suffix(1);
    if (text.empty()) {
        throw InvalidInputException("phone", "мобильный номер не указан.");
    }
    if (text.find_first_not_of("0123456789 +()-") != std::string_view::npos) {
        throw InvalidInputException("phone", "номер содержит буквы или другие недопустимые символы.");
    }
    check_phone_parentheses(text);
    if (text.starts_with('+')) {
        text.remove_prefix(1);
        if (text.starts_with(' ')) {
            throw InvalidInputException("phone", "знак «+» пишется слитно с кодом страны.");
        }
    }
    if (text.find('+') != std::string_view::npos) {
        throw InvalidInputException("phone", "знак «+» допустим только один раз в начале номера.");
    }
    if (text.starts_with('(')) invalid_phone_parentheses();
    if (text.starts_with('-')) invalid_phone_hyphen();
    if (!text.starts_with(kCountryCode)) {
        if (has_split_country_code(text)) {
            throw InvalidInputException("phone", "код страны 375 пишется слитно, без пробелов и дефисов.");
        }
        throw InvalidInputException("phone", "код страны должен быть +375 или 375.");
    }
    text.remove_prefix(kCountryCode.size());
    take_phone_separator(text);

    std::string_view parenthesized_code;
    if (text.starts_with('(')) {
        text.remove_prefix(1);
        const auto closing = text.find(')');
        const auto code_in_brackets = text.substr(0, closing);
        if (code_in_brackets.find_first_of(kPhoneSeparators) != std::string_view::npos) {
            throw InvalidInputException("phone", "пробелы и дефисы внутри скобок недопустимы. Пример — (29).");
        }
        if (code_in_brackets.size() != kMobileCodeLength) invalid_phone_parentheses();
        parenthesized_code = code_in_brackets;
        text.remove_prefix(closing + 1);
        take_phone_separator(text);
    }

    std::string national(parenthesized_code);
    while (!text.empty()) {
        const auto digits = take_phone_digits(text);
        if (digits.empty()) invalid_phone_parentheses();
        if (digits.size() > kNationalPhoneLength - national.size()) invalid_phone_length();
        national += digits;
        if (text.empty()) break;
        if (!take_phone_separator(text)) invalid_phone_parentheses();

        const auto length = national.size();
        if (length < kNationalPhoneLength && length != kMobileCodeLength &&
            length != kFirstSubscriberBoundary && length != kSecondSubscriberBoundary) {
            throw InvalidInputException("phone",
                "пробел или дефис разбивает группу цифр. Пример группировки — +375 (29) 123-45-67.");
        }
    }
    if (national.size() != kNationalPhoneLength) invalid_phone_length();

    if (!has_mobile_prefix(national)) {
        throw InvalidInputException("phone",
            "номер вне мобильных диапазонов Беларуси. Проверьте код оператора 25, 29, 33 или 44 и цифры после него.");
    }
    return national;
}

bool is_login_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '_' || c == '-';
}

}

std::string normalize_phone(std::string_view phone) {
    return std::string(kCountryCode) + parse_mobile_number(phone);
}

std::string format_phone(std::string_view phone) {
    const std::string national = parse_mobile_number(phone);
    const std::string_view digits = national;
    const auto subscriber = digits.substr(kMobileCodeLength);
    return std::format("+{} ({}) {}-{}-{}", kCountryCode, digits.substr(0, kMobileCodeLength),
                       subscriber.substr(0, kSubscriberHeadLength),
                       subscriber.substr(kSubscriberHeadLength, kSubscriberPairLength),
                       subscriber.substr(kSubscriberHeadLength + kSubscriberPairLength));
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
    for (char c : login) {
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
        const bool has_pair = i + kTwoByteLength <= text.size();
        const auto second = has_pair ? static_cast<unsigned char>(text[i + kTwoByteLength - 1]) : static_cast<unsigned char>(0);
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
            i += kTwoByteLength;
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
