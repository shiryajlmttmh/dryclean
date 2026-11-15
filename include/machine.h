#pragma once

#include <compare>
#include <iosfwd>
#include <string>

#include "types.h"

class Machine {
public:
    Machine() = default;
    Machine(std::string name, ServiceType type, double capacity_kg, Hours maintenance_interval);

    Id id() const noexcept;
    const std::string& name() const noexcept;
    ServiceType type() const noexcept;
    double capacity_kg() const noexcept;
    TimePoint free_at() const noexcept;
    Hours maintenance_interval() const noexcept;
    Hours hours_since_maintenance() const noexcept;
    Hours maintenance_remaining() const noexcept;
    void set_maintenance_interval(Hours hours);
    void reserve_work(Hours hours);
    void complete_maintenance() noexcept;

    void set_id(Id id) noexcept;
    void set_name(std::string name);
    void set_type(ServiceType type);
    void set_capacity_kg(double capacity_kg);
    void set_free_at(TimePoint free_at) noexcept;
    void occupy_until(TimePoint until);

    bool can_process(ServiceType service) const noexcept;
    bool is_free_at(TimePoint moment) const noexcept;
    void validate() const;

    bool operator==(const Machine& other) const noexcept;
    std::strong_ordering operator<=>(const Machine& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Machine& machine);
    friend std::istream& operator>>(std::istream& is, Machine& machine);

private:
    Id id_ = 0;
    std::string name_;
    ServiceType type_ = ServiceType::DryCleaning;
    double capacity_kg_ = 0.0;
    TimePoint free_at_{};
    Hours maintenance_interval_ = 0.0;
    Hours hours_since_maintenance_ = 0.0;
};
