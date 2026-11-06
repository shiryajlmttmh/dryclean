#pragma once

#include <compare>
#include <cstddef>
#include <iosfwd>
#include <optional>
#include <vector>

#include "types.h"

class Batch {
public:
    Batch() = default;
    Batch(std::vector<Id> item_ids, Id machine_id, ServiceType service_type, double total_weight_kg,
          double fill_ratio, TimePoint planned_start_at, TimePoint planned_ready_at, Id created_by);

    Id id() const noexcept;
    const std::vector<Id>& item_ids() const noexcept;
    Id machine_id() const noexcept;
    ServiceType service_type() const noexcept;
    double total_weight_kg() const noexcept;
    double fill_ratio() const noexcept;
    TimePoint planned_start_at() const noexcept;
    TimePoint planned_ready_at() const noexcept;
    Id created_by() const noexcept;
    BatchStatus status() const noexcept;
    std::optional<TimePoint> completed_at() const noexcept;
    std::optional<Id> completed_by() const noexcept;

    void set_id(Id id) noexcept;
    void complete(Id employee_id, TimePoint now);

    bool contains_item(Id item_id) const noexcept;
    std::size_t items_count() const noexcept;
    void validate() const;

    bool operator==(const Batch& other) const noexcept;
    std::strong_ordering operator<=>(const Batch& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Batch& batch);
    friend std::istream& operator>>(std::istream& is, Batch& batch);

private:
    Id id_ = 0;
    std::vector<Id> item_ids_;
    Id machine_id_ = 0;
    ServiceType service_type_ = ServiceType::DryCleaning;
    double total_weight_kg_ = 0.0;
    double fill_ratio_ = 0.0;
    TimePoint planned_start_at_{};
    TimePoint planned_ready_at_{};
    Id created_by_ = 0;
    BatchStatus status_ = BatchStatus::Scheduled;
    std::optional<TimePoint> completed_at_;
    std::optional<Id> completed_by_;
};
