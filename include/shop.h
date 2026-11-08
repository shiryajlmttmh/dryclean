#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "batch_planner.h"
#include "entity_names.h"
#include "repository.h"
#include "services.h"
#include "types.h"

class DryCleaningShop {
public:
    explicit DryCleaningShop(std::filesystem::path data_dir);

    DryCleaningShop(const DryCleaningShop&) = delete;
    DryCleaningShop& operator=(const DryCleaningShop&) = delete;

    const Repository<Client>& clients() const noexcept;
    const Repository<Item>& items() const noexcept;
    const Repository<Order>& orders() const noexcept;
    const Repository<Batch>& batches() const noexcept;
    const Repository<Material>& materials() const noexcept;
    const Repository<Employee>& employees() const noexcept;
    const Repository<Machine>& machines() const noexcept;
    const Service& service(ServiceType type) const;
    const std::filesystem::path& data_dir() const noexcept;

    Revision revision() const noexcept;
    bool is_dirty() const noexcept;

    const Employee& authenticate(const std::string& login) const;
    void require_permission(Id employee_id, Permission permission) const;

    Id add_employee(Employee employee, Id acting_employee);
    void update_employee(const Employee& employee, Id acting_employee);
    Id add_machine(Machine machine, Id acting_employee);
    void update_machine(const Machine& machine, Id acting_employee);

    Id register_client(std::string name, std::string phone, double discount, Id acting_employee);
    void update_client(const Client& client, Id acting_employee);
    void remove_client(Id client_id, Id acting_employee);
    const Client* find_client_by_phone(const std::string& phone) const;
    std::vector<Client> search_clients(const std::string& query) const;
    std::vector<Order> client_orders(Id client_id) const;

    Id add_material(Material material, Id acting_employee);
    void update_material(const Material& material, Id acting_employee);
    void restock_material(Id material_id, double amount, Id acting_employee);
    std::vector<Material> find_materials_by_name(const std::string& query) const;
    std::vector<Material> materials_sorted_by_name() const;
    std::vector<Material> materials_sorted_by_stock() const;

private:
    void begin_mutation(Id acting_employee, Permission permission) const;
    void publish() noexcept;
    void create_initial_data();
    std::size_t administrators_count() const;
    bool has_scheduled_batch(Id machine_id) const;

    Repository<Client> clients_;
    Repository<Item> items_;
    Repository<Order> orders_;
    Repository<Batch> batches_;
    Repository<Material> materials_;
    Repository<Employee> employees_;
    Repository<Machine> machines_;
    std::map<ServiceType, std::unique_ptr<Service>> services_;
    BatchPlanner planner_;
    std::filesystem::path data_dir_;
    Revision revision_ = 0;
    bool dirty_ = false;
};
