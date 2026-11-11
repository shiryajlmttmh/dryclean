#include "batch.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
#include <utility>

#include "exceptions.h"
#include "record_io.h"
#include "validation.h"

Batch::Batch(std::vector<Id> item_ids, Id machine_id, ServiceType service_type, double total_weight_kg,
             double fill_ratio, TimePoint planned_start_at, TimePoint planned_ready_at, Id created_by)
    : item_ids_(std::move(item_ids)),
      machine_id_(machine_id),
      service_type_(service_type),
      total_weight_kg_(total_weight_kg),
      fill_ratio_(fill_ratio),
      planned_start_at_(planned_start_at),
      planned_ready_at_(planned_ready_at),
      created_by_(created_by) {
    validate();
}

Id Batch::id() const noexcept { return id_; }

const std::vector<Id>& Batch::item_ids() const noexcept { return item_ids_; }

Id Batch::machine_id() const noexcept { return machine_id_; }

ServiceType Batch::service_type() const noexcept { return service_type_; }

double Batch::total_weight_kg() const noexcept { return total_weight_kg_; }

double Batch::fill_ratio() const noexcept { return fill_ratio_; }

TimePoint Batch::planned_start_at() const noexcept { return planned_start_at_; }

TimePoint Batch::planned_ready_at() const noexcept { return planned_ready_at_; }

Id Batch::created_by() const noexcept { return created_by_; }

BatchStatus Batch::status() const noexcept { return status_; }

std::optional<TimePoint> Batch::completed_at() const noexcept { return completed_at_; }

std::optional<Id> Batch::completed_by() const noexcept { return completed_by_; }

void Batch::set_id(Id id) noexcept { id_ = id; }

void Batch::complete(Id employee_id, TimePoint now) {
    if (status_ != BatchStatus::Scheduled) {
        throw InvalidInputException("status", "партия уже завершена");
    }
    check_nonzero_id("employee_id", employee_id);
    if (now < planned_ready_at_) {
        throw InvalidInputException("now", "партию нельзя завершить раньше планового времени готовности");
    }
    status_ = BatchStatus::Completed;
    completed_at_ = now;
    completed_by_ = employee_id;
}

bool Batch::contains_item(Id item_id) const noexcept {
    return std::find(item_ids_.begin(), item_ids_.end(), item_id) != item_ids_.end();
}

void Batch::validate() const {
    check_unique_ids("item_ids", item_ids_);
    if (item_ids_.empty()) {
        throw InvalidInputException("item_ids", "партия не может быть пустой");
    }
    check_nonzero_id("machine_id", machine_id_);
    if (!is_valid(service_type_)) {
        throw InvalidInputException("service_type", "неизвестный вид услуги");
    }
    if (!std::isfinite(total_weight_kg_) || total_weight_kg_ <= 0.0) {
        throw InvalidInputException("total_weight_kg", "масса партии должна быть больше нуля");
    }
    if (!std::isfinite(fill_ratio_) || fill_ratio_ <= 0.0 || fill_ratio_ > 1.0 + EPS) {
        throw InvalidInputException("fill_ratio", "заполнение партии должно быть больше нуля и не больше единицы");
    }
    if (planned_ready_at_ <= planned_start_at_) {
        throw InvalidInputException("planned_ready_at", "конец партии должен быть позже начала");
    }
    check_nonzero_id("created_by", created_by_);
    if (!is_valid(status_)) {
        throw InvalidInputException("status", "неизвестный статус партии");
    }
    if (status_ == BatchStatus::Scheduled) {
        if (completed_at_ || completed_by_) {
            throw InvalidInputException("status", "у незавершённой партии не должно быть данных о завершении");
        }
        return;
    }
    if (!completed_at_ || !completed_by_) {
        throw InvalidInputException("status", "у завершённой партии должны быть время и сотрудник");
    }
    check_nonzero_id("completed_by", *completed_by_);
    if (*completed_at_ < planned_ready_at_) {
        throw InvalidInputException("completed_at", "партия не может быть завершена раньше планового времени");
    }
}

bool Batch::operator==(const Batch& other) const noexcept { return id_ == other.id_; }

std::strong_ordering Batch::operator<=>(const Batch& other) const noexcept { return id_ <=> other.id_; }

std::ostream& operator<<(std::ostream& os, const Batch& batch) {
    RecordWriter writer;
    writer.put_id(batch.id_);
    writer.put_count(batch.item_ids_.size());
    for (const Id item_id : batch.item_ids_) {
        writer.put_id(item_id);
    }
    writer.put_id(batch.machine_id_);
    writer.put_token(to_string(batch.service_type_));
    writer.put_double(batch.total_weight_kg_);
    writer.put_double(batch.fill_ratio_);
    writer.put_time(batch.planned_start_at_);
    writer.put_time(batch.planned_ready_at_);
    writer.put_id(batch.created_by_);
    writer.put_token(to_string(batch.status_));
    writer.put_optional_time(batch.completed_at_);
    writer.put_optional_id(batch.completed_by_);
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Batch& batch) {
    return read_record(is, batch, [](Batch& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        const std::size_t count = reader.get_count();
        for (std::size_t i = 0; i < count; ++i) {
            value.item_ids_.push_back(reader.get_id());
        }
        value.machine_id_ = reader.get_id();
        value.service_type_ = parse_ServiceType(reader.get_token());
        value.total_weight_kg_ = reader.get_double();
        value.fill_ratio_ = reader.get_double();
        value.planned_start_at_ = reader.get_time();
        value.planned_ready_at_ = reader.get_time();
        value.created_by_ = reader.get_id();
        value.status_ = parse_BatchStatus(reader.get_token());
        value.completed_at_ = reader.get_optional_time();
        value.completed_by_ = reader.get_optional_id();
    });
}
