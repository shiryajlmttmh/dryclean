#pragma once

#include <array>
#include <string_view>

#include "types.h"

template <class Group>
struct ItemCatalogEntry {
    std::string_view name;
    Group group;
};

inline constexpr std::array kItemMaterials{
    ItemCatalogEntry{"хлопок", MaterialGroup::Basic},
    ItemCatalogEntry{"лён", MaterialGroup::Basic},
    ItemCatalogEntry{"полиэстер", MaterialGroup::Basic},
    ItemCatalogEntry{"шерсть", MaterialGroup::Delicate},
    ItemCatalogEntry{"шёлк", MaterialGroup::Delicate},
    ItemCatalogEntry{"кожа", MaterialGroup::LeatherSuede},
    ItemCatalogEntry{"замша", MaterialGroup::LeatherSuede},
    ItemCatalogEntry{"овчина", MaterialGroup::SheepskinVelour},
    ItemCatalogEntry{"велюр", MaterialGroup::SheepskinVelour},
    ItemCatalogEntry{"натуральный мех", MaterialGroup::NaturalFur}
};

inline constexpr std::array kItemColors{
    ItemCatalogEntry{"белый", ColorGroup::Light},
    ItemCatalogEntry{"бежевый", ColorGroup::Light},
    ItemCatalogEntry{"серый", ColorGroup::Light},
    ItemCatalogEntry{"чёрный", ColorGroup::Dark},
    ItemCatalogEntry{"коричневый", ColorGroup::Dark},
    ItemCatalogEntry{"красный", ColorGroup::Dark},
    ItemCatalogEntry{"оранжевый", ColorGroup::Dark},
    ItemCatalogEntry{"жёлтый", ColorGroup::Light},
    ItemCatalogEntry{"зелёный", ColorGroup::Dark},
    ItemCatalogEntry{"голубой", ColorGroup::Light},
    ItemCatalogEntry{"синий", ColorGroup::Dark},
    ItemCatalogEntry{"фиолетовый", ColorGroup::Dark},
    ItemCatalogEntry{"розовый", ColorGroup::Light}
};
