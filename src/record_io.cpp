#include "record_io.h"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <system_error>

#include "normalize.h"

namespace {

template <class T>
T parse_number(const std::string& token) {
    T value{};
    const char* first = token.data();
    const char* last = first + token.size();
    const auto [end, error] = std::from_chars(first, last, value);
    if (error != std::errc() || end != last) {
        throw InvalidInputException("record", "некорректное число: " + token);
    }
    return value;
}

}

RecordWriter::RecordWriter() {
    out_.imbue(std::locale::classic());
    out_.precision(std::numeric_limits<double>::max_digits10);
}

void RecordWriter::separate() {
    if (!first_) {
        out_ << ' ';
    }
    first_ = false;
}

void RecordWriter::put_token(const std::string& token) {
    separate();
    out_ << token;
}

void RecordWriter::put_id(Id id) {
    separate();
    out_ << id;
}

void RecordWriter::put_count(std::size_t count) {
    separate();
    out_ << count;
}

void RecordWriter::put_bool(bool value) {
    separate();
    out_ << (value ? '1' : '0');
}

void RecordWriter::put_double(double value) {
    if (!std::isfinite(value)) {
        throw InvalidInputException("record", "нельзя сохранить нечисловое значение");
    }
    separate();
    out_ << value;
}

void RecordWriter::put_string(const std::string& value) {
    separate();
    out_ << std::quoted(value);
}

void RecordWriter::put_time(TimePoint value) {
    separate();
    out_ << to_epoch_ms(value);
}

void RecordWriter::put_optional_id(const std::optional<Id>& value) {
    put_bool(value.has_value());
    if (value) {
        put_id(*value);
    }
}

void RecordWriter::put_optional_time(const std::optional<TimePoint>& value) {
    put_bool(value.has_value());
    if (value) {
        put_time(*value);
    }
}

std::string RecordWriter::str() const { return out_.str(); }

RecordReader::RecordReader(std::string line) : in_(std::move(line)) { in_.imbue(std::locale::classic()); }

std::string RecordReader::get_token() {
    std::string token;
    if (!(in_ >> token)) {
        throw InvalidInputException("record", "в записи не хватает полей");
    }
    return token;
}

Id RecordReader::get_id() {
    const auto value = parse_number<std::uint64_t>(get_token());
    if (value > std::numeric_limits<Id>::max()) {
        throw InvalidInputException("record", "идентификатор вне допустимого диапазона");
    }
    return static_cast<Id>(value);
}

std::size_t RecordReader::get_count(std::size_t limit) {
    const auto value = parse_number<std::size_t>(get_token());
    if (value > limit) {
        throw InvalidInputException("record", "слишком много элементов во вложенной коллекции");
    }
    return value;
}

bool RecordReader::get_bool() {
    const std::string token = get_token();
    if (token == "0") {
        return false;
    }
    if (token == "1") {
        return true;
    }
    throw InvalidInputException("record", "ожидалось логическое значение 0 или 1");
}

double RecordReader::get_double() {
    const auto value = parse_number<double>(get_token());
    if (!std::isfinite(value)) {
        throw InvalidInputException("record", "ожидалось конечное число");
    }
    return value;
}

std::string RecordReader::get_string() {
    in_ >> std::ws;
    if (in_.peek() != '"') {
        throw InvalidInputException("record", "ожидалась строка в кавычках");
    }
    std::string value;
    if (!(in_ >> std::quoted(value))) {
        throw InvalidInputException("record", "строка в кавычках не закрыта");
    }
    if (value.size() > kMaxStringBytes) {
        throw InvalidInputException("record", "строка длиннее допустимого размера");
    }
    const auto next = in_.peek();
    if (next != std::char_traits<char>::eof() && next != ' ') {
        throw InvalidInputException("record", "после строки ожидался пробел");
    }
    return value;
}

TimePoint RecordReader::get_time() { return from_epoch_ms(parse_number<std::int64_t>(get_token())); }

std::optional<Id> RecordReader::get_optional_id() {
    if (!get_bool()) {
        return std::nullopt;
    }
    return get_id();
}

std::optional<TimePoint> RecordReader::get_optional_time() {
    if (!get_bool()) {
        return std::nullopt;
    }
    return get_time();
}

void RecordReader::finish() {
    in_ >> std::ws;
    if (in_.peek() != std::char_traits<char>::eof()) {
        throw InvalidInputException("record", "в конце записи лишние данные");
    }
}
