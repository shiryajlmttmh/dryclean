#include "employee.h"

#include <istream>
#include <ostream>
#include <utility>

#include "exceptions.h"
#include "normalize.h"
#include "record_io.h"
#include "password.h"

Employee::Employee(std::string name, std::string login, EmployeeRole role) {
    set_name(std::move(name));
    set_login(std::move(login));
    set_role(role);
}

Id Employee::id() const noexcept { return id_; }

const std::string& Employee::name() const noexcept { return name_; }

const std::string& Employee::login() const noexcept { return login_; }

EmployeeRole Employee::role() const noexcept { return role_; }

void Employee::set_id(Id id) noexcept { id_ = id; }

void Employee::set_name(std::string name) {
    name = normalize_text(std::move(name));
    require_text("name", name);
    name_ = std::move(name);
}

void Employee::set_login(std::string login) { login_ = normalize_login(std::move(login)); }

void Employee::set_password(const std::string& value) { password_hash_ = password::hash(value); }

void Employee::copy_password_from(const Employee& employee) {
    password_hash_ = employee.password_hash_;
}

bool Employee::has_password() const noexcept { return !password_hash_.empty(); }

bool Employee::check_password(const std::string& value) const { return password::verify(value, password_hash_); }

void Employee::set_role(EmployeeRole role) {
    if (!is_valid(role)) {
        throw InvalidInputException("role", "неизвестная роль сотрудника");
    }
    role_ = role;
}

bool Employee::has_permission(Permission permission) const noexcept {
    return permissions_for(role_).contains(permission);
}

std::set<Permission> Employee::permissions_for(EmployeeRole role) {
    switch (role) {
        case EmployeeRole::Receptionist:
            return {Permission::ManageClients, Permission::AcceptOrders, Permission::IssueOrders};
        case EmployeeRole::Operator:
            return {Permission::ChangeStatus, Permission::BuildPlan, Permission::ConfirmPlan,
                    Permission::ManageStock};
        case EmployeeRole::Administrator:
            return {Permission::ManageClients, Permission::AcceptOrders, Permission::IssueOrders,
                    Permission::ChangeStatus,  Permission::BuildPlan,    Permission::ConfirmPlan,
                    Permission::ManageStock,   Permission::ManageStaff};
    }
    throw InvalidInputException("role", "неизвестная роль сотрудника");
}

void Employee::validate() const {
    if (!password_hash_.empty() && !password::valid_hash(password_hash_)) {
        throw InvalidInputException("password", "некорректный формат хеша пароля");
    }
    require_text("name", name_);
    if (normalize_login(login_) != login_) {
        throw InvalidInputException("login", "логин должен храниться в нормализованном виде");
    }
    if (!is_valid(role_)) {
        throw InvalidInputException("role", "неизвестная роль сотрудника");
    }
}

bool Employee::operator==(const Employee& other) const noexcept { return id_ == other.id_; }

std::strong_ordering Employee::operator<=>(const Employee& other) const noexcept { return id_ <=> other.id_; }

std::ostream& operator<<(std::ostream& os, const Employee& employee) {
    RecordWriter writer;
    writer.put_id(employee.id_);
    writer.put_string(employee.name_);
    writer.put_string(employee.login_);
    writer.put_token(to_string(employee.role_));
    writer.put_string(employee.password_hash_);
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Employee& employee) {
    return read_record(is, employee, [](Employee& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        value.name_ = reader.get_string();
        value.login_ = reader.get_string();
        value.role_ = parse_EmployeeRole(reader.get_token());
        value.password_hash_ = reader.get_string();
    });
}
