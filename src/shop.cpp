#include "shop.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

#include "exceptions.h"
#include "normalize.h"
#include "record_io.h"

namespace {

constexpr std::size_t kMinAdministrators = 1;
constexpr std::array kAllServices{ServiceType::DryCleaning, ServiceType::WetCleaning,
                                                  ServiceType::StainRemoval, ServiceType::Leather};

constexpr Hours kHoursPerDay = 24.0;

TimePoint deadline_for(TimePoint start, Hours nominal_hours) {
    return add_hours(start, kHoursPerDay * std::ceil(nominal_hours / kHoursPerDay));
}

constexpr auto kPlanningDelay = std::chrono::minutes(1);

constexpr const char* kManifestName = "manifest.txt";
constexpr const char* kManifestType = "manifest";
constexpr const char* kFileExtension = ".txt";
constexpr std::size_t kFileCount = 7;

void check_employees(const Repository<Employee>& employees) {
    std::set<std::string> logins;
    for (const Employee& employee : employees) {
        if (!logins.insert(employee.login()).second) {
            throw IntegrityException("повторяющийся логин сотрудника");
        }
    }
}

void check_clients(const Repository<Client>& clients, const Repository<Order>& orders) {
    std::set<std::string> phones;
    for (const Client& client : clients) {
        if (!phones.insert(client.phone()).second) {
            throw IntegrityException("повторяющийся телефон клиента");
        }
        for (const Id order_id : client.order_ids()) {
            const Order* order = orders.find_by_id(order_id);
            if (order == nullptr || order->client_id() != client.id()) {
                throw IntegrityException("заказ клиента отсутствует или принадлежит другому клиенту");
            }
        }
    }
}

struct OrderProgress {
    std::size_t batched = 0;
    bool all_completed = true;
    TimePoint latest_completion{};
};

OrderProgress check_order_items(const Order& order, const Repository<Item>& items,
                                const Repository<Batch>& batches, std::set<std::string>& tags) {
    OrderProgress progress;
    std::size_t position = 0;
    for (const Id item_id : order.item_ids()) {
        ++position;
        const Item* item = items.find_by_id(item_id);
        if (item == nullptr || item->order_id() != order.id()) {
            throw IntegrityException("изделие заказа отсутствует или принадлежит другому заказу");
        }
        if (item->tag() != Item::make_tag(order.id(), position)) {
            throw IntegrityException("метка изделия не соответствует заказу и номеру");
        }
        if (!tags.insert(item->tag()).second) {
            throw IntegrityException("повторяющаяся метка изделия");
        }
        if (!item->is_batched()) {
            progress.all_completed = false;
            continue;
        }
        ++progress.batched;
        const Batch* batch = batches.find_by_id(item->batch_id());
        if (batch == nullptr || !batch->contains_item(item->id())) {
            throw IntegrityException("партия изделия отсутствует или не содержит изделие");
        }
        if (batch->status() != BatchStatus::Completed || !batch->completed_at()) {
            progress.all_completed = false;
        } else {
            progress.latest_completion = std::max(progress.latest_completion, *batch->completed_at());
        }
    }
    return progress;
}

void check_order_status(const Order& order, const OrderProgress& progress) {
    const OrderStatus status = order.status();
    if (status == OrderStatus::Accepted && progress.batched != 0) {
        throw IntegrityException("у принятого заказа не должно быть изделий в партиях");
    }
    if (status != OrderStatus::Accepted && progress.batched == 0) {
        throw IntegrityException("у заказа в работе нет изделий в партиях");
    }
    if ((status == OrderStatus::Ready || status == OrderStatus::Issued) && !progress.all_completed) {
        throw IntegrityException("готовый или выданный заказ содержит незавершённые партии");
    }
    if (order.actual_ready_at() && *order.actual_ready_at() < progress.latest_completion) {
        throw IntegrityException("время готовности заказа раньше завершения партий");
    }
}

void check_orders(const Repository<Client>& clients, const Repository<Item>& items,
                  const Repository<Order>& orders, const Repository<Batch>& batches,
                  const Repository<Employee>& employees) {
    std::set<std::string> tags;
    for (const Order& order : orders) {
        const Client* client = clients.find_by_id(order.client_id());
        if (client == nullptr) {
            throw IntegrityException("заказ ссылается на несуществующего клиента");
        }
        const auto& client_orders = client->order_ids();
        if (std::find(client_orders.begin(), client_orders.end(), order.id()) == client_orders.end()) {
            throw IntegrityException("заказ не указан в списке заказов клиента");
        }
        if (!employees.contains(order.employee_id())) {
            throw IntegrityException("заказ ссылается на несуществующего сотрудника");
        }
        for (const StatusRecord& record : order.history()) {
            if (!employees.contains(record.employee_id())) {
                throw IntegrityException("журнал заказа ссылается на несуществующего сотрудника");
            }
        }
        check_order_status(order, check_order_items(order, items, batches, tags));
    }
}

void check_items(const Repository<Item>& items, const Repository<Order>& orders,
                 const Repository<Batch>& batches) {
    for (const Item& item : items) {
        const Order* order = orders.find_by_id(item.order_id());
        if (order == nullptr || !order->contains_item(item.id())) {
            throw IntegrityException("изделие не входит в свой заказ");
        }
        if (!item.is_batched()) {
            continue;
        }
        const Batch* batch = batches.find_by_id(item.batch_id());
        if (batch == nullptr || !batch->contains_item(item.id())) {
            throw IntegrityException("изделие ссылается на партию, которая его не содержит");
        }
    }
}

void check_machine_queues(const Repository<Batch>& batches, const Repository<Machine>& machines) {
    std::map<Id, std::vector<std::pair<TimePoint, TimePoint>>> queues;
    std::map<Id, TimePoint> latest;
    for (const Batch& batch : batches) {
        queues[batch.machine_id()].emplace_back(batch.planned_start_at(), batch.planned_ready_at());
        TimePoint& slot = latest[batch.machine_id()];
        slot = std::max(slot, batch.planned_ready_at());
    }
    for (auto& queue : queues) {
        std::sort(queue.second.begin(), queue.second.end());
        for (std::size_t i = 1; i < queue.second.size(); ++i) {
            if (queue.second[i].first < queue.second[i - 1].second) {
                throw IntegrityException("партии одной машины пересекаются по времени");
            }
        }
    }
    for (const Machine& machine : machines) {
        const auto found = latest.find(machine.id());
        if (found != latest.end() && machine.free_at() < found->second) {
            throw IntegrityException("машина освобождается раньше окончания своих партий");
        }
    }
}

template <class T>
std::string file_name_for() {
    return std::string(EntityName<T>::value) + kFileExtension;
}

template <class T>
void save_file(const Repository<T>& repository, const std::filesystem::path& directory) {
    repository.save(directory / file_name_for<T>());
}

void write_manifest(const std::filesystem::path& path, Revision revision,
                    const std::vector<std::pair<std::string, std::size_t>>& files) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw StorageException(path, "не удалось открыть файл для записи");
    }
    out.imbue(std::locale::classic());
    RecordWriter header;
    header.put_token(kFormatMagic);
    header.put_count(kFormatVersion);
    header.put_token(kManifestType);
    header.put_revision(revision);
    header.put_count(files.size());
    out << header.str() << '\n';
    for (const auto& file : files) {
        RecordWriter line;
        line.put_token(file.first);
        line.put_count(file.second);
        out << line.str() << '\n';
    }
    out.flush();
    if (!out) {
        throw StorageException(path, "ошибка записи в файл");
    }
    out.close();
    if (out.fail()) {
        throw StorageException(path, "не удалось закрыть файл после записи");
    }
}

