#include "order.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
#include <utility>

#include "exceptions.h"
#include "record_io.h"
#include "validation.h"

namespace {

void check_service_type(ServiceType service_type) {
    if (!is_valid(service_type)) {
        throw InvalidInputException("service_type", "неизвестный вид услуги");
    }
}

void check_total_cost(double total_cost) {
    if (!std::isfinite(total_cost) || total_cost < 0.0) {
        throw InvalidInputException("total_cost", "стоимость заказа не может быть отрицательной");
    }
}

void check_history(const std::vector<StatusRecord>& history, OrderStatus status, TimePoint created_at) {
    if (history.empty()) {
        throw InvalidInputException("history", "журнал статусов не может быть пустым");
    }
    if (history.front().status() != OrderStatus::Accepted) {
        throw InvalidInputException("history", "журнал должен начинаться со статуса «Принят»");
    }
    if (history.front().timestamp() < created_at) {
        throw InvalidInputException("history", "первая запись журнала раньше создания заказа");
    }
    for (std::size_t i = 0; i < history.size(); ++i) {
        history[i].validate();
        if (i == 0) {
            continue;
        }
        if (!Order::is_transition_allowed(history[i - 1].status(), history[i].status())) {
            throw InvalidInputException("history", "журнал содержит недопустимый переход статуса");
        }
        if (history[i].timestamp() < history[i - 1].timestamp()) {
            throw InvalidInputException("history", "время в журнале не может убывать");
        }
    }
    if (history.back().status() != status) {
        throw InvalidInputException("history", "последняя запись журнала не совпадает со статусом заказа");
    }
}

void check_result_times(OrderStatus status, const std::optional<TimePoint>& actual_ready_at,
                        const std::optional<TimePoint>& issued_at) {
    const bool is_ready = status == OrderStatus::Ready || status == OrderStatus::Issued;
    if (actual_ready_at.has_value() != is_ready) {
        throw InvalidInputException("actual_ready_at", "время готовности не соответствует статусу заказа");
    }
    if (issued_at.has_value() != (status == OrderStatus::Issued)) {
        throw InvalidInputException("issued_at", "время выдачи не соответствует статусу заказа");
    }
    if (issued_at && actual_ready_at && *issued_at < *actual_ready_at) {
        throw InvalidInputException("issued_at", "выдача не может быть раньше готовности");
    }
}

}

Order::Order(Id client_id, ServiceType service_type, bool urgent, Id employee_id, TimePoint created_at,
             double discount_snapshot)
    : client_id_(client_id),
      service_type_(service_type),
      urgent_(urgent),
      created_at_(created_at),
      deadline_(created_at),
      employee_id_(employee_id),
      discount_snapshot_(discount_snapshot) {
    check_nonzero_id("client_id", client_id_);
    check_service_type(service_type_);
    check_nonzero_id("employee_id", employee_id_);
    check_discount(discount_snapshot_);
    history_.emplace_back(OrderStatus::Accepted, employee_id_, created_at_);
}

Id Order::id() const noexcept { return id_; }

Id Order::client_id() const noexcept { return client_id_; }

const std::vector<Id>& Order::item_ids() const noexcept { return item_ids_; }

ServiceType Order::service_type() const noexcept { return service_type_; }

bool Order::is_urgent() const noexcept { return urgent_; }

OrderStatus Order::status() const noexcept { return status_; }

TimePoint Order::created_at() const noexcept { return created_at_; }

TimePoint Order::deadline() const noexcept { return deadline_; }

std::optional<TimePoint> Order::planned_ready_at() const noexcept { return planned_ready_at_; }

std::optional<TimePoint> Order::actual_ready_at() const noexcept { return actual_ready_at_; }

std::optional<TimePoint> Order::issued_at() const noexcept { return issued_at_; }

Id Order::employee_id() const noexcept { return employee_id_; }

double Order::discount_snapshot() const noexcept { return discount_snapshot_; }

double Order::total_cost() const noexcept { return total_cost_; }

const std::vector<StatusRecord>& Order::history() const noexcept { return history_; }

void Order::set_id(Id id) noexcept { id_ = id; }

void Order::add_item_id(Id item_id) {
    if (status_ != OrderStatus::Accepted) {
        throw InvalidInputException("status", "состав заказа нельзя менять после начала обработки");
    }
    check_nonzero_id("item_id", item_id);
    if (contains_item(item_id)) {
        throw InvalidInputException("item_id", "изделие уже входит в заказ");
    }
    item_ids_.push_back(item_id);
}

void Order::remove_item_id(Id item_id) {
    if (status_ != OrderStatus::Accepted) {
        throw InvalidInputException("status", "состав заказа нельзя менять после начала обработки");
    }
    const auto it = std::find(item_ids_.begin(), item_ids_.end(), item_id);
    if (it == item_ids_.end()) {
        throw InvalidInputException("item_id", "изделия нет в заказе");
    }
    item_ids_.erase(it);
}

void Order::set_deadline(TimePoint deadline) {
    if (deadline < created_at_) {
        throw InvalidInputException("deadline", "срок выдачи не может быть раньше создания заказа");
    }
    deadline_ = deadline;
}

