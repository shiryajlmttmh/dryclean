#include "status_record.h"

#include <istream>
#include <ostream>

#include "exceptions.h"
#include "record_io.h"
#include "validation.h"

StatusRecord::StatusRecord(OrderStatus status, Id employee_id, TimePoint timestamp)
    : status_(status), employee_id_(employee_id), timestamp_(timestamp) {
    validate();
}

OrderStatus StatusRecord::status() const noexcept { return status_; }

Id StatusRecord::employee_id() const noexcept { return employee_id_; }

TimePoint StatusRecord::timestamp() const noexcept { return timestamp_; }

void StatusRecord::validate() const {
    if (!is_valid(status_)) {
        throw InvalidInputException("status", "неизвестный статус заказа");
    }
    check_nonzero_id("employee_id", employee_id_);
}

void write_status_record(RecordWriter& writer, const StatusRecord& record) {
    writer.put_token(to_string(record.status_));
    writer.put_id(record.employee_id_);
    writer.put_time(record.timestamp_);
}

StatusRecord read_status_record(RecordReader& reader) {
    StatusRecord record;
    record.status_ = parse_OrderStatus(reader.get_token());
    record.employee_id_ = reader.get_id();
    record.timestamp_ = reader.get_time();
    return record;
}

std::ostream& operator<<(std::ostream& os, const StatusRecord& record) {
    RecordWriter writer;
    write_status_record(writer, record);
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, StatusRecord& record) {
    return read_record(is, record, [](StatusRecord& value, RecordReader& reader) {
        value = read_status_record(reader);
    });
}
