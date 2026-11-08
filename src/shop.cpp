#include "shop.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

#include "exceptions.h"
#include "normalize.h"
#include "record_io.h"

namespace {

constexpr std::array<ServiceType, 4> kAllServices{ServiceType::DryCleaning, ServiceType::WetCleaning,
                                                  ServiceType::StainRemoval, ServiceType::Leather};

constexpr Hours kHoursPerDay = 24.0;

TimePoint deadline_for(TimePoint start, Hours nominal_hours) {
    return add_hours(start, kHoursPerDay * std::ceil(nominal_hours / kHoursPerDay));
}

constexpr auto kPlanningDelay = std::chrono::minutes(1);

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

OrderQuote DryCleaningShop::make_quote(const OrderDraft& draft, const Client& client, TimePoint now) const {
    if (draft.items.empty()) {
        throw InvalidInputException("items", "заказ не может быть пустым");
    }
    if (draft.items.size() > kMaxNestedElements) {
        throw InvalidInputException("items", "слишком много изделий в заказе");
    }
    const Service& selected = service(draft.service_type);
    OrderQuote quote;
    for (const Item& item : draft.items) {
        if (item.id() != 0 || item.order_id() != 0 || item.batch_id() != 0 || !item.tag().empty()) {
            throw InvalidInputException("items", "изделие черновика не должно иметь служебных полей");
        }
        item.validate();
        if (!selected.is_applicable(item)) {
            throw InvalidInputException("items", "изделие не подходит для выбранной услуги");
        }
        quote.item_costs.push_back(selected.calculate_cost(item, draft.urgent, client.discount()));
        quote.item_durations.push_back(selected.calculate_duration(item, draft.urgent));
    }
    double total = 0.0;
    for (const double cost : quote.item_costs) {
        total += cost;
    }
    quote.total_cost = Service::round_money(total);
    quote.nominal_hours = *std::max_element(quote.item_durations.begin(), quote.item_durations.end());
    quote.deadline = deadline_for(now, quote.nominal_hours);
    return quote;
}

OrderQuote DryCleaningShop::quote_order(const OrderDraft& draft, TimePoint now) const {
    return make_quote(draft, clients_.get(draft.client_id), now);
}

Id DryCleaningShop::accept_order(const OrderDraft& draft, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::AcceptOrders);
    const Client& client = clients_.get(draft.client_id);
    const OrderQuote quote = make_quote(draft, client, now);

    const Id expected_id = orders_.next_id();
    Repository<Item> items = items_;
    Repository<Order> orders = orders_;
    Repository<Client> clients = clients_;

    Order order(draft.client_id, draft.service_type, draft.urgent, acting_employee, now, client.discount());
    std::size_t index = 0;
    for (Item item : draft.items) {
        item.set_order_id(expected_id);
        item.set_tag(Item::make_tag(expected_id, ++index));
        order.add_item_id(items.add(std::move(item)));
    }
    order.set_deadline(quote.deadline);
    order.set_total_cost(quote.total_cost);
    const Id order_id = orders.add(std::move(order));
    if (order_id != expected_id) {
        throw InvalidInputException("order_id", "выданный идентификатор заказа не совпал с ожидаемым");
    }
    clients.get(draft.client_id).add_order_id(order_id);

    items_.swap(items);
    orders_.swap(orders);
    clients_.swap(clients);
    publish();
    return order_id;
}

