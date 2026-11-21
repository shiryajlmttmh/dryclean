#include "item.h"

#include <cmath>
#include <algorithm>
#include <set>
#include <iomanip>
#include <istream>
#include <locale>
#include <ostream>
#include <sstream>
#include <utility>

#include "exceptions.h"
#include "item_catalog.h"
#include "normalize.h"
#include "pricing.h"
#include "record_io.h"

namespace {

constexpr int kTagOrderWidth = 4;
constexpr int kTagIndexWidth = 2;
constexpr char kTagPrefix = 'O';
constexpr char kTagSeparator = '-';

template <class Group, std::size_t Count>
Group catalog_group(const std::array<ItemCatalogEntry<Group>, Count>& catalog, std::string_view name,
                    const char* field, const char* message) {
    const auto entry = std::ranges::find(catalog, name, &ItemCatalogEntry<Group>::name);
    if (entry == catalog.end()) throw InvalidInputException(field, message);
    return entry->group;
}

void check_weight(double weight_kg) {
    if (!std::isfinite(weight_kg) || weight_kg <= 0.0) {
        throw InvalidInputException("weight_kg", "масса должна быть больше нуля");
    }
}

void check_soil_level(SoilLevel soil_level) {
    if (!is_valid(soil_level)) {
        throw InvalidInputException("soil_level", "неизвестная степень загрязнения");
    }
}

void check_optional_text(const char* field_name, const std::string& value) {
    if (!value.empty()) {
        require_text(field_name, value);
    }
}

void write_defect(RecordWriter& writer, const Item::Defect& defect) {
    writer.put_token(to_string(defect.type));
    writer.put_string(defect.name);
    writer.put_string(defect.description);
    writer.put_double(defect.area_cm2);
}

Item::Defect read_defect(RecordReader& reader) {
    Item::Defect defect;
    defect.type = parse_StainClass(reader.get_token());
    defect.name = reader.get_string();
    defect.description = reader.get_string();
    defect.area_cm2 = reader.get_double();
    return defect;
}

}

double Item::Defect::surcharge() const { return stain_surcharge(type); }

void Item::Defect::validate() const {
    if (!is_valid(type)) {
        throw InvalidInputException("type", "неизвестный класс пятна");
    }
    require_text("defect_name", name);
    check_optional_text("description", description);
    if (!std::isfinite(area_cm2) || area_cm2 <= 0.0) {
        throw InvalidInputException("area_cm2", "площадь пятна должна быть больше нуля");
    }
    if (type == StainClass::Light && area_cm2 > kLightStainMaxAreaCm2) {
        throw InvalidInputException("type", "лёгким может быть только пятно площадью до " +
                                                std::to_string(static_cast<int>(kLightStainMaxAreaCm2)) + " см²");
    }
}

Item::Item(std::string kind, std::string material_name, std::string color_name, double weight_kg, SoilLevel soil_level) {
    set_kind(std::move(kind));
    set_material(std::move(material_name));
    set_color(std::move(color_name));
    set_weight_kg(weight_kg);
    set_soil_level(soil_level);
}

Id Item::id() const noexcept { return id_; }

Id Item::order_id() const noexcept { return order_id_; }

const std::string& Item::tag() const noexcept { return tag_; }

const std::string& Item::kind() const noexcept { return kind_; }

const std::string& Item::material_name() const noexcept { return material_name_; }

MaterialGroup Item::material_group() const noexcept { return material_group_; }

FabricGroup Item::fabric_group() const { return fabric_group_for(material_group_); }

const std::string& Item::color_name() const noexcept { return color_name_; }

ColorGroup Item::color_group() const noexcept { return color_group_; }

double Item::weight_kg() const noexcept { return weight_kg_; }

SoilLevel Item::soil_level() const noexcept { return soil_level_; }

const std::vector<Item::Defect>& Item::defects() const noexcept { return defects_; }

