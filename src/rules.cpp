#include "rules.h"

std::optional<std::string> FabricRule::check(const PlanItem& first, const PlanItem& second) const {
    if (first.fabric_group == FabricGroup::LeatherFur || second.fabric_group == FabricGroup::LeatherFur) {
        return std::string("кожа и мех обрабатываются отдельной партией");
    }
    if (first.fabric_group != second.fabric_group) {
        return std::string("плотные и деликатные ткани нельзя обрабатывать вместе");
    }
    return std::nullopt;
}

std::string FabricRule::name() const { return "Группа ткани"; }

std::optional<std::string> ColorRule::check(const PlanItem& first, const PlanItem& second) const {
    if (first.color_group != second.color_group) {
        return std::string("светлые и тёмные изделия нельзя обрабатывать вместе");
    }
    return std::nullopt;
}

std::string ColorRule::name() const { return "Группа цвета"; }

std::optional<std::string> ModeRule::check(const PlanItem& first, const PlanItem& second) const {
    if (first.service_type != second.service_type) {
        return std::string("разные виды услуг обрабатываются в разных режимах");
    }
    return std::nullopt;
}

std::string ModeRule::name() const { return "Режим обработки"; }
