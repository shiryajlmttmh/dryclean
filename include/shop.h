#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "batch_planner.h"
#include "entity_names.h"
#include "repository.h"
#include "services.h"
#include "storage.h"
#include "types.h"

struct OrderDraft {
    Id client_id = 0;
    std::vector<Item> items;
    ServiceType service_type = ServiceType::DryCleaning;
    bool urgent = false;
};

enum class MaterialOrder { ByName, ByStock };

struct OrderQuote {
    std::vector<double> item_costs;
    std::vector<Hours> item_durations;
    double discount = 0.0;
    double total_cost = 0.0;
    Hours nominal_hours = 0.0;
    TimePoint deadline{};
};

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
    void remove_employee(Id employee_id, Id acting_employee);
    Id add_machine(Machine machine, Id acting_employee);
    void update_machine(const Machine& machine, Id acting_employee);
    void remove_machine(Id machine_id, Id acting_employee);

    Id register_client(std::string name, std::string phone, double discount, Id acting_employee);
    void update_client(const Client& client, Id acting_employee);
    void remove_client(Id client_id, Id acting_employee);
    const Client* find_client_by_phone(const std::string& phone) const;
    std::vector<Client> search_clients(const std::string& query) const;
    std::vector<Order> client_orders(Id client_id) const;

    OrderQuote quote_order(const OrderDraft& draft, TimePoint now) const;
    Id accept_order(const OrderDraft& draft, Id acting_employee, TimePoint now);
    void update_item(const Item& item, Id acting_employee);
    double calculate_item_cost(Id item_id) const;
    double calculate_order_cost(Id order_id) const;
    std::optional<TimePoint> estimate_ready_time(Id order_id) const;
    std::vector<Order> orders_by_status(OrderStatus status) const;
    std::vector<Order> find_overdue_orders(TimePoint now) const;
    void remove_order(Id order_id, Id acting_employee);
    void change_order_status(Id order_id, OrderStatus status, Id acting_employee, TimePoint now);
    void issue_order(Id order_id, Id acting_employee, TimePoint now);

    std::vector<PlanItem> collect_plan_items(TimePoint now) const;
    Plan build_plan(TimePoint now, Id acting_employee) const;
    void validate_plan(const Plan& plan, TimePoint now) const;
    std::vector<Id> confirm_plan(const Plan& plan, Id acting_employee, TimePoint now);
    void complete_batch(Id batch_id, Id acting_employee, TimePoint now);

    void save_all();
    void load_all();

    Id add_material(Material material, Id acting_employee);
    void update_material(const Material& material, Id acting_employee);
    void restock_material(Id material_id, double amount, Id acting_employee);
    void write_off_material(Id material_id, double amount, Id acting_employee);
    void remove_material(Id material_id, Id acting_employee);
    std::vector<Material> find_materials(const std::string& query, MaterialOrder order) const;

private:
    void begin_mutation(Id acting_employee, Permission permission) const;
    void publish() noexcept;
    std::size_t administrators_count() const;
    bool has_employee_records(Id employee_id) const;
    bool has_scheduled_batch(Id machine_id) const;
    class PlanClaims {
    public:
        explicit PlanClaims(const std::vector<PlanItem>& candidates);
        const PlanItem& claim(Id id);
        std::size_t count() const noexcept;

    private:
        std::map<Id, const PlanItem*> by_id_;
        std::set<Id> seen_;
    };

    std::vector<PlannedBatch> verified_batches(const Plan& plan, TimePoint now) const;
    PlannedBatch check_planned_batch(const PlannedBatch& source, PlanClaims& claims, std::map<Id, double>& needed,
                                     Hours& duration) const;
    void accumulate_material_needs(const PlanItem& member, std::map<Id, double>& needed) const;
    void check_material_stock(const std::map<Id, double>& needed) const;
    void check_schedule(std::vector<PlannedBatch>& batches, const std::vector<Hours>& durations,
                        const Plan& plan) const;
    void check_references(const Repository<Client>& clients, const Repository<Item>& items,
                          const Repository<Order>& orders, const Repository<Batch>& batches,
                          const Repository<Employee>& employees, const Repository<Machine>& machines) const;
    void check_batch(const Batch& batch, const Repository<Item>& items, const Repository<Order>& orders,
                     const Repository<Machine>& machines, const Repository<Employee>& employees) const;
    void validate_references() const;
    void ensure_storage();
    bool all_batches_completed(const Order& order, TimePoint& latest_completion) const;

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
    std::unique_ptr<Storage> storage_;
    Revision revision_ = 0;
    bool dirty_ = false;
    bool loaded_ = false;
};