const std::string& Item::condition_notes() const noexcept { return condition_notes_; }

Id Item::batch_id() const noexcept { return batch_id_; }

void Item::set_id(Id id) noexcept { id_ = id; }

void Item::set_order_id(Id order_id) noexcept { order_id_ = order_id; }

void Item::set_tag(std::string tag) {
    tag = normalize_text(std::move(tag));
    check_optional_text("tag", tag);
    tag_ = std::move(tag);
}

void Item::set_kind(std::string kind) {
    kind = normalize_text(std::move(kind));
    require_text("kind", kind);
    kind_ = std::move(kind);
}

void Item::set_material(std::string material_name) {
    const auto group = catalog_group(kItemMaterials, material_name, "material_name", "выберите материал из списка");
    material_name_ = std::move(material_name);
    material_group_ = group;
}

void Item::set_color(std::string color_name) {
    const auto group = catalog_group(kItemColors, color_name, "color_name", "выберите цвет из списка");
    color_name_ = std::move(color_name);
    color_group_ = group;
}

void Item::set_weight_kg(double weight_kg) {
    check_weight(weight_kg);
    weight_kg_ = weight_kg;
}

void Item::set_soil_level(SoilLevel soil_level) {
    check_soil_level(soil_level);
    soil_level_ = soil_level;
}

void Item::set_condition_notes(std::string condition_notes) {
    condition_notes = normalize_text(std::move(condition_notes));
    check_optional_text("condition_notes", condition_notes);
    condition_notes_ = std::move(condition_notes);
}

void Item::set_batch_id(Id batch_id) noexcept { batch_id_ = batch_id; }

ServiceType Item::service_type() const noexcept {
    return services_[std::min(completed_batches_.size(), services_.size() - 1)];
}

const std::vector<ServiceType>& Item::services() const noexcept { return services_; }

const std::vector<Id>& Item::completed_batches() const noexcept { return completed_batches_; }

bool Item::services_completed() const noexcept { return completed_batches_.size() == services_.size(); }

bool Item::is_last_service() const noexcept { return completed_batches_.size() + 1 >= services_.size(); }

void Item::set_services(std::vector<ServiceType> services) {
    if (is_batched() || !completed_batches_.empty()) {
        throw InvalidInputException("services", "нельзя менять услуги изделия в обработке");
    }
    if (services.empty() || services.size() > kMaxNestedElements ||
        std::any_of(services.begin(), services.end(), [](ServiceType type) { return !is_valid(type); })) {
        throw InvalidInputException("services", "задайте последовательность допустимых услуг");
    }
    services_ = std::move(services);
}

void Item::complete_service() {
    if (!is_batched() || services_completed()) {
        throw InvalidInputException("services", "нет услуги для завершения");
    }
    completed_batches_.push_back(batch_id_);
    if (!services_completed()) batch_id_ = 0;
}

void Item::add_defect(Defect defect) {
    defect.name = normalize_text(std::move(defect.name));
    defect.description = normalize_text(std::move(defect.description));
    defect.validate();
    defects_.push_back(std::move(defect));
}

void Item::remove_defect(std::size_t index) {
    if (index >= defects_.size()) {
        throw InvalidInputException("index", "пятна с таким номером нет");
    }
    defects_.erase(defects_.begin() + static_cast<std::ptrdiff_t>(index));
}

std::size_t Item::stain_count() const noexcept { return defects_.size(); }

bool Item::is_batched() const noexcept { return batch_id_ != 0; }

