#include "shop.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <sstream>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

#include "exceptions.h"
#include "display.h"
#include "normalize.h"
#include "record_io.h"

namespace {

constexpr std::size_t kMinAdministrators = 1;
constexpr std::array kAllServices{ServiceType::DryCleaning, ServiceType::WetCleaning,
                                                  ServiceType::StainRemoval, ServiceType::Leather};

constexpr Hours kHoursPerDay = 24.0;
constexpr Hours kStartReserveHours = 1.0;

TimePoint deadline_for(TimePoint start, Hours nominal_hours) {
    return add_hours(start, kHoursPerDay * std::ceil((nominal_hours + kStartReserveHours) / kHoursPerDay));
}

constexpr auto kPlanLifetime = std::chrono::minutes(30);

constexpr const char* kManifestName = "manifest.txt";
constexpr const char* kManifestType = "manifest";
constexpr const char* kFileExtension = ".txt";
constexpr std::size_t kFileCount = 8;

void remember(Repository<Suggestion>& suggestions, SuggestionKind kind, const std::string& value) {
    const auto key = to_lower_text(value);
    const auto existing = std::ranges::find_if(suggestions, [&](const Suggestion& suggestion) {
        return suggestion.kind() == kind && to_lower_text(suggestion.value()) == key;
    });
    if (existing == suggestions.end()) suggestions.add(Suggestion(kind, value));
}

void remember_material(Repository<Suggestion>& suggestions, const Material& material) {
    remember(suggestions, SuggestionKind::MaterialName, material.name());
    remember(suggestions, SuggestionKind::Unit, material.unit());
}

void remember_item(Repository<Suggestion>& suggestions, const Item& item) {
    remember(suggestions, SuggestionKind::ItemName, item.kind());
    for (const auto& defect : item.defects()) {
        remember(suggestions, SuggestionKind::DefectName, defect.name);
    }
}

void check_suggestions(const Repository<Suggestion>& suggestions) {
    std::set<std::pair<SuggestionKind, std::string>> keys;
    for (const Suggestion& suggestion : suggestions) {
        if (!keys.emplace(suggestion.kind(), to_lower_text(suggestion.value())).second) {
            throw IntegrityException("повторяющаяся подсказка справочника");
        }
    }
}

void check_employees(const Repository<Employee>& employees) {
    if (std::none_of(employees.begin(), employees.end(), [](const Employee& employee) {
            return employee.role() == EmployeeRole::Administrator;
        })) {
        throw IntegrityException("в хранилище должна быть учётная запись администратора");
    }
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
            if (!item->completed_batches().empty()) ++progress.batched;
            progress.all_completed = false;
            continue;
        }
        ++progress.batched;
        const Batch* batch = batches.find_by_id(item->batch_id());
        if (batch == nullptr || !batch->contains_item(item->id())) {
            throw IntegrityException("партия изделия отсутствует или не содержит изделие");
        }
        if (!item->services_completed() || batch->status() != BatchStatus::Completed || !batch->completed_at()) {
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
        TimePoint previous_completion{};
        for (Id batch_id : item.completed_batches()) {
            const Batch* completed = batches.find_by_id(batch_id);
            if (completed == nullptr || !completed->contains_item(item.id()) || !completed->completed_at() ||
                completed->status() != BatchStatus::Completed ||
                completed->effective_start_at() < previous_completion) {
                throw IntegrityException("нарушен порядок завершённых услуг изделия");
            }
            previous_completion = *completed->completed_at();
        }
        if (!item.is_batched()) {
            continue;
        }
        const Batch* batch = batches.find_by_id(item.batch_id());
        if (batch == nullptr || !batch->contains_item(item.id())) {
            throw IntegrityException("изделие ссылается на партию, которая его не содержит");
        }
        if (!item.services_completed() && batch->effective_start_at() < previous_completion) {
            throw IntegrityException("услуга изделия начинается раньше завершения предыдущей");
        }
    }
}

