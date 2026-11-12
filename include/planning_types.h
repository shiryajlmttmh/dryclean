#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "types.h"

struct PlanItem {
    Id item_id = 0;
    Id order_id = 0;
    ServiceType service_type = ServiceType::DryCleaning;
    MaterialGroup material_group = MaterialGroup::Basic;
    FabricGroup fabric_group = FabricGroup::Dense;
    ColorGroup color_group = ColorGroup::Light;
    double weight_kg = 0.0;
    Hours duration = 0.0;
    Hours finishing = 0.0;
    TimePoint deadline{};
    double priority = 0.0;
    std::size_t degree = 0;
};

using ConflictGraph = std::map<Id, std::map<Id, std::vector<std::string>>>;

struct PlannedBatch {
    std::vector<Id> item_ids;
    Id machine_id = 0;
    ServiceType service_type = ServiceType::DryCleaning;
    double total_weight_kg = 0.0;
    double fill_ratio = 0.0;
    TimePoint planned_start_at{};
    TimePoint planned_ready_at{};
};

struct Plan {
    Revision source_revision = 0;
    TimePoint built_at{};
    std::vector<PlannedBatch> batches;
    std::vector<std::pair<Id, std::string>> unplaced;
    ConflictGraph conflicts;
    std::map<Id, std::optional<TimePoint>> order_ready_at;
    double objective = 0.0;
    double average_load = 0.0;
    std::size_t late_count = 0;
};
