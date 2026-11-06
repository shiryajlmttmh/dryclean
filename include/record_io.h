#pragma once

#include <cstddef>
#include <istream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "exceptions.h"
#include "types.h"

constexpr std::size_t kMaxNestedElements = 10000;

class RecordWriter {
public:
    RecordWriter();

    void put_token(const std::string& token);
    void put_id(Id id);
    void put_count(std::size_t count);
    void put_bool(bool value);
    void put_double(double value);
    void put_string(const std::string& value);
    void put_time(TimePoint value);
    void put_optional_id(const std::optional<Id>& value);
    void put_optional_time(const std::optional<TimePoint>& value);
    std::string str() const;

private:
    void separate();

    std::ostringstream out_;
    bool first_ = true;
};

class RecordReader {
public:
    explicit RecordReader(std::string line);

    std::string get_token();
    Id get_id();
    std::size_t get_count(std::size_t limit = kMaxNestedElements);
    bool get_bool();
    double get_double();
    std::string get_string();
    TimePoint get_time();
    std::optional<Id> get_optional_id();
    std::optional<TimePoint> get_optional_time();
    void finish();

private:
    std::istringstream in_;
};

template <class T, class Fill>
std::istream& read_record(std::istream& is, T& target, Fill&& fill) {
    std::string line;
    if (!std::getline(is, line)) {
        return is;
    }
    try {
        RecordReader reader(std::move(line));
        T temp;
        fill(temp, reader);
        reader.finish();
        temp.validate();
        target = std::move(temp);
    } catch (const ShopException&) {
        is.setstate(std::ios::failbit);
    }
    return is;
}