std::map<Id, std::vector<const Batch*>> machine_queues(const Repository<Batch>& batches) {
    std::map<Id, std::vector<const Batch*>> queues;
    for (const Batch& batch : batches) {
        queues[batch.machine_id()].push_back(&batch);
    }
    for (auto& [id, queue] : queues) std::ranges::sort(queue, {}, &Batch::id);
    return queues;
}

void check_machine_queues(const Repository<Batch>& batches, const Repository<Machine>& machines) {
    for (const auto& [machine_id, queue] : machine_queues(batches)) {
        TimePoint end{};
        bool has_unfinished = false;
        for (const Batch* batch : queue) {
            if (batch->started_at() && has_unfinished) {
                throw IntegrityException("партия запущена до подтверждения предыдущей партии машины");
            }
            if (batch->effective_start_at() < end) {
                throw IntegrityException("партии одной машины пересекаются по времени");
            }
            has_unfinished = has_unfinished || batch->status() != BatchStatus::Completed;
            end = batch->occupied_until();
        }
        if (machines.get(machine_id).free_at() < end) {
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

using FinishingHours = std::function<Hours(const Item&, const Order&)>;

void advance_orders(Repository<Order>& orders, const Repository<Item>& items, const Repository<Batch>& batches,
                    const std::set<Id>& touched_orders, const FinishingHours& finishing, Id acting_employee,
                    TimePoint now) {
    for (const Id order_id : touched_orders) {
        Order& order = orders.get(order_id);
        if (order.status() == OrderStatus::Accepted) {
            order.change_status(OrderStatus::InProgress, acting_employee, now);
        }
        std::optional<TimePoint> ready = TimePoint{};
        for (const Id item_id : order.item_ids()) {
            const Item& item = items.get(item_id);
            if (!item.is_batched() || !item.is_last_service()) {
                ready = std::nullopt;
                break;
            }
            ready = std::max(*ready, add_hours(batches.get(item.batch_id()).occupied_until(), finishing(item, order)));
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
const Repository<Suggestion>& DryCleaningShop::suggestions() const noexcept { return suggestions_; }

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

const Employee& DryCleaningShop::authenticate(const std::string& login, const std::string& password) const {
    const std::string key = normalize_login(login);
    for (const Employee& employee : employees_) {
        if (employee.login() == key && employee.check_password(password)) {
            return employee;
        }
    }
    throw InvalidInputException("login", "неверный логин или пароль");
}

bool DryCleaningShop::needs_password_setup() const noexcept { return loaded_ && employees_.empty(); }

Id DryCleaningShop::initialize_admin_password(const std::string& password) {
    if (!needs_password_setup()) {
        throw InvalidInputException("password", "первичная настройка пароля недоступна");
    }
    if (revision_ == std::numeric_limits<Revision>::max()) {
        throw InvalidInputException("revision", "исчерпан счётчик изменений");
    }
    Employee admin(kInitialAdminName, kInitialAdminLogin, EmployeeRole::Administrator);
    admin.set_password(password);
    Repository<Employee> updated = employees_;
    const Id id = updated.add(std::move(admin));
    const Revision previous_revision = revision_;
    employees_.swap(updated);
    publish();
    try {
        save_all();
    } catch (...) {
        employees_.swap(updated);
        revision_ = previous_revision;
        dirty_ = false;
        throw;
    }
    return id;
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

bool DryCleaningShop::has_unfinished_batch(Id machine_id) const {
    return std::any_of(batches_.begin(), batches_.end(), [machine_id](const Batch& batch) {
        return batch.machine_id() == machine_id && batch.status() != BatchStatus::Completed;
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
    auto suggestions = suggestions_;
    remember(suggestions, SuggestionKind::EmployeeName, copy.get(id).name());
    employees_.swap(copy);
    suggestions_.swap(suggestions);
    publish();
    return id;
}

void DryCleaningShop::update_employee(const Employee& employee, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    const Employee& current = employees_.get(employee.id());
    employee.validate();
    for (const Employee& other : employees_) {
        if (other.id() != employee.id() && other.login() == employee.login()) {
            throw InvalidInputException("login", "сотрудник с таким логином уже существует");
        }
    }
    const bool loses_admin = current.role() == EmployeeRole::Administrator &&
                             employee.role() != EmployeeRole::Administrator;
    if (loses_admin && administrators_count() <= kMinAdministrators) {
        throw InvalidInputException("role", "нельзя лишить прав последнего администратора");
    }
    Repository<Employee> copy = employees_;
    replace_by_id(copy, employee);
    auto suggestions = suggestions_;
    remember(suggestions, SuggestionKind::EmployeeName, employee.name());
    employees_.swap(copy);
    suggestions_.swap(suggestions);
    publish();
}

Id DryCleaningShop::add_machine(Machine machine, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    machine.set_free_at(TimePoint{});
    Repository<Machine> copy = machines_;
    const Id id = copy.add(std::move(machine));
    auto suggestions = suggestions_;
    remember(suggestions, SuggestionKind::MachineName, copy.get(id).name());
    machines_.swap(copy);
    suggestions_.swap(suggestions);
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
    updated.set_maintenance_interval(machine.maintenance_interval());
    updated.validate();
    const bool changes_setup = updated.type() != current.type() || updated.capacity_kg() != current.capacity_kg();
    if (changes_setup && has_unfinished_batch(current.id())) {
        throw InvalidInputException("machine", "сначала завершите все партии машины, чтобы изменить тип или вместимость");
    }
    Repository<Machine> copy = machines_;
    replace_by_id(copy, updated);
    auto suggestions = suggestions_;
    remember(suggestions, SuggestionKind::MachineName, updated.name());
    machines_.swap(copy);
    suggestions_.swap(suggestions);
    publish();
}

bool DryCleaningShop::can_service_machine(Id machine_id, TimePoint now) const {
    return machines_.get(machine_id).is_free_at(now) && !has_unfinished_batch(machine_id);
}

void DryCleaningShop::service_machine(Id machine_id, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    if (!can_service_machine(machine_id, now)) {
        throw InvalidInputException("maintenance", "сначала завершите все партии машины");
    }
    Repository<Machine> copy = machines_;
    copy.get(machine_id).complete_maintenance();
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
    auto suggestions = suggestions_;
    remember(suggestions, SuggestionKind::ClientName, copy.get(id).name());
    clients_.swap(copy);
    suggestions_.swap(suggestions);
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
    auto suggestions = suggestions_;
    remember(suggestions, SuggestionKind::ClientName, updated.name());
    clients_.swap(copy);
    suggestions_.swap(suggestions);
    publish();
}

bool DryCleaningShop::can_remove_client(Id client_id) const {
    return clients_.get(client_id).orders_count() == 0 &&
           std::none_of(orders_.begin(), orders_.end(), [client_id](const Order& order) {
               return order.client_id() == client_id;
           });
}

void DryCleaningShop::remove_client(Id client_id, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageClients);
    if (!can_remove_client(client_id)) {
        throw InvalidInputException("client", "нельзя удалить клиента, у которого есть заказы");
    }
    Repository<Client> copy = clients_;
    copy.remove(client_id);
    clients_.swap(copy);
    publish();
}

bool DryCleaningShop::has_employee_records(Id employee_id) const {
    const bool in_orders = std::any_of(orders_.begin(), orders_.end(), [employee_id](const Order& order) {
        return order.employee_id() == employee_id ||
               std::any_of(order.history().begin(), order.history().end(),
                           [employee_id](const StatusRecord& record) { return record.employee_id() == employee_id; });
    });
    return in_orders || std::any_of(batches_.begin(), batches_.end(), [employee_id](const Batch& batch) {
               return batch.created_by() == employee_id || batch.started_by() == employee_id ||
                      batch.completed_by() == employee_id;
           });
}

std::string DryCleaningShop::employee_removal_block(Id employee_id, Id acting_employee) const {
    const Employee& target = employees_.get(employee_id);
    if (employee_id == acting_employee) {
        return "Нельзя удалить свою учётную запись.";
    }
    if (target.role() == EmployeeRole::Administrator && administrators_count() <= kMinAdministrators) {
        return "Назначьте другого администратора, чтобы удалить эту учётную запись.";
    }
    if (has_employee_records(employee_id)) {
        return "Нельзя удалить сотрудника, указанного в заказе или партии.";
    }
    return {};
}

void DryCleaningShop::remove_employee(Id employee_id, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    const std::string reason = employee_removal_block(employee_id, acting_employee);
    if (!reason.empty()) {
        throw InvalidInputException("employee", reason);
    }
    Repository<Employee> copy = employees_;
    copy.remove(employee_id);
    employees_.swap(copy);
    publish();
}

bool DryCleaningShop::can_remove_machine(Id machine_id) const {
    return std::none_of(batches_.begin(), batches_.end(), [machine_id](const Batch& batch) {
        return batch.machine_id() == machine_id;
    });
}

void DryCleaningShop::remove_machine(Id machine_id, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStaff);
    if (!can_remove_machine(machine_id)) {
        throw InvalidInputException("machine", "нельзя удалить машину, на которой запланированы или выполнялись партии");
    }
    Repository<Machine> copy = machines_;
    copy.remove(machine_id);
    machines_.swap(copy);
    publish();
}

void DryCleaningShop::remove_material(Id material_id, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStock);
    Repository<Material> copy = materials_;
    copy.remove(material_id);
    materials_.swap(copy);
    publish();
}

bool DryCleaningShop::is_order_editable(Id order_id) const {
    const Order& order = orders_.get(order_id);
    return order.status() == OrderStatus::Accepted &&
           std::none_of(order.item_ids().begin(), order.item_ids().end(), [this](Id item_id) {
               return items_.get(item_id).is_batched();
           });
}

void DryCleaningShop::remove_order(Id order_id, Id acting_employee) {
    begin_mutation(acting_employee, Permission::AcceptOrders);
    const Order& order = orders_.get(order_id);
    if (!is_order_editable(order_id)) {
        throw InvalidInputException("order", "удалить можно только принятый заказ, который ещё не передан в обработку");
    }
    Repository<Item> items = items_;
    Repository<Client> clients = clients_;
    Repository<Order> orders = orders_;
    for (const Id item_id : order.item_ids()) {
        items.remove(item_id);
    }
    clients.get(order.client_id()).remove_order_id(order_id);
    orders.remove(order_id);
    items_.swap(items);
    clients_.swap(clients);
    orders_.swap(orders);
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
    auto suggestions = suggestions_;
    remember_material(suggestions, copy.get(id));
    materials_.swap(copy);
    suggestions_.swap(suggestions);
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
    auto suggestions = suggestions_;
    remember_material(suggestions, updated);
    materials_.swap(copy);
    suggestions_.swap(suggestions);
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

void DryCleaningShop::write_off_material(Id material_id, double amount, Id acting_employee) {
    begin_mutation(acting_employee, Permission::ManageStock);
    if (!(amount > 0.0)) {
        throw InvalidInputException("amount", "количество списания должно быть больше нуля");
    }
    Material updated = materials_.get(material_id);
    updated.write_off(amount);
    updated.validate();
    Repository<Material> copy = materials_;
    replace_by_id(copy, updated);
    materials_.swap(copy);
    publish();
}

std::vector<Material> DryCleaningShop::find_materials(const std::string& query, MaterialOrder order) const {
    const std::string key = to_lower_text(normalize_text(query));
    std::vector<Material> result = materials_.find_if([&key](const Material& material) {
        return key.empty() || to_lower_text(material.name()).find(key) != std::string::npos;
    });
    std::stable_sort(result.begin(), result.end(), [order](const Material& left, const Material& right) {
        if (order == MaterialOrder::ByStock && left.stock() != right.stock()) {
            return left.stock() < right.stock();
        }
        return to_lower_text(left.name()) < to_lower_text(right.name());
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
    OrderQuote quote;
    quote.discount = client.discount();
    for (const Item& item : draft.items) {
        if (item.id() != 0 || item.order_id() != 0 || item.batch_id() != 0 || !item.tag().empty() ||
            !item.completed_batches().empty()) {
            throw InvalidInputException("items", "изделие черновика не должно иметь служебных полей");
        }
        item.validate();
        quote.item_costs.push_back(item_cost(item, draft.urgent, client.discount()));
        quote.item_durations.push_back(item_duration(item, draft.urgent));
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
    const OrderQuote quote = quote_order(draft, now);

    const Id expected_id = orders_.next_id();
    Repository<Item> items = items_;
    Repository<Order> orders = orders_;
    Repository<Client> clients = clients_;

    Order order(draft.client_id, draft.urgent, acting_employee, now, quote.discount);
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
    auto suggestions = suggestions_;
    for (const Item& item : draft.items) remember_item(suggestions, item);

    items_.swap(items);
    orders_.swap(orders);
    clients_.swap(clients);
    suggestions_.swap(suggestions);
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
    if (!is_order_editable(order.id())) {
        throw InvalidInputException("item", "изделие можно менять, только пока заказ не поставлен в обработку");
    }

    Item updated = current;
    updated.set_services(item.services());
    updated.set_kind(item.kind());
    updated.set_material(item.material_name());
    updated.set_color(item.color_name());
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

    double total = 0.0;
    Hours nominal = 0.0;
    for (const Id id : order.item_ids()) {
        const Item& member = id == updated.id() ? updated : items_.get(id);
        total += item_cost(member, order.is_urgent(), order.discount_snapshot());
        nominal = std::max(nominal, item_duration(member, order.is_urgent()));
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
    auto suggestions = suggestions_;
    remember_item(suggestions, updated);
    items_.swap(items);
    orders_.swap(orders);
    suggestions_.swap(suggestions);
    publish();
}

const Service& DryCleaningShop::applicable_service(ServiceType type, const Item& item) const {
    const Service& selected = service(type);
    if (!selected.is_applicable(item)) {
        throw InvalidInputException("services", "изделие не подходит для услуги «" + display_name(type) + "»");
    }
    return selected;
}

double DryCleaningShop::item_cost(const Item& item, bool urgent, double discount) const {
    double total = 0.0;
    for (ServiceType type : item.services()) {
        const Service& selected = applicable_service(type, item);
        total += selected.calculate_cost(item, urgent, discount);
    }
    return Service::round_money(total);
}

Hours DryCleaningShop::item_duration(const Item& item, bool urgent) const {
    Hours total = 0.0;
    for (ServiceType type : item.services()) {
        total += applicable_service(type, item).calculate_duration(item, urgent);
    }
    return total;
}

double DryCleaningShop::calculate_item_cost(Id item_id) const {
    const Item& item = items_.get(item_id);
    const Order& order = orders_.get(item.order_id());
    return item_cost(item, order.is_urgent(), order.discount_snapshot());
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

std::optional<TimePoint> DryCleaningShop::order_processing_completed_at(Id order_id) const {
    const Order& order = orders_.get(order_id);
    TimePoint latest_completion{};
    for (const Id item_id : order.item_ids()) {
        const Item* item = items_.find_by_id(item_id);
        if (item == nullptr || !item->is_batched() || !item->services_completed()) {
            return std::nullopt;
        }
        const Batch* batch = batches_.find_by_id(item->batch_id());
        if (batch == nullptr || batch->status() != BatchStatus::Completed || !batch->completed_at()) {
            return std::nullopt;
        }
        latest_completion = std::max(latest_completion, *batch->completed_at());
    }
    return latest_completion;
}

void DryCleaningShop::change_order_status(Id order_id, OrderStatus status, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::ChangeStatus);
    const Order& order = orders_.get(order_id);
    if (status != OrderStatus::Ready || order.status() != OrderStatus::InProgress) {
        throw InvalidInputException("status", "вручную можно перевести заказ только из «В обработке» в «Готов»");
    }
    const auto latest = order_processing_completed_at(order_id);
    if (!latest) {
        throw InvalidInputException("status", "не все партии заказа завершены");
    }
    if (now < *latest) {
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
        plan_item.service_type = item.service_type();
        plan_item.material_group = item.material_group();
        plan_item.fabric_group = item.fabric_group();
        plan_item.color_group = item.color_group();
        plan_item.weight_kg = item.weight_kg();
        plan_item.duration = service(item.service_type()).calculate_processing(item, order->is_urgent());
        plan_item.finishing = finishing_hours(item, *order);
        plan_item.deadline = order->deadline();
        plan_item.priority = planner_.compute_priority(plan_item, now);
        result.push_back(plan_item);
    }
    return result;
}

Hours DryCleaningShop::finishing_hours(const Item& item, const Order& order) const {
    if (!item.is_last_service()) return 0.0;
    const Service& selected = service(item.service_type());
    const Hours turnaround = selected.calculate_duration(item, order.is_urgent());
    return std::max(0.0, turnaround - selected.calculate_processing(item, order.is_urgent()));
}

std::map<Id, TimePoint> DryCleaningShop::forecast_machine_availability(TimePoint now) const {
    std::map<Id, TimePoint> available;
    for (const Machine& machine : machines_) available.emplace(machine.id(), now);
    for (const auto& [machine_id, queue] : machine_queues(batches_)) {
        TimePoint& slot = available.at(machine_id);
        for (const Batch* batch : queue) {
            if (batch->status() == BatchStatus::Scheduled) {
                slot = std::max(slot, batch->planned_start_at()) + batch->duration();
            } else {
                slot = std::max(slot, batch->occupied_until());
            }
        }
    }
    for (const Machine& machine : machines_) {
        auto& slot = available.at(machine.id());
        slot = std::max(slot, machine.free_at());
    }
    return available;
}

Plan DryCleaningShop::build_plan(TimePoint now, Id acting_employee) const {
    require_permission(acting_employee, Permission::BuildPlan);
    std::vector<Machine> machines(machines_.begin(), machines_.end());
    const auto available = forecast_machine_availability(now);
    for (Machine& machine : machines) machine.set_free_at(available.at(machine.id()));
    const std::vector<Material> materials = materials_.find_if([](const Material&) { return true; });
    Plan plan = planner_.build_plan(collect_plan_items(now), machines, materials, now, revision_);
    for (auto& entry : plan.order_ready_at) {
        if (!entry.second) {
            continue;
        }
        for (const Id item_id : orders_.get(entry.first).item_ids()) {
            const Item& item = items_.get(item_id);
            if (!item.is_last_service()) {
                entry.second = std::nullopt;
                break;
            }
            if (item.is_batched()) {
                entry.second = std::max(
                    *entry.second,
                    add_hours(batches_.get(item.batch_id()).occupied_until(), finishing_hours(item, orders_.get(entry.first))));
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
                                     const Plan& plan) const {
    auto cursor = forecast_machine_availability(plan.built_at);
    std::map<Id, Hours> work;
    for (std::size_t i = 0; i < batches.size(); ++i) {
        work[batches[i].machine_id] += durations[i];
        if (work[batches[i].machine_id] > machines_.get(batches[i].machine_id).maintenance_remaining() + EPS) {
            throw PlanningException("план превышает оставшийся ресурс машины до ТО");
        }
        TimePoint& slot = cursor[batches[i].machine_id];
        batches[i].planned_start_at = slot;
        batches[i].planned_ready_at = add_hours(slot, durations[i]);
        for (Id id : batches[i].item_ids) {
            const auto& previous = items_.get(id).completed_batches();
            if (!previous.empty() && slot < *batches_.get(previous.back()).completed_at()) {
                throw PlanningException("этап начинается раньше завершения предыдущего", id);
            }
        }
        slot = batches[i].planned_ready_at;
        if (batches[i].planned_start_at != plan.batches[i].planned_start_at ||
            batches[i].planned_ready_at != plan.batches[i].planned_ready_at) {
            throw PlanningException("расписание плана устарело, перестройте план");
        }
    }
}

std::string DryCleaningShop::plan_freshness_block(const Plan& plan, TimePoint now) const {
    if (plan.source_revision != revision_) {
        return "Данные изменились. Постройте план заново.";
    }
    if (now < plan.built_at) {
        return "Время компьютера изменилось. Проверьте часы и постройте план заново.";
    }
    if (now - plan.built_at > kPlanLifetime) {
        return "Срок действия плана истёк. Постройте план заново.";
    }
    return {};
}

std::vector<PlannedBatch> DryCleaningShop::verified_batches(const Plan& plan, TimePoint now) const {
    if (const auto block = plan_freshness_block(plan, now); !block.empty()) throw PlanningException(block);
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
    check_schedule(result, durations, plan);
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
        Machine& machine = machines.get(planned.machine_id);
        machine.reserve_work(hours_between(planned.planned_ready_at, planned.planned_start_at));
        machine.occupy_until(planned.planned_ready_at);
    }
    for (const auto& entry : consumption) {
        materials.get(entry.first).write_off(entry.second);
    }
    const FinishingHours finishing = [this](const Item& item, const Order& order) {
        return finishing_hours(item, order);
    };
    advance_orders(orders, items, batches, touched_orders, finishing, acting_employee, now);

    batches_.swap(batches);
    items_.swap(items);
    machines_.swap(machines);
    materials_.swap(materials);
    orders_.swap(orders);
    publish();
    return created;
}

std::string DryCleaningShop::batch_start_block(Id batch_id, TimePoint now) const {
    const Batch& batch = batches_.get(batch_id);
    if (batch.status() != BatchStatus::Scheduled) return "Партия уже была запущена.";
    for (const Batch& other : batches_) {
        if (other.machine_id() != batch.machine_id()) continue;
        if (other.status() == BatchStatus::InProgress) {
            return "Сначала подтвердите завершение партии №" + std::to_string(other.id()) + ".";
        }
        if (other.id() < batch.id() && other.status() == BatchStatus::Scheduled) {
            return "Перед этой партией в очереди есть партия №" + std::to_string(other.id()) + ".";
        }
        if (other.completed_at() && now < *other.completed_at()) {
            return "Время запуска должно быть не раньше подтверждения предыдущей партии.";
        }
    }
    for (Id id : batch.item_ids()) {
        const Item& item = items_.get(id);
        const auto& previous = item.completed_batches();
        if (!previous.empty() && now < *batches_.get(previous.back()).completed_at()) {
            return "Время запуска должно быть не раньше завершения предыдущего этапа изделия.";
        }
        const auto& history = orders_.get(item.order_id()).history();
        if (now < history.back().timestamp()) {
            return "Проверьте часы компьютера. Время запуска раньше последнего изменения статуса заказа.";
        }
    }
    return {};
}

void DryCleaningShop::start_batch(Id batch_id, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::ChangeStatus);
    const auto blocked = batch_start_block(batch_id, now);
    if (!blocked.empty()) throw InvalidInputException("batch", blocked);
    Batch updated = batches_.get(batch_id);
    updated.start(acting_employee, now);
    apply_batch_event(updated, acting_employee, now);
}

void DryCleaningShop::complete_batch(Id batch_id, Id acting_employee, TimePoint now) {
    begin_mutation(acting_employee, Permission::ChangeStatus);
    Batch updated = batches_.get(batch_id);
    updated.complete(acting_employee, now);
    apply_batch_event(updated, acting_employee, now);
}

void DryCleaningShop::apply_batch_event(const Batch& updated, Id acting_employee, TimePoint now) {
    updated.validate();
    Repository<Batch> batches = batches_;
    replace_by_id(batches, updated);
    Repository<Item> items = items_;
    Repository<Order> orders = orders_;
    Repository<Machine> machines = machines_;
    std::set<Id> touched_orders;
    for (Id id : updated.item_ids()) {
        Item& item = items.get(id);
        if (updated.status() == BatchStatus::Completed) item.complete_service();
        touched_orders.insert(item.order_id());
    }
    std::vector<Batch*> waiting;
    for (Batch& batch : batches) {
        if (batch.machine_id() == updated.machine_id() && batch.status() == BatchStatus::Scheduled) {
            waiting.push_back(&batch);
        }
    }
    std::ranges::sort(waiting, {}, &Batch::id);
    auto slot = updated.occupied_until();
    for (Batch* batch : waiting) {
        if (batch->planned_start_at() < slot) {
            batch->postpone_to(slot);
            for (Id id : batch->item_ids()) touched_orders.insert(items.get(id).order_id());
        }
        slot = batch->planned_ready_at();
    }
    machines.get(updated.machine_id()).set_free_at(slot);
    advance_orders(orders, items, batches, touched_orders, [this](const Item& item, const Order& order) {
        return finishing_hours(item, order);
    }, acting_employee, now);
    batches_.swap(batches);
    items_.swap(items);
    orders_.swap(orders);
    machines_.swap(machines);
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
    if (!employees.contains(batch.created_by()) || (batch.started_by() && !employees.contains(*batch.started_by())) ||
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
        if (item == nullptr) {
            throw IntegrityException("изделие партии отсутствует или указывает на другую партию");
        }
        const auto& completed = item->completed_batches();
        const auto stage = std::find(completed.begin(), completed.end(), batch.id());
        ServiceType type = item->service_type();
        if (stage != completed.end()) {
            const auto index = static_cast<std::size_t>(stage - completed.begin());
            type = item->services()[index];
            if (batch.status() != BatchStatus::Completed) {
                throw IntegrityException("завершённая услуга ссылается на незавершённую партию");
            }
        } else if (item->batch_id() != batch.id() || batch.status() == BatchStatus::Completed) {
            throw IntegrityException("партия не соответствует текущей услуге изделия");
        }
        const Order& order = orders.get(item->order_id());
        if (type != batch.service_type()) {
            throw IntegrityException("услуга изделия не совпадает с услугой партии");
        }
        weight += item->weight_kg();
        longest = std::max(longest, service(type).calculate_processing(*item, order.is_urgent()));
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
        save_file(suggestions_, directory);
        write_manifest(directory / kManifestName, revision_,
                       {{file_name_for<Client>(), clients_.size()},
                        {file_name_for<Item>(), items_.size()},
                        {file_name_for<Order>(), orders_.size()},
                        {file_name_for<Batch>(), batches_.size()},
                        {file_name_for<Material>(), materials_.size()},
                        {file_name_for<Employee>(), employees_.size()},
                        {file_name_for<Machine>(), machines_.size()},
                        {file_name_for<Suggestion>(), suggestions_.size()}});
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
    Repository<Suggestion> suggestions;
    Revision stored_revision = 0;

    if (!current) {
        if (storage_->has_data()) {
            throw FileFormatException("CURRENT", kWholeFile, "указатель CURRENT отсутствует, но в каталоге есть данные");
        }
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
        load_file(suggestions, directory, counts);
        check_suggestions(suggestions);
        check_references(clients, items, orders, batches, employees, machines);
    }

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
    suggestions_.swap(suggestions);
    revision_ = loaded_ ? base + 1 : base;
    loaded_ = true;
}