void Order::set_planned_ready_at(std::optional<TimePoint> planned_ready_at) {
    if (planned_ready_at && *planned_ready_at < created_at_) {
        throw InvalidInputException("planned_ready_at", "прогноз готовности не может быть раньше создания заказа");
    }
    planned_ready_at_ = planned_ready_at;
}

void Order::set_total_cost(double total_cost) {
    check_total_cost(total_cost);
    total_cost_ = total_cost;
}

void Order::set_employee_id(Id employee_id) {
    check_nonzero_id("employee_id", employee_id);
    employee_id_ = employee_id;
}

bool Order::contains_item(Id item_id) const noexcept {
    return std::find(item_ids_.begin(), item_ids_.end(), item_id) != item_ids_.end();
}

bool Order::is_transition_allowed(OrderStatus from, OrderStatus to) noexcept {
    return (from == OrderStatus::Accepted && to == OrderStatus::InProgress) ||
           (from == OrderStatus::InProgress && to == OrderStatus::Ready) ||
           (from == OrderStatus::Ready && to == OrderStatus::Issued);
}

void Order::change_status(OrderStatus status, Id employee_id, TimePoint when) {
    if (!is_transition_allowed(status_, status)) {
        throw InvalidInputException("status", "недопустимый переход статуса заказа");
    }
    check_nonzero_id("employee_id", employee_id);
    if (!history_.empty() && when < history_.back().timestamp()) {
        throw InvalidInputException("when", "время смены статуса не может быть раньше предыдущего");
    }
    history_.emplace_back(status, employee_id, when);
    status_ = status;
    if (status == OrderStatus::Ready) {
        actual_ready_at_ = when;
    } else if (status == OrderStatus::Issued) {
        issued_at_ = when;
    }
}

bool Order::is_overdue(TimePoint now) const noexcept {
    return (status_ == OrderStatus::Accepted || status_ == OrderStatus::InProgress) && now > deadline_;
}

void Order::validate() const {
    check_nonzero_id("client_id", client_id_);
    check_unique_ids("item_ids", item_ids_);
    if (item_ids_.empty()) {
        throw InvalidInputException("item_ids", "заказ не может быть пустым");
    }
    check_service_type(service_type_);
    if (!is_valid(status_)) {
        throw InvalidInputException("status", "неизвестный статус заказа");
    }
    if (deadline_ < created_at_) {
        throw InvalidInputException("deadline", "срок выдачи не может быть раньше создания заказа");
    }
    if (planned_ready_at_ && *planned_ready_at_ < created_at_) {
        throw InvalidInputException("planned_ready_at", "прогноз готовности не может быть раньше создания заказа");
    }
    check_nonzero_id("employee_id", employee_id_);
    check_discount(discount_snapshot_);
    check_total_cost(total_cost_);
    check_history(history_, status_, created_at_);
    check_result_times(status_, actual_ready_at_, issued_at_);
}

bool Order::operator==(const Order& other) const noexcept { return id_ == other.id_; }

std::strong_ordering Order::operator<=>(const Order& other) const noexcept { return id_ <=> other.id_; }

std::ostream& operator<<(std::ostream& os, const Order& order) {
    RecordWriter writer;
    writer.put_id(order.id_);
    writer.put_id(order.client_id_);
    writer.put_count(order.item_ids_.size());
    for (const Id item_id : order.item_ids_) {
        writer.put_id(item_id);
    }
    writer.put_token(to_string(order.service_type_));
    writer.put_bool(order.urgent_);
    writer.put_token(to_string(order.status_));
    writer.put_time(order.created_at_);
    writer.put_time(order.deadline_);
    writer.put_optional_time(order.planned_ready_at_);
    writer.put_optional_time(order.actual_ready_at_);
    writer.put_optional_time(order.issued_at_);
    writer.put_id(order.employee_id_);
    writer.put_double(order.discount_snapshot_);
    writer.put_double(order.total_cost_);
    writer.put_count(order.history_.size());
    for (const StatusRecord& record : order.history_) {
        write_status_record(writer, record);
    }
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Order& order) {
    return read_record(is, order, [](Order& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        value.client_id_ = reader.get_id();
        const std::size_t item_count = reader.get_count();
        for (std::size_t i = 0; i < item_count; ++i) {
            value.item_ids_.push_back(reader.get_id());
        }
        value.service_type_ = parse_ServiceType(reader.get_token());
        value.urgent_ = reader.get_bool();
        value.status_ = parse_OrderStatus(reader.get_token());
        value.created_at_ = reader.get_time();
        value.deadline_ = reader.get_time();
        value.planned_ready_at_ = reader.get_optional_time();
        value.actual_ready_at_ = reader.get_optional_time();
        value.issued_at_ = reader.get_optional_time();
        value.employee_id_ = reader.get_id();
        value.discount_snapshot_ = reader.get_double();
        value.total_cost_ = reader.get_double();
        const std::size_t history_count = reader.get_count();
        for (std::size_t i = 0; i < history_count; ++i) {
            value.history_.push_back(read_status_record(reader));
        }
    });
}
