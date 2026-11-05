#pragma once

#include <compare>
#include <iosfwd>
#include <set>
#include <string>

#include "types.h"

class Material {
public:
    Material() = default;
    Material(std::string name, std::string unit, double stock, double consumption_per_kg,
             std::set<ServiceType> services);

    Id id() const noexcept;
    const std::string& name() const noexcept;
    const std::string& unit() const noexcept;
    double stock() const noexcept;
    double consumption_per_kg() const noexcept;
    const std::set<ServiceType>& services() const noexcept;

    void set_id(Id id) noexcept;
    void set_name(std::string name);
    void set_consumption_per_kg(double consumption_per_kg);
    void set_services(std::set<ServiceType> services);
    void add_stock(double amount);
    void write_off(double amount);

    double required_for(double weight_kg) const;
    bool is_used_for(ServiceType service) const noexcept;
    void validate() const;

    bool operator==(const Material& other) const noexcept;
    std::strong_ordering operator<=>(const Material& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Material& material);
    friend std::istream& operator>>(std::istream& is, Material& material);

private:
    Id id_ = 0;
    std::string name_;
    std::string unit_;
    double stock_ = 0.0;
    double consumption_per_kg_ = 0.0;
    std::set<ServiceType> services_;
};
