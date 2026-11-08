#include "shop.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "exceptions.h"
#include "normalize.h"

namespace {

constexpr std::array<ServiceType, 4> kAllServices{ServiceType::DryCleaning, ServiceType::WetCleaning,
                                                  ServiceType::StainRemoval, ServiceType::Leather};

constexpr const char* kInitialAdminName = "Администратор";
constexpr const char* kInitialAdminLogin = "admin";

template <class T>
void replace_by_id(Repository<T>& repository, const T& value) {
    repository.get(value.id()) = value;
}

}

DryCleaningShop::DryCleaningShop(std::filesystem::path data_dir) : data_dir_(std::move(data_dir)) {
    for (const ServiceType type : kAllServices) {
        services_.emplace(type, make_service(type));
    }
    planner_.add_rule(std::make_unique<FabricRule>());
    planner_.add_rule(std::make_unique<ColorRule>());
    planner_.add_rule(std::make_unique<ModeRule>());
}

const Repository<Client>& DryCleaningShop::clients() const noexcept { return clients_; }
const Repository<Item>& DryCleaningShop::items() const noexcept { return items_; }
const Repository<Order>& DryCleaningShop::orders() const noexcept { return orders_; }
const Repository<Batch>& DryCleaningShop::batches() const noexcept { return batches_; }
const Repository<Material>& DryCleaningShop::materials() const noexcept { return materials_; }
const Repository<Employee>& DryCleaningShop::employees() const noexcept { return employees_; }
const Repository<Machine>& DryCleaningShop::machines() const noexcept { return machines_; }

const Service& DryCleaningShop::service(ServiceType type) const {
    const auto found = services_.find(type);
    if (found == services_.end()) {
        throw InvalidInputException("service_type", "неизвестный вид услуги");
    }
    return *found->second;
}

const std::filesystem::path& DryCleaningShop::data_dir() const noexcept { return data_dir_; }

Revision DryCleaningShop::revision() const noexcept { return revision_; }

bool DryCleaningShop::is_dirty() const noexcept { return dirty_; }

const Employee& DryCleaningShop::authenticate(const std::string& login) const {
    const std::string key = normalize_login(login);
    for (const Employee& employee : employees_) {
        if (employee.login() == key) {
            return employee;
        }
    }
    throw InvalidInputException("login", "сотрудник с таким логином не найден");
}

void DryCleaningShop::require_permission(Id employee_id, Permission permission) const {
    const Employee* employee = employees_.find_by_id(employee_id);
    if (employee == nullptr) {
        throw InvalidInputException("employee_id", "сотрудник не найден");
    }
    if (!employee->has_permission(permission)) {
        throw InvalidInputException("permission", "недостаточно прав для этой операции");
    }
}

void DryCleaningShop::begin_mutation(Id acting_employee, Permission permission) const {
    require_permission(acting_employee, permission);
    if (revision_ == std::numeric_limits<Revision>::max()) {
        throw InvalidInputException("revision", "исчерпан счётчик изменений");
    }
}

void DryCleaningShop::publish() noexcept {
    ++revision_;
    dirty_ = true;
}

void DryCleaningShop::create_initial_data() {
    Repository<Employee> fresh;
    fresh.add(Employee(kInitialAdminName, kInitialAdminLogin, EmployeeRole::Administrator));
    employees_.swap(fresh);
    dirty_ = true;
}

std::size_t DryCleaningShop::administrators_count() const {
    return static_cast<std::size_t>(std::count_if(employees_.begin(), employees_.end(), [](const Employee& employee) {
        return employee.role() == EmployeeRole::Administrator;
    }));
}

bool DryCleaningShop::has_scheduled_batch(Id machine_id) const {
    return std::any_of(batches_.begin(), batches_.end(), [machine_id](const Batch& batch) {
        return batch.machine_id() == machine_id && batch.status() == BatchStatus::Scheduled;
    });
}

Id DryCleaningShop::add_employee(Employee employee, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    employee.validate();
    for (const Employee& other : employees_) {
        if (other.login() == employee.login()) {
            throw InvalidInputException("login", "сотрудник с таким логином уже существует");
        }
    }
    Repository<Employee> copy = employees_;
    const Id id = copy.add(std::move(employee));
    employees_.swap(copy);
    publish();
    return id;
}

void DryCleaningShop::update_employee(const Employee& employee, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    const Employee& current = employees_.get(employee.id());
    Employee updated = current;
    updated.set_name(employee.name());
    updated.set_login(employee.login());
    updated.set_role(employee.role());
    updated.validate();
    for (const Employee& other : employees_) {
        if (other.id() != updated.id() && other.login() == updated.login()) {
            throw InvalidInputException("login", "сотрудник с таким логином уже существует");
        }
    }
    const bool loses_admin = current.role() == EmployeeRole::Administrator &&
                             updated.role() != EmployeeRole::Administrator;
    if (loses_admin && administrators_count() <= 1) {
        throw InvalidInputException("role", "нельзя лишить прав последнего администратора");
    }
    Repository<Employee> copy = employees_;
    replace_by_id(copy, updated);
    employees_.swap(copy);
    publish();
}

