#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "machine.h"
#include "material.h"
#include "planning_types.h"
#include "rules.h"
#include "types.h"

constexpr double kDefaultLambda = 10.0;

class BatchPlanner {
public:
    BatchPlanner() = default;

    void add_rule(std::unique_ptr<CompatibilityRule> rule);
    std::size_t rules_count() const noexcept;
    double lambda() const noexcept;
    void set_lambda(double lambda);

    Plan build_plan(std::vector<PlanItem> items, const std::vector<Machine>& machines,
                    const std::vector<Material>& materials, TimePoint now, Revision source_revision) const;

    ConflictGraph build_conflict_graph(const std::vector<PlanItem>& items) const;
    Hours compute_slack(const PlanItem& item, TimePoint now) const;
    double compute_priority(const PlanItem& item, TimePoint now) const;
    void prioritize(std::vector<PlanItem>& items, const ConflictGraph& graph, TimePoint now) const;
    std::vector<std::string> explain_conflict(const ConflictGraph& graph, Id first, Id second) const;
    static double fill_ratio(double weight, double capacity);

private:
    Plan plan_batches(const std::vector<PlanItem>& sorted, const ConflictGraph& graph,
                      const std::vector<Machine>& machines, const std::vector<Material>& materials,
                      TimePoint now) const;
    std::optional<std::string> can_add(const PlannedBatch& batch, const PlanItem& item, const ConflictGraph& graph,
                                       const Machine& machine, const std::map<Id, double>& stock_left,
                                       const std::vector<Material>& materials) const;
    std::optional<Id> pick_machine(ServiceType service, double weight, const std::vector<Machine>& machines,
                                   const std::map<Id, TimePoint>& available) const;
    void reschedule(Plan& plan, const std::vector<PlanItem>& items, const std::vector<Machine>& machines,
                    TimePoint now) const;
    static bool mergeable(const PlannedBatch& first, const PlannedBatch& second, const ConflictGraph& graph,
                          const Machine& machine);
    void merge_batches(Plan& plan, const ConflictGraph& graph, const std::vector<PlanItem>& items,
                       const std::vector<Machine>& machines, TimePoint now) const;
    void evaluate(Plan& plan, const std::vector<PlanItem>& items, const std::vector<Machine>& machines) const;

    std::vector<std::unique_ptr<CompatibilityRule>> rules_;
    double lambda_ = kDefaultLambda;
};
