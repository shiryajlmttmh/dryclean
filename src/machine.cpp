#include "machine.h"

#include <cmath>
#include <algorithm>
#include <istream>
#include <ostream>
#include <utility>

#include "exceptions.h"
#include "normalize.h"
#include "record_io.h"

namespace {

void check_type(ServiceType type) {
    if (!is_valid(type)) {
        throw InvalidInputException("type", "неизвестный вид услуги");
    }
}

void check_capacity(double capacity_kg) {
    if (!std::isfinite(capacity_kg) || capacity_kg <= 0.0) {
        throw InvalidInputException("capacity_kg", "вместимость должна быть больше нуля");
    }
}

}

Machine::Machine(std::string name, ServiceType type, double capacity_kg, Hours maintenance_interval) {
    set_name(std::move(name));
    set_type(type);
    set_capacity_kg(capacity_kg);
    set_maintenance_interval(maintenance_interval);
}

Id Machine::id() const noexcept { return id_; }

const std::string& Machine::name() const noexcept { return name_; }

ServiceType Machine::type() const noexcept { return type_; }

double Machine::capacity_kg() const noexcept { return capacity_kg_; }

TimePoint Machine::free_at() const noexcept { return free_at_; }

Hours Machine::maintenance_interval() const noexcept { return maintenance_interval_; }

Hours Machine::hours_since_maintenance() const noexcept { return hours_since_maintenance_; }

Hours Machine::maintenance_remaining() const noexcept {
    return std::max(0.0, maintenance_interval_ - hours_since_maintenance_);
}

void Machine::set_maintenance_interval(Hours hours) {
    if (!std::isfinite(hours) || hours <= 0.0 || hours < hours_since_maintenance_) {
        throw InvalidInputException("maintenance", "интервал ТО должен быть положительным и не меньше занятого ресурса");
    }
    maintenance_interval_ = hours;
}

void Machine::reserve_work(Hours hours) {
    if (!std::isfinite(hours) || hours <= 0.0 || hours > maintenance_remaining() + EPS) {
        throw InvalidInputException("maintenance", "недостаточно часов до ТО машины «" + name_ + "»");
    }
    hours_since_maintenance_ += hours;
}

void Machine::complete_maintenance() noexcept { hours_since_maintenance_ = 0.0; }

void Machine::set_id(Id id) noexcept { id_ = id; }

void Machine::set_name(std::string name) {
    name = normalize_text(std::move(name));
    require_text("name", name);
    name_ = std::move(name);
}

void Machine::set_type(ServiceType type) {
    check_type(type);
    type_ = type;
}

void Machine::set_capacity_kg(double capacity_kg) {
    check_capacity(capacity_kg);
    capacity_kg_ = capacity_kg;
}

void Machine::set_free_at(TimePoint free_at) noexcept { free_at_ = free_at; }

void Machine::occupy_until(TimePoint until) {
    if (until < free_at_) {
        throw InvalidInputException("free_at", "новый момент занятости не может быть раньше текущего");
    }
    free_at_ = until;
}

bool Machine::can_process(ServiceType service) const noexcept { return type_ == service; }

bool Machine::is_free_at(TimePoint moment) const noexcept { return free_at_ <= moment; }

void Machine::validate() const {
    require_text("name", name_);
    check_type(type_);
    check_capacity(capacity_kg_);
    if (!std::isfinite(maintenance_interval_) || maintenance_interval_ <= 0.0 ||
        !std::isfinite(hours_since_maintenance_) || hours_since_maintenance_ < 0.0 ||
        hours_since_maintenance_ > maintenance_interval_) {
        throw InvalidInputException("maintenance", "некорректный ресурс машины до ТО");
    }
}

bool Machine::operator==(const Machine& other) const noexcept { return id_ == other.id_; }

std::strong_ordering Machine::operator<=>(const Machine& other) const noexcept { return id_ <=> other.id_; }

std::ostream& operator<<(std::ostream& os, const Machine& machine) {
    RecordWriter writer;
    writer.put_id(machine.id_);
    writer.put_string(machine.name_);
    writer.put_token(to_string(machine.type_));
    writer.put_double(machine.capacity_kg_);
    writer.put_time(machine.free_at_);
    writer.put_double(machine.maintenance_interval_);
    writer.put_double(machine.hours_since_maintenance_);
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Machine& machine) {
    return read_record(is, machine, [](Machine& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        value.name_ = reader.get_string();
        value.type_ = parse_ServiceType(reader.get_token());
        value.capacity_kg_ = reader.get_double();
        value.free_at_ = reader.get_time();
        value.maintenance_interval_ = reader.get_double();
        value.hours_since_maintenance_ = reader.get_double();
    });
}