void DryCleaningShop::update_item(const Item& item, Id acting_employee) {
    begin_mutation(acting_employee, Permission::AcceptOrders);
    const Item& current = items_.get(item.id());
    const Order& order = orders_.get(current.order_id());
    if (item.order_id() != current.order_id() || item.tag() != current.tag() || item.batch_id() != current.batch_id()) {
        throw InvalidInputException("item", "служебные поля изделия менять нельзя");
    }
    const bool any_batched = std::any_of(order.item_ids().begin(), order.item_ids().end(), [this](Id id) {
        return items_.get(id).is_batched();
    });
    if (order.status() != OrderStatus::Accepted || any_batched) {
        throw InvalidInputException("item", "изделие можно менять, только пока заказ не поставлен в обработку");
    }

    Item updated = current;
    updated.set_kind(item.kind());
    updated.set_material(item.material_name(), item.material_group(), item.fabric_group());
    updated.set_color(item.color_name(), item.color_group());
    updated.set_weight_kg(item.weight_kg());
    updated.set_soil_level(item.soil_level());
    updated.set_condition_notes(item.condition_notes());
    while (!updated.defects().empty()) {
        updated.remove_defect(updated.defects().size() - 1);
    }
    for (const Item::Defect& defect : item.defects()) {
        updated.add_defect(defect);
    }
    updated.validate();

    const Service& selected = service(order.service_type());
    if (!selected.is_applicable(updated)) {
        throw InvalidInputException("item", "изделие не подходит для услуги заказа");
    }

    double total = 0.0;
    Hours nominal = 0.0;
    for (const Id id : order.item_ids()) {
        const Item& member = id == updated.id() ? updated : items_.get(id);
        total += selected.calculate_cost(member, order.is_urgent(), order.discount_snapshot());
        nominal = std::max(nominal, selected.calculate_duration(member, order.is_urgent()));
    }

    Order changed = order;
    changed.set_total_cost(Service::round_money(total));
    changed.set_deadline(deadline_for(order.created_at(), nominal));
    changed.set_planned_ready_at(std::nullopt);
    changed.validate();

    Repository<Item> items = items_;
    Repository<Order> orders = orders_;
    replace_by_id(items, updated);
    replace_by_id(orders, changed);
    items_.swap(items);
    orders_.swap(orders);
    publish();
}

double DryCleaningShop::calculate_item_cost(Id item_id) const {
    const Item& item = items_.get(item_id);
    const Order& order = orders_.get(item.order_id());
    return service(order.service_type()).calculate_cost(item, order.is_urgent(), order.discount_snapshot());
}

Hours DryCleaningShop::calculate_item_duration(Id item_id) const {
    const Item& item = items_.get(item_id);
    const Order& order = orders_.get(item.order_id());
    return service(order.service_type()).calculate_duration(item, order.is_urgent());
}

double DryCleaningShop::calculate_order_cost(Id order_id) const {
    const Order& order = orders_.get(order_id);
    double total = 0.0;
    for (const Id item_id : order.item_ids()) {
        total += calculate_item_cost(item_id);
    }
    return Service::round_money(total);
}

std::optional<TimePoint> DryCleaningShop::estimate_ready_time(Id order_id) const {
    return orders_.get(order_id).planned_ready_at();
}

std::vector<Order> DryCleaningShop::orders_by_status(OrderStatus status) const {
    return orders_.find_if([status](const Order& order) { return order.status() == status; });
}

std::vector<Order> DryCleaningShop::find_overdue_orders(TimePoint now) const {
    return orders_.find_if([now](const Order& order) { return order.is_overdue(now); });
}

bool DryCleaningShop::all_batches_completed(const Order& order, TimePoint& latest_completion) const {
    latest_completion = TimePoint{};
    for (const Id item_id : order.item_ids()) {
        const Item* item = items_.find_by_id(item_id);
        if (item == nullptr || !item->is_batched()) {
            return false;
        }
        const Batch* batch = batches_.find_by_id(item->batch_id());
        if (batch == nullptr || batch->status() != BatchStatus::Completed || !batch->completed_at()) {
            return false;
        }
        latest_completion = std::max(latest_completion, *batch->completed_at());
    }
    return true;
}

void DryCleaningShop::change_order_status(Id order_id, OrderStatus status, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::ChangeStatus);
    const Order& order = orders_.get(order_id);
    if (status != OrderStatus::Ready || order.status() != OrderStatus::InProgress) {
        throw InvalidInputException("status", "вручную можно перевести заказ только из «В обработке» в «Готов»");
    }
    TimePoint latest;
    if (!all_batches_completed(order, latest)) {
        throw InvalidInputException("status", "не все партии заказа завершены");
    }
    if (now < latest) {
        throw InvalidInputException("now", "время готовности не может быть раньше завершения партий");
    }
    Order changed = order;
    changed.change_status(OrderStatus::Ready, acting_employee, now);
    changed.validate();
    Repository<Order> orders = orders_;
    replace_by_id(orders, changed);
    orders_.swap(orders);
    publish();
}

void DryCleaningShop::issue_order(Id order_id, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::IssueOrders);
    const Order& order = orders_.get(order_id);
    if (order.status() != OrderStatus::Ready) {
        throw InvalidInputException("status", "выдать можно только готовый заказ");
    }
    if (order.actual_ready_at() && now < *order.actual_ready_at()) {
        throw InvalidInputException("now", "время выдачи не может быть раньше готовности");
    }
    Order changed = order;
    changed.change_status(OrderStatus::Issued, acting_employee, now);
    changed.validate();
    Repository<Order> orders = orders_;
    replace_by_id(orders, changed);
    orders_.swap(orders);
    publish();
}