std::map<std::string, std::size_t> read_manifest(const std::filesystem::path& path, Revision& revision) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw FileFormatException(kManifestName, kWholeFile, "файл манифеста отсутствует");
    }
    std::map<std::string, std::size_t> counts;
    std::string text;
    std::size_t line_number = 1;
    try {
        if (!std::getline(in, text)) {
            throw InvalidInputException("manifest", "манифест пуст");
        }
        RecordReader header(text);
        if (header.get_token() != kFormatMagic || header.get_count() != kFormatVersion ||
            header.get_token() != kManifestType) {
            throw InvalidInputException("manifest", "неизвестный формат манифеста");
        }
        revision = header.get_revision();
        const std::size_t declared = header.get_count();
        header.finish();
        if (declared != kFileCount) {
            throw InvalidInputException("manifest", "неверное число файлов в манифесте");
        }
        for (std::size_t i = 0; i < declared; ++i) {
            ++line_number;
            if (!std::getline(in, text)) {
                throw InvalidInputException("manifest", "манифест оборван");
            }
            RecordReader reader(text);
            const std::string name = reader.get_token();
            const std::size_t count = reader.get_count(kMaxRecords);
            reader.finish();
            if (!counts.emplace(name, count).second) {
                throw InvalidInputException("manifest", "файл указан в манифесте дважды");
            }
        }
        while (std::getline(in, text)) {
            if (text.find_first_not_of(" \t\r\n") != std::string::npos) {
                ++line_number;
                throw InvalidInputException("manifest", "после последней записи есть лишние данные");
            }
        }
    } catch (const InvalidInputException& error) {
        throw FileFormatException(kManifestName, line_number, error.what());
    }
    return counts;
}

