#pragma once

#include <compare>
#include <iosfwd>
#include <optional>
#include <vector>

#include "status_record.h"
#include "types.h"

class Order {
public:
    Order() = default;
    Order(Id client_id, ServiceType service_type, bool urgent, Id employee_id, TimePoint created_at,
          double discount_snapshot);

    Id id() const noexcept;
    Id client_id() const noexcept;
    const std::vector<Id>& item_ids() const noexcept;
    ServiceType service_type() const noexcept;
    bool is_urgent() const noexcept;
    OrderStatus status() const noexcept;
    TimePoint created_at() const noexcept;
    TimePoint deadline() const noexcept;
    std::optional<TimePoint> planned_ready_at() const noexcept;
    std::optional<TimePoint> actual_ready_at() const noexcept;
    std::optional<TimePoint> issued_at() const noexcept;
    Id employee_id() const noexcept;
    double discount_snapshot() const noexcept;
    double total_cost() const noexcept;
    const std::vector<StatusRecord>& history() const noexcept;

    void set_id(Id id) noexcept;
    void add_item_id(Id item_id);
    void set_deadline(TimePoint deadline);
    void set_planned_ready_at(std::optional<TimePoint> planned_ready_at);
    void set_total_cost(double total_cost);

    bool contains_item(Id item_id) const noexcept;
    static bool is_transition_allowed(OrderStatus from, OrderStatus to) noexcept;
    void change_status(OrderStatus status, Id employee_id, TimePoint when);
    bool is_overdue(TimePoint now) const noexcept;
    void validate() const;

    bool operator==(const Order& other) const noexcept;
    std::strong_ordering operator<=>(const Order& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Order& order);
    friend std::istream& operator>>(std::istream& is, Order& order);

private:
    Id id_ = 0;
    Id client_id_ = 0;
    std::vector<Id> item_ids_;
    ServiceType service_type_ = ServiceType::DryCleaning;
    bool urgent_ = false;
    OrderStatus status_ = OrderStatus::Accepted;
    TimePoint created_at_{};
    TimePoint deadline_{};
    std::optional<TimePoint> planned_ready_at_;
    std::optional<TimePoint> actual_ready_at_;
    std::optional<TimePoint> issued_at_;
    Id employee_id_ = 0;
    double discount_snapshot_ = 0.0;
    double total_cost_ = 0.0;
    std::vector<StatusRecord> history_;
};
