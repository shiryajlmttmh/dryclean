#pragma once

#include <compare>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "types.h"

class Item {
public:
    struct Defect {
        StainClass type = StainClass::Light;
        std::string description;
        double area_cm2 = 0.0;

        double surcharge() const;
        void validate() const;
        bool operator==(const Defect& other) const = default;
    };

    Item() = default;
    Item(std::string kind, std::string material_name, MaterialGroup material_group, FabricGroup fabric_group,
         std::string color_name, ColorGroup color_group, double weight_kg, SoilLevel soil_level);

    Id id() const noexcept;
    Id order_id() const noexcept;
    const std::string& tag() const noexcept;
    const std::string& kind() const noexcept;
    const std::string& material_name() const noexcept;
    MaterialGroup material_group() const noexcept;
    FabricGroup fabric_group() const noexcept;
    const std::string& color_name() const noexcept;
    ColorGroup color_group() const noexcept;
    double weight_kg() const noexcept;
    SoilLevel soil_level() const noexcept;
    const std::vector<Defect>& defects() const noexcept;
    const std::string& condition_notes() const noexcept;
    Id batch_id() const noexcept;

    void set_id(Id id) noexcept;
    void set_order_id(Id order_id) noexcept;
    void set_tag(std::string tag);
    void set_kind(std::string kind);
    void set_material(std::string material_name, MaterialGroup material_group, FabricGroup fabric_group);
    void set_color(std::string color_name, ColorGroup color_group);
    void set_weight_kg(double weight_kg);
    void set_soil_level(SoilLevel soil_level);
    void set_condition_notes(std::string condition_notes);
    void set_batch_id(Id batch_id) noexcept;
    void add_defect(Defect defect);
    void remove_defect(std::size_t index);

    std::size_t stain_count() const noexcept;
    bool is_batched() const noexcept;
    static std::string make_tag(Id order_id, std::size_t index);
    void validate() const;

    bool operator==(const Item& other) const noexcept;
    std::strong_ordering operator<=>(const Item& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Item& item);
    friend std::istream& operator>>(std::istream& is, Item& item);

private:
    Id id_ = 0;
    Id order_id_ = 0;
    std::string tag_;
    std::string kind_;
    std::string material_name_;
    MaterialGroup material_group_ = MaterialGroup::Basic;
    FabricGroup fabric_group_ = FabricGroup::Dense;
    std::string color_name_;
    ColorGroup color_group_ = ColorGroup::Light;
    double weight_kg_ = 0.0;
    SoilLevel soil_level_ = SoilLevel::Light;
    std::vector<Defect> defects_;
    std::string condition_notes_;
    Id batch_id_ = 0;
};

std::ostream& operator<<(std::ostream& os, const Item::Defect& defect);
std::istream& operator>>(std::istream& is, Item::Defect& defect);
