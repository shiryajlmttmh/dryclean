#pragma once

#include <iosfwd>

#include "types.h"

class RecordWriter;
class RecordReader;

class StatusRecord {
public:
    StatusRecord() = default;
    StatusRecord(OrderStatus status, Id employee_id, TimePoint timestamp);

    OrderStatus status() const noexcept;
    Id employee_id() const noexcept;
    TimePoint timestamp() const noexcept;

    void validate() const;

    bool operator==(const StatusRecord& other) const = default;

    friend void write_status_record(RecordWriter& writer, const StatusRecord& record);
    friend StatusRecord read_status_record(RecordReader& reader);

private:
    OrderStatus status_ = OrderStatus::Accepted;
    Id employee_id_ = 0;
    TimePoint timestamp_{};
};

void write_status_record(RecordWriter& writer, const StatusRecord& record);
StatusRecord read_status_record(RecordReader& reader);

std::ostream& operator<<(std::ostream& os, const StatusRecord& record);
std::istream& operator>>(std::istream& is, StatusRecord& record);