std::vector<PlanItem> DryCleaningShop::collect_plan_items(TimePoint now) const {
    std::vector<PlanItem> result;
    for (const Item& item : items_) {
        if (item.is_batched()) {
            continue;
        }
        const Order* order = orders_.find_by_id(item.order_id());
        if (order == nullptr || (order->status() != OrderStatus::Accepted && order->status() != OrderStatus::InProgress)) {
            continue;
        }
        PlanItem plan_item;
        plan_item.item_id = item.id();
        plan_item.order_id = order->id();
        plan_item.service_type = order->service_type();
        plan_item.fabric_group = item.fabric_group();
        plan_item.color_group = item.color_group();
        plan_item.weight_kg = item.weight_kg();
        plan_item.duration = service(order->service_type()).calculate_duration(item, order->is_urgent());
        plan_item.deadline = order->deadline();
        plan_item.priority = planner_.compute_priority(plan_item, now);
        result.push_back(plan_item);
    }
    return result;
}

Plan DryCleaningShop::build_plan(TimePoint now, Id acting_employee) const {
    require_permission(acting_employee, Permission::BuildPlan);
    const std::vector<Machine> machines = machines_.find_if([](const Machine&) { return true; });
    const std::vector<Material> materials = materials_.find_if([](const Material&) { return true; });
    Plan plan = planner_.build_plan(collect_plan_items(now), machines, materials, now, revision_);
    for (auto& entry : plan.order_ready_at) {
        if (!entry.second) {
            continue;
        }
        for (const Id item_id : orders_.get(entry.first).item_ids()) {
            const Item& item = items_.get(item_id);
            if (item.is_batched()) {
                entry.second = std::max(*entry.second, batches_.get(item.batch_id()).planned_ready_at());
            }
        }
    }
    return plan;
}

std::vector<PlannedBatch> DryCleaningShop::verified_batches(const Plan& plan, TimePoint now) const {
    if (plan.source_revision != revision_) {
        throw PlanningException("план устарел: данные изменились после его построения");
    }
    const std::vector<PlanItem> candidates = collect_plan_items(now);
    std::map<Id, const PlanItem*> by_id;
    for (const PlanItem& candidate : candidates) {
        by_id[candidate.item_id] = &candidate;
    }
    std::set<Id> seen;
    const auto claim = [&](Id id) {
        if (by_id.find(id) == by_id.end()) {
            throw PlanningException("изделие не может быть запланировано", id);
        }
        if (!seen.insert(id).second) {
            throw PlanningException("изделие встречается в плане более одного раза", id);
        }
    };
    for (const auto& entry : plan.unplaced) {
        claim(entry.first);
    }

    std::vector<PlannedBatch> result;
    std::vector<Hours> durations;
    std::map<Id, double> needed;
    for (const PlannedBatch& source : plan.batches) {
        if (source.item_ids.empty()) {
            throw PlanningException("в плане есть пустая партия");
        }
        const Machine* machine = machines_.find_by_id(source.machine_id);
        if (machine == nullptr) {
            throw PlanningException("в плане указана несуществующая машина");
        }
        std::vector<PlanItem> members;
        double weight = 0.0;
        Hours longest = 0.0;
        for (const Id id : source.item_ids) {
            claim(id);
            const PlanItem& member = *by_id.at(id);
            if (member.service_type != source.service_type || !machine->can_process(source.service_type)) {
                throw PlanningException("услуга изделия не совпадает с услугой партии или машины", id);
            }
            members.push_back(member);
            weight += member.weight_kg;
            longest = std::max(longest, member.duration);
        }
        const ConflictGraph graph = planner_.build_conflict_graph(members);
        for (const auto& node : graph) {
            if (!node.second.empty()) {
                throw PlanningException("в партии оказались несовместимые изделия: " + node.second.begin()->second.front(),
                                        node.first);
            }
        }
        if (weight > machine->capacity_kg() + EPS) {
            throw PlanningException("масса партии превышает вместимость машины");
        }
        for (const PlanItem& member : members) {
            bool has_norm = false;
            for (const Material& material : materials_) {
                if (material.is_used_for(member.service_type) && material.consumption_per_kg() > 0.0) {
                    has_norm = true;
                    needed[material.id()] += material.required_for(member.weight_kg);
                }
            }
            if (!has_norm) {
                throw PlanningException("для услуги не заданы нормы расхода материалов", member.item_id);
            }
        }
        PlannedBatch checked;
        checked.item_ids = source.item_ids;
        checked.machine_id = source.machine_id;
        checked.service_type = source.service_type;
        checked.total_weight_kg = weight;
        checked.fill_ratio = BatchPlanner::fill_ratio(weight, machine->capacity_kg());
        result.push_back(std::move(checked));
        durations.push_back(longest);
    }
    if (seen.size() != candidates.size()) {
        throw PlanningException("план не охватывает все изделия, ожидающие обработки");
    }
    for (const auto& entry : needed) {
        const Material& material = materials_.get(entry.first);
        if (entry.second > material.stock() + EPS) {
            throw OutOfStockException(material.name(), entry.second, material.stock());
        }
    }

    std::map<Id, TimePoint> cursor;
    for (const Machine& machine : machines_) {
        cursor[machine.id()] = std::max(now + kPlanningDelay, machine.free_at());
    }
    for (std::size_t i = 0; i < result.size(); ++i) {
        TimePoint& slot = cursor[result[i].machine_id];
        result[i].planned_start_at = slot;
        result[i].planned_ready_at = add_hours(slot, durations[i]);
        slot = result[i].planned_ready_at;
        if (result[i].planned_start_at != plan.batches[i].planned_start_at ||
            result[i].planned_ready_at != plan.batches[i].planned_ready_at) {
            throw PlanningException("расписание плана устарело, перестройте план");
        }
    }
    return result;
}

