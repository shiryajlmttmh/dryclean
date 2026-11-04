#pragma once

#include <compare>
#include <iosfwd>
#include <set>
#include <string>

#include "types.h"

class Employee {
public:
    Employee() = default;
    Employee(std::string name, std::string login, EmployeeRole role);

    Id id() const noexcept;
    const std::string& name() const noexcept;
    const std::string& login() const noexcept;
    EmployeeRole role() const noexcept;

    void set_id(Id id) noexcept;
    void set_name(std::string name);
    void set_login(std::string login);
    void set_role(EmployeeRole role);

    bool has_permission(Permission permission) const noexcept;
    std::set<Permission> permissions() const;
    static std::set<Permission> permissions_for(EmployeeRole role);
    void validate() const;

    bool operator==(const Employee& other) const noexcept;
    std::strong_ordering operator<=>(const Employee& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Employee& employee);
    friend std::istream& operator>>(std::istream& is, Employee& employee);

private:
    Id id_ = 0;
    std::string name_;
    std::string login_;
    EmployeeRole role_ = EmployeeRole::Receptionist;
};
