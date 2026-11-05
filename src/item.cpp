#include "item.h"

#include <cmath>
#include <iomanip>
#include <istream>
#include <locale>
#include <ostream>
#include <sstream>
#include <utility>

#include "exceptions.h"
#include "normalize.h"
#include "pricing.h"
#include "record_io.h"

namespace {

constexpr int kTagOrderWidth = 4;
constexpr int kTagIndexWidth = 2;
constexpr char kTagPrefix = 'O';
constexpr char kTagSeparator = '-';

void check_material_groups(MaterialGroup material_group, FabricGroup fabric_group) {
    if (!is_valid(material_group)) {
        throw InvalidInputException("material_group", "неизвестная группа материала");
    }
    if (!is_valid(fabric_group)) {
        throw InvalidInputException("fabric_group", "неизвестная группа ткани");
    }
    bool consistent = false;
    switch (material_group) {
        case MaterialGroup::Basic:
            consistent = fabric_group == FabricGroup::Dense || fabric_group == FabricGroup::Delicate;
            break;
        case MaterialGroup::Delicate:
            consistent = fabric_group == FabricGroup::Delicate;
            break;
        case MaterialGroup::LeatherSuede:
        case MaterialGroup::SheepskinVelour:
        case MaterialGroup::NaturalFur:
            consistent = fabric_group == FabricGroup::LeatherFur;
            break;
    }
    if (!consistent) {
        throw InvalidInputException("fabric_group", "группа ткани не соответствует группе материала");
    }
}

void check_color_group(ColorGroup color_group) {
    if (!is_valid(color_group)) {
        throw InvalidInputException("color_group", "неизвестная цветовая группа");
    }
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
    writer.put_string(defect.description);
    writer.put_double(defect.area_cm2);
}

Item::Defect read_defect(RecordReader& reader) {
    Item::Defect defect;
    defect.type = parse_StainClass(reader.get_token());
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
    require_text("description", description);
    if (!std::isfinite(area_cm2) || area_cm2 <= 0.0) {
        throw InvalidInputException("area_cm2", "площадь пятна должна быть больше нуля");
    }
    if (type == StainClass::Light && area_cm2 > kLightStainMaxAreaCm2) {
        throw InvalidInputException("type", "лёгким может быть только пятно площадью до " +
                                                std::to_string(static_cast<int>(kLightStainMaxAreaCm2)) + " см²");
    }
}

Item::Item(std::string kind, std::string material_name, MaterialGroup material_group, FabricGroup fabric_group,
           std::string color_name, ColorGroup color_group, double weight_kg, SoilLevel soil_level) {
    set_kind(std::move(kind));
    set_material(std::move(material_name), material_group, fabric_group);
    set_color(std::move(color_name), color_group);
    set_weight_kg(weight_kg);
    set_soil_level(soil_level);
}

Id Item::id() const noexcept { return id_; }

Id Item::order_id() const noexcept { return order_id_; }

const std::string& Item::tag() const noexcept { return tag_; }

const std::string& Item::kind() const noexcept { return kind_; }

const std::string& Item::material_name() const noexcept { return material_name_; }

MaterialGroup Item::material_group() const noexcept { return material_group_; }

FabricGroup Item::fabric_group() const noexcept { return fabric_group_; }

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

void Item::set_material(std::string material_name, MaterialGroup material_group, FabricGroup fabric_group) {
    material_name = normalize_text(std::move(material_name));
    require_text("material_name", material_name);
    check_material_groups(material_group, fabric_group);
    material_name_ = std::move(material_name);
    material_group_ = material_group;
    fabric_group_ = fabric_group;
}

void Item::set_color(std::string color_name, ColorGroup color_group) {
    color_name = normalize_text(std::move(color_name));
    require_text("color_name", color_name);
    check_color_group(color_group);
    color_name_ = std::move(color_name);
    color_group_ = color_group;
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

void Item::add_defect(Defect defect) {
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
    check_optional_text("tag", tag_);
    require_text("kind", kind_);
    require_text("material_name", material_name_);
    check_material_groups(material_group_, fabric_group_);
    require_text("color_name", color_name_);
    check_color_group(color_group_);
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
    writer.put_token(to_string(item.material_group_));
    writer.put_token(to_string(item.fabric_group_));
    writer.put_string(item.color_name_);
    writer.put_token(to_string(item.color_group_));
    writer.put_double(item.weight_kg_);
    writer.put_token(to_string(item.soil_level_));
    writer.put_count(item.defects_.size());
    for (const Item::Defect& defect : item.defects_) {
        write_defect(writer, defect);
    }
    writer.put_string(item.condition_notes_);
    writer.put_id(item.batch_id_);
    return os << writer.str();
}

std::istream& operator>>(std::istream& is, Item& item) {
    return read_record(is, item, [](Item& value, RecordReader& reader) {
        value.id_ = reader.get_id();
        value.order_id_ = reader.get_id();
        value.tag_ = reader.get_string();
        value.kind_ = reader.get_string();
        value.material_name_ = reader.get_string();
        value.material_group_ = parse_MaterialGroup(reader.get_token());
        value.fabric_group_ = parse_FabricGroup(reader.get_token());
        value.color_name_ = reader.get_string();
        value.color_group_ = parse_ColorGroup(reader.get_token());
        value.weight_kg_ = reader.get_double();
        value.soil_level_ = parse_SoilLevel(reader.get_token());
        const std::size_t count = reader.get_count();
        for (std::size_t i = 0; i < count; ++i) {
            value.defects_.push_back(read_defect(reader));
        }
        value.condition_notes_ = reader.get_string();
        value.batch_id_ = reader.get_id();
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