Id DryCleaningShop::add_machine(Machine machine, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    machine.set_free_at(TimePoint{});
    Repository<Machine> copy = machines_;
    const Id id = copy.add(std::move(machine));
    machines_.swap(copy);
    publish();
    return id;
}

void DryCleaningShop::update_machine(const Machine& machine, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    const Machine& current = machines_.get(machine.id());
    Machine updated = current;
    updated.set_name(machine.name());
    updated.set_type(machine.type());
    updated.set_capacity_kg(machine.capacity_kg());
    updated.validate();
    const bool changes_setup = updated.type() != current.type() || updated.capacity_kg() != current.capacity_kg();
    if (changes_setup && has_scheduled_batch(current.id())) {
        throw InvalidInputException("machine", "нельзя менять тип или вместимость при запланированных партиях");
    }
    Repository<Machine> copy = machines_;
    replace_by_id(copy, updated);
    machines_.swap(copy);
    publish();
}

Id DryCleaningShop::register_client(std::string name, std::string phone, double discount, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageClients);
    Client client(std::move(name), std::move(phone), discount);
    if (find_client_by_phone(client.phone()) != nullptr) {
        throw InvalidInputException("phone", "клиент с таким телефоном уже существует");
    }
    Repository<Client> copy = clients_;
    const Id id = copy.add(std::move(client));
    clients_.swap(copy);
    publish();
    return id;
}

void DryCleaningShop::update_client(const Client& client, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageClients);
    const Client& current = clients_.get(client.id());
    Client updated = current;
    updated.set_name(client.name());
    updated.set_phone(client.phone());
    updated.set_discount(client.discount());
    updated.validate();
    for (const Client& other : clients_) {
        if (other.id() != updated.id() && other.phone() == updated.phone()) {
            throw InvalidInputException("phone", "клиент с таким телефоном уже существует");
        }
    }
    Repository<Client> copy = clients_;
    replace_by_id(copy, updated);
    clients_.swap(copy);
    publish();
}

void DryCleaningShop::remove_client(Id client_id, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageClients);
    const Client& client = clients_.get(client_id);
    const bool has_orders = client.orders_count() > 0 || std::any_of(orders_.begin(), orders_.end(),
                                                                       [client_id](const Order& order) {
                                                                           return order.client_id() == client_id;
                                                                       });
    if (has_orders) {
        throw InvalidInputException("client", "нельзя удалить клиента, у которого есть заказы");
    }
    Repository<Client> copy = clients_;
    copy.remove(client_id);
    clients_.swap(copy);
    publish();
}

const Client* DryCleaningShop::find_client_by_phone(const std::string& phone) const {
    const std::string key = normalize_phone(phone);
    for (const Client& client : clients_) {
        if (client.phone() == key) {
            return &client;
        }
    }
    return nullptr;
}

std::vector<Client> DryCleaningShop::search_clients(const std::string& query) const {
    if (normalize_text(query).empty()) {
        return clients_.find_if([](const Client&) { return true; });
    }
    return clients_.find_if([&query](const Client& client) { return client.matches(query); });
}

std::vector<Order> DryCleaningShop::client_orders(Id client_id) const {
    const Client& client = clients_.get(client_id);
    std::vector<Order> result;
    for (const Id order_id : client.order_ids()) {
        const Order* order = orders_.find_by_id(order_id);
        if (order != nullptr) {
            result.push_back(*order);
        }
    }
    return result;
}

Id DryCleaningShop::add_material(Material material, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStock);
    Repository<Material> copy = materials_;
    const Id id = copy.add(std::move(material));
    materials_.swap(copy);
    publish();
    return id;
}

void DryCleaningShop::update_material(const Material& material, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStock);
    Material updated = materials_.get(material.id());
    updated.set_name(material.name());
    updated.set_consumption_per_kg(material.consumption_per_kg());
    updated.set_services(material.services());
    updated.validate();
    Repository<Material> copy = materials_;
    replace_by_id(copy, updated);
    materials_.swap(copy);
    publish();
}

void DryCleaningShop::restock_material(Id material_id, double amount, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStock);
    Material updated = materials_.get(material_id);
    updated.add_stock(amount);
    updated.validate();
    Repository<Material> copy = materials_;
    replace_by_id(copy, updated);
    materials_.swap(copy);
    publish();
}

std::vector<Material> DryCleaningShop::find_materials_by_name(const std::string& query) const {
    const std::string key = normalize_text(query);
    return materials_.find_if([&key](const Material& material) {
        return key.empty() || material.name().find(key) != std::string::npos;
    });
}

std::vector<Material> DryCleaningShop::materials_sorted_by_name() const {
    std::vector<Material> result = materials_.find_if([](const Material&) { return true; });
    std::sort(result.begin(), result.end(), [](const Material& left, const Material& right) {
        if (left.name() != right.name()) {
            return left.name() < right.name();
        }
        return left.id() < right.id();
    });
    return result;
}

std::vector<Material> DryCleaningShop::materials_sorted_by_stock() const {
    std::vector<Material> result = materials_.find_if([](const Material&) { return true; });
    std::sort(result.begin(), result.end(), [](const Material& left, const Material& right) {
        if (left.stock() != right.stock()) {
            return left.stock() < right.stock();
        }
        return left.id() < right.id();
    });
    return result;
}