void DryCleaningShop::validate_plan(const Plan& plan, TimePoint now) const {
    verified_batches(plan, now);
}

std::vector<Id> DryCleaningShop::confirm_plan(const Plan& plan, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::ConfirmPlan);
    if (plan.batches.empty()) {
        throw PlanningException("в плане нет партий для подтверждения");
    }
    const std::vector<PlannedBatch> verified = verified_batches(plan, now);

    Repository<Batch> batches = batches_;
    Repository<Item> items = items_;
    Repository<Machine> machines = machines_;
    Repository<Material> materials = materials_;
    Repository<Order> orders = orders_;

    std::vector<Id> created;
    std::set<Id> touched_orders;
    std::map<Id, double> consumption;
    for (const PlannedBatch& planned : verified) {
        Batch batch(planned.item_ids, planned.machine_id, planned.service_type, planned.total_weight_kg,
                    planned.fill_ratio, planned.planned_start_at, planned.planned_ready_at, acting_employee);
        const Id batch_id = batches.add(std::move(batch));
        created.push_back(batch_id);
        for (const Id item_id : planned.item_ids) {
            Item& item = items.get(item_id);
            item.set_batch_id(batch_id);
            touched_orders.insert(item.order_id());
            for (const Material& material : materials_) {
                if (material.is_used_for(planned.service_type) && material.consumption_per_kg() > 0.0) {
                    consumption[material.id()] += material.required_for(item.weight_kg());
                }
            }
        }
        machines.get(planned.machine_id).occupy_until(planned.planned_ready_at);
    }
    for (const auto& entry : consumption) {
        materials.get(entry.first).write_off(entry.second);
    }
    for (const Id order_id : touched_orders) {
        Order& order = orders.get(order_id);
        if (order.status() == OrderStatus::Accepted) {
            order.change_status(OrderStatus::InProgress, acting_employee, now);
        }
        std::optional<TimePoint> ready = TimePoint{};
        for (const Id item_id : order.item_ids()) {
            const Item& item = items.get(item_id);
            if (!item.is_batched()) {
                ready = std::nullopt;
                break;
            }
            ready = std::max(*ready, batches.get(item.batch_id()).planned_ready_at());
        }
        order.set_planned_ready_at(ready);
        order.validate();
    }

    batches_.swap(batches);
    items_.swap(items);
    machines_.swap(machines);
    materials_.swap(materials);
    orders_.swap(orders);
    publish();
    return created;
}

void DryCleaningShop::complete_batch(Id batch_id, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::ChangeStatus);
    Batch updated = batches_.get(batch_id);
    updated.complete(acting_employee, now);
    updated.validate();
    Repository<Batch> batches = batches_;
    replace_by_id(batches, updated);
    batches_.swap(batches);
    publish();
}
