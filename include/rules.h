#pragma once

#include <optional>
#include <string>

#include "planning_types.h"

class CompatibilityRule {
public:
    virtual ~CompatibilityRule() = default;

    virtual std::optional<std::string> check(const PlanItem& first, const PlanItem& second) const = 0;
    virtual std::string name() const = 0;
};

class FabricRule : public CompatibilityRule {
public:
    std::optional<std::string> check(const PlanItem& first, const PlanItem& second) const override;
    std::string name() const override;
};

class ColorRule : public CompatibilityRule {
public:
    std::optional<std::string> check(const PlanItem& first, const PlanItem& second) const override;
    std::string name() const override;
};

class ModeRule : public CompatibilityRule {
public:
    std::optional<std::string> check(const PlanItem& first, const PlanItem& second) const override;
    std::string name() const override;
};