std::string Item::make_tag(Id order_id, std::size_t index) {
    if (order_id == 0) {
        throw InvalidInputException("order_id", "для маркировки нужен идентификатор заказа");
    }
    if (index == 0) {
        throw InvalidInputException("index", "нумерация изделий начинается с единицы");
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setfill('0') << kTagPrefix << std::setw(kTagOrderWidth) << order_id << kTagSeparator
        << std::setw(kTagIndexWidth) << index;
    return out.str();
}

void Item::validate() const {
    if (services_.empty() || services_.size() > kMaxNestedElements ||
        std::any_of(services_.begin(), services_.end(), [](ServiceType type) { return !is_valid(type); }) ||
        completed_batches_.size() > services_.size()) {
        throw InvalidInputException("services", "некорректная последовательность услуг");
    }
    std::set<Id> completed;
    for (Id id : completed_batches_) {
        if (id == 0 || !completed.insert(id).second) {
            throw InvalidInputException("services", "некорректные завершённые партии изделия");
        }
    }
    if (services_completed() && batch_id_ != completed_batches_.back()) {
        throw InvalidInputException("services", "не совпадает последняя партия изделия");
    }
    if (!services_completed() && completed.contains(batch_id_)) {
        throw InvalidInputException("services", "текущая партия изделия уже завершена");
    }
    check_optional_text("tag", tag_);
    require_text("kind", kind_);
    if (catalog_group(kItemMaterials, material_name_, "material_name", "выберите материал из списка") !=
        material_group_) {
        throw InvalidInputException("material_group", "группа не соответствует материалу изделия");
    }
    if (catalog_group(kItemColors, color_name_, "color_name", "выберите цвет из списка") != color_group_) {
        throw InvalidInputException("color_group", "группа не соответствует цвету изделия");
    }
    check_weight(weight_kg_);
    check_soil_level(soil_level_);
    for (const Defect& defect : defects_) {
        defect.validate();
    }
    check_optional_text("condition_notes", condition_notes_);
}

bool Item::operator==(const Item& other) const noexcept { return id_ == other.id_; }

std::strong_ordering Item::operator<=>(const Item& other) const noexcept { return id_ <=> other.id_; }

std::ostream& operator<<(std::ostream& os, const Item& item) {
    RecordWriter writer;
    writer.put_id(item.id_);
    writer.put_id(item.order_id_);
    writer.put_string(item.tag_);
    writer.put_string(item.kind_);
    writer.put_string(item.material_name_);
    writer.put_string(item.color_name_);
    writer.put_double(item.weight_kg_);
    writer.put_token(to_string(item.soil_level_));
    writer.put_count(item.defects_.size());
    for (const Item::Defect& defect : item.defects_) {
        write_defect(writer, defect);
    }
    writer.put_string(item.condition_notes_);
    writer.put_id(item.batch_id_);
    writer.put_count(item.services_.size());
    for (ServiceType type : item.services_) writer.put_token(to_string(type));
    writer.put_count(item.completed_batches_.size());
    for (Id id : item.completed_batches_) writer.put_id(id);
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Item& item) {
    return read_record(is, item, [](Item& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        value.order_id_ = reader.get_id();
        value.tag_ = reader.get_string();
        value.kind_ = reader.get_string();
        value.set_material(reader.get_string());
        value.set_color(reader.get_string());
        value.weight_kg_ = reader.get_double();
        value.soil_level_ = parse_SoilLevel(reader.get_token());
        const std::size_t count = reader.get_count();
        for (std::size_t i = 0; i < count; ++i) {
            value.defects_.push_back(read_defect(reader));
        }
        value.condition_notes_ = reader.get_string();
        value.batch_id_ = reader.get_id();
        value.services_.clear();
        const auto service_count = reader.get_count();
        for (std::size_t i = 0; i < service_count; ++i) value.services_.push_back(parse_ServiceType(reader.get_token()));
        const auto completed_count = reader.get_count();
        for (std::size_t i = 0; i < completed_count; ++i) value.completed_batches_.push_back(reader.get_id());
    });
}

std::ostream& operator<<(std::ostream& os, const Item::Defect& defect) {
    RecordWriter writer;
    write_defect(writer, defect);
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Item::Defect& defect) {
    return read_record(is, defect, [](Item::Defect& value, RecordReader& reader) { value = read_defect(reader); });
}