template <class T>
void load_file(Repository<T>& repository, const std::filesystem::path& directory,
               const std::map<std::string, std::size_t>& counts) {
    const std::string name = file_name_for<T>();
    std::error_code ec;
    if (!std::filesystem::exists(directory / name, ec)) {
        throw FileFormatException(name, kWholeFile, "файл снимка отсутствует");
    }
    const auto expected = counts.find(name);
    if (expected == counts.end()) {
        throw FileFormatException(kManifestName, kWholeFile, "в манифесте нет файла " + name);
    }
    repository.load(directory / name);
    if (repository.size() != expected->second) {
        throw FileFormatException(name, kWholeFile, "число записей не совпадает с манифестом");
    }
}

constexpr const char* kInitialAdminName = "Администратор";
constexpr const char* kInitialAdminLogin = "admin";

void advance_orders(Repository<Order>& orders, const Repository<Item>& items, const Repository<Batch>& batches,
                    const std::set<Id>& touched_orders, Id acting_employee, TimePoint now) {
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
}

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

void DryCleaningShop::ensure_storage() {
    if (!storage_) {
        storage_ = std::make_unique<Storage>(data_dir_);
    }
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
    if (loses_admin && administrators_count() <= kMinAdministrators) {
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
    const std::string key = to_lower_text(normalize_text(query));
    return materials_.find_if([&key](const Material& material) {
        return key.empty() || to_lower_text(material.name()).find(key) != std::string::npos;
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

OrderQuote DryCleaningShop::quote_order(const OrderDraft& draft, TimePoint now) const {
    const Client& client = clients_.get(draft.client_id);
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

Id DryCleaningShop::accept_order(const OrderDraft& draft, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::AcceptOrders);
    const Client& client = clients_.get(draft.client_id);
    const OrderQuote quote = quote_order(draft, now);

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
        plan_item.material_group = item.material_group();
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

DryCleaningShop::PlanClaims::PlanClaims(const std::vector<PlanItem>& candidates) {
    for (const PlanItem& candidate : candidates) {
        by_id_[candidate.item_id] = &candidate;
    }
}

const PlanItem& DryCleaningShop::PlanClaims::claim(Id id) {
    const auto found = by_id_.find(id);
    if (found == by_id_.end()) {
        throw PlanningException("изделие не может быть запланировано", id);
    }
    if (!seen_.insert(id).second) {
        throw PlanningException("изделие встречается в плане более одного раза", id);
    }
    return *found->second;
}

std::size_t DryCleaningShop::PlanClaims::count() const noexcept { return seen_.size(); }

void DryCleaningShop::accumulate_material_needs(const PlanItem& member, std::map<Id, double>& needed) const {
    bool has_material = false;
    for (const Material& material : materials_) {
        if (material.is_used_for(member.service_type)) {
            has_material = true;
            needed[material.id()] += material.required_for(member.weight_kg);
        }
    }
    if (!has_material) {
        throw PlanningException("для услуги не назначено ни одного расходного материала", member.item_id);
    }
}

PlannedBatch DryCleaningShop::check_planned_batch(const PlannedBatch& source, PlanClaims& claims,
                                                  std::map<Id, double>& needed, Hours& duration) const {
    if (source.item_ids.empty()) {
        throw PlanningException("в плане есть пустая партия");
    }
    const Machine* machine = machines_.find_by_id(source.machine_id);
    if (machine == nullptr) {
        throw PlanningException("в плане указана несуществующая машина");
    }
    std::vector<PlanItem> members;
    double weight = 0.0;
    duration = 0.0;
    for (const Id id : source.item_ids) {
        const PlanItem& member = claims.claim(id);
        if (member.service_type != source.service_type || !machine->can_process(source.service_type)) {
            throw PlanningException("услуга изделия не совпадает с услугой партии или машины", id);
        }
        members.push_back(member);
        weight += member.weight_kg;
        duration = std::max(duration, member.duration);
    }
    for (const auto& node : planner_.build_conflict_graph(members)) {
        if (!node.second.empty()) {
            throw PlanningException("в партии оказались несовместимые изделия: " + node.second.begin()->second.front(),
                                    node.first);
        }
    }
    if (weight > machine->capacity_kg() + EPS) {
        throw PlanningException("масса партии превышает вместимость машины");
    }
    const double fill = BatchPlanner::fill_ratio(weight, machine->capacity_kg());
    if (std::abs(source.total_weight_kg - weight) > EPS || std::abs(source.fill_ratio - fill) > EPS) {
        throw PlanningException("масса или загрузка партии в плане не совпадают с данными изделий");
    }
    for (const PlanItem& member : members) {
        accumulate_material_needs(member, needed);
    }
    PlannedBatch checked;
    checked.item_ids = source.item_ids;
    checked.machine_id = source.machine_id;
    checked.service_type = source.service_type;
    checked.total_weight_kg = weight;
    checked.fill_ratio = fill;
    return checked;
}

void DryCleaningShop::check_material_stock(const std::map<Id, double>& needed) const {
    for (const auto& entry : needed) {
        const Material& material = materials_.get(entry.first);
        if (entry.second > material.stock() + EPS) {
            throw OutOfStockException(material.name(), entry.second, material.stock());
        }
    }
}

void DryCleaningShop::check_schedule(std::vector<PlannedBatch>& batches, const std::vector<Hours>& durations,
                                     const Plan& plan, TimePoint now) const {
    std::map<Id, TimePoint> cursor;
    for (const Machine& machine : machines_) {
        cursor[machine.id()] = std::max(now + kPlanningDelay, machine.free_at());
    }
    for (std::size_t i = 0; i < batches.size(); ++i) {
        TimePoint& slot = cursor[batches[i].machine_id];
        batches[i].planned_start_at = slot;
        batches[i].planned_ready_at = add_hours(slot, durations[i]);
        slot = batches[i].planned_ready_at;
        if (batches[i].planned_start_at != plan.batches[i].planned_start_at ||
            batches[i].planned_ready_at != plan.batches[i].planned_ready_at) {
            throw PlanningException("расписание плана устарело, перестройте план");
        }
    }
}

std::vector<PlannedBatch> DryCleaningShop::verified_batches(const Plan& plan, TimePoint now) const {
    if (plan.source_revision != revision_) {
        throw PlanningException("план устарел: данные изменились после его построения");
    }
    const std::vector<PlanItem> candidates = collect_plan_items(now);
    PlanClaims claims(candidates);
    for (const auto& entry : plan.unplaced) {
        claims.claim(entry.first);
    }
    std::vector<PlannedBatch> result;
    std::vector<Hours> durations;
    std::map<Id, double> needed;
    for (const PlannedBatch& source : plan.batches) {
        Hours duration = 0.0;
        result.push_back(check_planned_batch(source, claims, needed, duration));
        durations.push_back(duration);
    }
    if (claims.count() != candidates.size()) {
        throw PlanningException("план не охватывает все изделия, ожидающие обработки");
    }
    check_material_stock(needed);
    check_schedule(result, durations, plan, now);
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
                if (material.is_used_for(planned.service_type)) {
                    consumption[material.id()] += material.required_for(item.weight_kg());
                }
            }
        }
        machines.get(planned.machine_id).occupy_until(planned.planned_ready_at);
    }
    for (const auto& entry : consumption) {
        materials.get(entry.first).write_off(entry.second);
    }
    advance_orders(orders, items, batches, touched_orders, acting_employee, now);

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

void DryCleaningShop::validate_references() const {
    check_references(clients_, items_, orders_, batches_, employees_, machines_);
}

void DryCleaningShop::check_batch(const Batch& batch, const Repository<Item>& items,
                                  const Repository<Order>& orders, const Repository<Machine>& machines,
                                  const Repository<Employee>& employees) const {
    const Machine* machine = machines.find_by_id(batch.machine_id());
    if (machine == nullptr) {
        throw IntegrityException("партия ссылается на несуществующую машину");
    }
    if (!employees.contains(batch.created_by()) ||
        (batch.completed_by() && !employees.contains(*batch.completed_by()))) {
        throw IntegrityException("партия ссылается на несуществующего сотрудника");
    }
    if (!machine->can_process(batch.service_type())) {
        throw IntegrityException("машина партии не подходит для её услуги");
    }
    double weight = 0.0;
    Hours longest = 0.0;
    for (const Id item_id : batch.item_ids()) {
        const Item* item = items.find_by_id(item_id);
        if (item == nullptr || item->batch_id() != batch.id()) {
            throw IntegrityException("изделие партии отсутствует или указывает на другую партию");
        }
        const Order& order = orders.get(item->order_id());
        if (order.service_type() != batch.service_type()) {
            throw IntegrityException("услуга заказа не совпадает с услугой партии");
        }
        weight += item->weight_kg();
        longest = std::max(longest, service(order.service_type()).calculate_duration(*item, order.is_urgent()));
    }
    if (std::abs(weight - batch.total_weight_kg()) > EPS) {
        throw IntegrityException("масса партии не совпадает с массой изделий");
    }
    if (weight > machine->capacity_kg() + EPS) {
        throw IntegrityException("масса партии превышает вместимость машины");
    }
    if (std::abs(batch.fill_ratio() - BatchPlanner::fill_ratio(weight, machine->capacity_kg())) > EPS) {
        throw IntegrityException("заполнение партии не совпадает с расчётным");
    }
    if (batch.planned_ready_at() != add_hours(batch.planned_start_at(), longest)) {
        throw IntegrityException("время окончания партии не совпадает с длительностью изделий");
    }
    if (batch.completed_at() && *batch.completed_at() < batch.planned_ready_at()) {
        throw IntegrityException("партия завершена раньше планового окончания");
    }
}

void DryCleaningShop::check_references(const Repository<Client>& clients, const Repository<Item>& items,
                                       const Repository<Order>& orders, const Repository<Batch>& batches,
                                       const Repository<Employee>& employees,
                                       const Repository<Machine>& machines) const {
    check_employees(employees);
    check_clients(clients, orders);
    check_orders(clients, items, orders, batches, employees);
    check_items(items, orders, batches);
    for (const Batch& batch : batches) {
        check_batch(batch, items, orders, machines, employees);
    }
    check_machine_queues(batches, machines);
}

void DryCleaningShop::save_all() {
    if (!loaded_) {
        throw InvalidInputException("state", "данные не загружены, сохранение невозможно");
    }
    validate_references();
    ensure_storage();
    const Generation generation = storage_->begin_generation();
    const std::filesystem::path directory = storage_->generation_path(generation);
    try {
        save_file(clients_, directory);
        save_file(items_, directory);
        save_file(orders_, directory);
        save_file(batches_, directory);
        save_file(materials_, directory);
        save_file(employees_, directory);
        save_file(machines_, directory);
        write_manifest(directory / kManifestName, revision_,
                       {{file_name_for<Client>(), clients_.size()},
                        {file_name_for<Item>(), items_.size()},
                        {file_name_for<Order>(), orders_.size()},
                        {file_name_for<Batch>(), batches_.size()},
                        {file_name_for<Material>(), materials_.size()},
                        {file_name_for<Employee>(), employees_.size()},
                        {file_name_for<Machine>(), machines_.size()}});
        storage_->commit_generation(generation);
    } catch (...) {
        storage_->abort_generation(generation);
        throw;
    }
    dirty_ = false;
}

void DryCleaningShop::load_all() {
    if (dirty_) {
        throw InvalidInputException("state", "есть несохранённые изменения, загрузка запрещена");
    }
    ensure_storage();
    const std::optional<Generation> current = storage_->current_generation();

    Repository<Client> clients;
    Repository<Item> items;
    Repository<Order> orders;
    Repository<Batch> batches;
    Repository<Material> materials;
    Repository<Employee> employees;
    Repository<Machine> machines;
    Revision stored_revision = 0;
    bool fresh = false;

    if (!current) {
        if (storage_->has_data()) {
            throw FileFormatException("CURRENT", kWholeFile, "указатель CURRENT отсутствует, но в каталоге есть данные");
        }
        employees.add(Employee(kInitialAdminName, kInitialAdminLogin, EmployeeRole::Administrator));
        fresh = true;
    } else {
        const std::filesystem::path directory = storage_->generation_path(*current);
        const auto counts = read_manifest(directory / kManifestName, stored_revision);
        load_file(clients, directory, counts);
        load_file(items, directory, counts);
        load_file(orders, directory, counts);
        load_file(batches, directory, counts);
        load_file(materials, directory, counts);
        load_file(employees, directory, counts);
        load_file(machines, directory, counts);
    }

    check_references(clients, items, orders, batches, employees, machines);

    const Revision base = std::max(revision_, stored_revision);
    if (loaded_ && base == std::numeric_limits<Revision>::max()) {
        throw InvalidInputException("revision", "исчерпан счётчик изменений");
    }
    clients_.swap(clients);
    items_.swap(items);
    orders_.swap(orders);
    batches_.swap(batches);
    materials_.swap(materials);
    employees_.swap(employees);
    machines_.swap(machines);
    revision_ = loaded_ ? base + 1 : base;
    dirty_ = fresh;
    loaded_ = true;
}
