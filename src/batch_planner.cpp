#include "batch_planner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>

#include "exceptions.h"

namespace {

constexpr auto kPlanningDelay = std::chrono::minutes(1);
constexpr double kPercent = 100.0;

const Machine* find_machine(const std::vector<Machine>& machines, Id id) {
    for (const Machine& machine : machines) {
        if (machine.id() == id) {
            return &machine;
        }
    }
    return nullptr;
}

std::map<Id, const PlanItem*> index_items(const std::vector<PlanItem>& items) {
    std::map<Id, const PlanItem*> result;
    for (const PlanItem& item : items) {
        result[item.item_id] = &item;
    }
    return result;
}

std::map<Id, TimePoint> machine_availability(const Plan& plan, const std::vector<Machine>& machines,
                                             TimePoint now) {
    std::map<Id, TimePoint> result;
    const TimePoint base = now + kPlanningDelay;
    for (const Machine& machine : machines) {
        result[machine.id()] = std::max(base, machine.free_at());
    }
    for (const PlannedBatch& batch : plan.batches) {
        TimePoint& slot = result[batch.machine_id];
        slot = std::max(slot, batch.planned_ready_at);
    }
    return result;
}

std::vector<const Material*> norm_materials(ServiceType service, const std::vector<Material>& materials) {
    std::vector<const Material*> result;
    for (const Material& material : materials) {
        if (material.is_used_for(service) && material.consumption_per_kg() > 0.0) {
            result.push_back(&material);
        }
    }
    return result;
}

void write_off_stock(const PlanItem& item, const std::vector<Material>& materials,
                     std::map<Id, double>& stock_left) {
    for (const Material* material : norm_materials(item.service_type, materials)) {
        stock_left[material->id()] -= material->required_for(item.weight_kg);
    }
}

void validate_items(const std::vector<PlanItem>& items) {
    std::set<Id> seen;
    for (const PlanItem& item : items) {
        if (item.item_id == 0) {
            throw PlanningException("у изделия нулевой идентификатор", item.item_id);
        }
        if (!seen.insert(item.item_id).second) {
            throw PlanningException("повторяющийся идентификатор изделия", item.item_id);
        }
        if (!std::isfinite(item.weight_kg) || item.weight_kg <= 0.0) {
            throw PlanningException("масса изделия должна быть больше нуля", item.item_id);
        }
        if (!std::isfinite(item.duration) || item.duration < 0.0) {
            throw PlanningException("длительность изделия должна быть неотрицательной", item.item_id);
        }
    }
}

}

void BatchPlanner::add_rule(std::unique_ptr<CompatibilityRule> rule) {
    if (!rule) {
        throw InvalidInputException("rule", "правило совместимости не задано");
    }
    rules_.push_back(std::move(rule));
}

std::size_t BatchPlanner::rules_count() const noexcept { return rules_.size(); }

double BatchPlanner::lambda() const noexcept { return lambda_; }

void BatchPlanner::set_lambda(double lambda) {
    if (!std::isfinite(lambda) || lambda < 0.0) {
        throw InvalidInputException("lambda", "коэффициент штрафа должен быть конечным и неотрицательным");
    }
    lambda_ = lambda;
}

double BatchPlanner::fill_ratio(double weight, double capacity) {
    if (!(capacity > 0.0)) {
        return 0.0;
    }
    return weight / capacity;
}

ConflictGraph BatchPlanner::build_conflict_graph(const std::vector<PlanItem>& items) const {
    ConflictGraph graph;
    for (const PlanItem& item : items) {
        graph[item.item_id];
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        for (std::size_t j = i + 1; j < items.size(); ++j) {
            std::vector<std::string> reasons;
            for (const auto& rule : rules_) {
                if (const auto reason = rule->check(items[i], items[j])) {
                    reasons.push_back(rule->name() + ": " + *reason);
                }
            }
            if (!reasons.empty()) {
                graph[items[i].item_id][items[j].item_id] = reasons;
                graph[items[j].item_id][items[i].item_id] = std::move(reasons);
            }
        }
    }
    return graph;
}

Hours BatchPlanner::compute_slack(const PlanItem& item, TimePoint now) const {
    return hours_between(item.deadline, now) - item.duration;
}

double BatchPlanner::compute_priority(const PlanItem& item, TimePoint now) const {
    return 1.0 / (1.0 + std::max(compute_slack(item, now), 0.0));
}

void BatchPlanner::prioritize(std::vector<PlanItem>& items, const ConflictGraph& graph, TimePoint now) const {
    for (PlanItem& item : items) {
        const auto node = graph.find(item.item_id);
        item.degree = node == graph.end() ? 0 : node->second.size();
        item.priority = compute_priority(item, now);
    }
    std::sort(items.begin(), items.end(), [](const PlanItem& left, const PlanItem& right) {
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        if (left.degree != right.degree) {
            return left.degree > right.degree;
        }
        if (left.deadline != right.deadline) {
            return left.deadline < right.deadline;
        }
        return left.item_id < right.item_id;
    });
}

std::vector<std::string> BatchPlanner::explain_conflict(const ConflictGraph& graph, Id first, Id second) const {
    const auto node = graph.find(first);
    if (node == graph.end()) {
        return {};
    }
    const auto edge = node->second.find(second);
    if (edge == node->second.end()) {
        return {};
    }
    return edge->second;
}

std::optional<std::string> BatchPlanner::can_add(const PlannedBatch& batch, const PlanItem& item,
                                                 const ConflictGraph& graph, const Machine& machine,
                                                 const std::map<Id, double>& stock_left,
                                                 const std::vector<Material>& materials) const {
    if (batch.service_type != item.service_type || !machine.can_process(item.service_type)) {
        return std::string("машина не подходит для услуги");
    }
    const auto node = graph.find(item.item_id);
    if (node != graph.end()) {
        for (const Id member : batch.item_ids) {
            const auto edge = node->second.find(member);
            if (edge != node->second.end()) {
                return std::string("несовместимо с изделием из партии: ") + edge->second.front();
            }
        }
    }
    if (batch.total_weight_kg + item.weight_kg > machine.capacity_kg() + EPS) {
        return std::string("превышена вместимость машины");
    }
    const auto norms = norm_materials(item.service_type, materials);
    if (norms.empty()) {
        return std::string("для услуги не заданы нормы расхода материалов");
    }
    for (const Material* material : norms) {
        const auto stock = stock_left.find(material->id());
        const double left = stock == stock_left.end() ? 0.0 : stock->second;
        if (material->required_for(item.weight_kg) > left + EPS) {
            return "не хватает материала «" + material->name() + "»";
        }
    }
    return std::nullopt;
}

std::optional<Id> BatchPlanner::pick_machine(ServiceType service, double weight,
                                             const std::vector<Machine>& machines,
                                             const std::map<Id, TimePoint>& available) const {
    std::optional<Id> best;
    TimePoint best_time{};
    for (const Machine& machine : machines) {
        if (!machine.can_process(service) || machine.capacity_kg() + EPS < weight) {
            continue;
        }
        const auto slot = available.find(machine.id());
        if (slot == available.end()) {
            continue;
        }
        if (!best || slot->second < best_time || (slot->second == best_time && machine.id() < *best)) {
            best = machine.id();
            best_time = slot->second;
        }
    }
    return best;
}

void BatchPlanner::reschedule(Plan& plan, const std::vector<PlanItem>& items, const std::vector<Machine>& machines,
                              TimePoint now) const {
    const auto by_id = index_items(items);
    const TimePoint base = now + kPlanningDelay;
    std::map<Id, TimePoint> cursor;
    for (const Machine& machine : machines) {
        cursor[machine.id()] = std::max(base, machine.free_at());
    }
    for (PlannedBatch& batch : plan.batches) {
        Hours longest = 0.0;
        for (const Id id : batch.item_ids) {
            const auto found = by_id.find(id);
            if (found != by_id.end()) {
                longest = std::max(longest, found->second->duration);
            }
        }
        TimePoint& slot = cursor[batch.machine_id];
        batch.planned_start_at = slot;
        batch.planned_ready_at = add_hours(slot, longest);
        slot = batch.planned_ready_at;
    }
}

void BatchPlanner::evaluate(Plan& plan, const std::vector<PlanItem>& items,
                            const std::vector<Machine>& machines) const {
    const auto by_id = index_items(items);
    double fill_sum = 0.0;
    std::size_t late = 0;
    for (PlannedBatch& batch : plan.batches) {
        double weight = 0.0;
        for (const Id id : batch.item_ids) {
            const auto found = by_id.find(id);
            if (found == by_id.end()) {
                continue;
            }
            weight += found->second->weight_kg;
            if (batch.planned_ready_at > found->second->deadline) {
                ++late;
            }
        }
        const Machine* machine = find_machine(machines, batch.machine_id);
        batch.total_weight_kg = weight;
        batch.fill_ratio = machine == nullptr ? 0.0 : fill_ratio(weight, machine->capacity_kg());
        fill_sum += batch.fill_ratio;
    }
    const std::size_t count = plan.batches.size();
    plan.late_count = late;
    plan.objective = count == 0 ? 0.0 : static_cast<double>(count) + lambda_ * static_cast<double>(late);
    plan.average_load = count == 0 ? 0.0 : kPercent * fill_sum / static_cast<double>(count);
}

Plan BatchPlanner::plan_batches(const std::vector<PlanItem>& sorted, const ConflictGraph& graph,
                                const std::vector<Machine>& machines, const std::vector<Material>& materials,
                                TimePoint now) const {
    Plan plan;
    std::map<Id, double> stock_left;
    for (const Material& material : materials) {
        stock_left[material.id()] = material.stock();
    }

    for (const PlanItem& item : sorted) {
        std::optional<std::size_t> best_index;
        double best_fill = 0.0;
        TimePoint best_end{};
        Id best_machine = 0;

        for (std::size_t index = 0; index < plan.batches.size(); ++index) {
            const PlannedBatch& batch = plan.batches[index];
            const Machine* machine = find_machine(machines, batch.machine_id);
            if (machine == nullptr || can_add(batch, item, graph, *machine, stock_left, materials)) {
                continue;
            }
            const double fill = fill_ratio(batch.total_weight_kg + item.weight_kg, machine->capacity_kg());
            Plan trial;
            trial.batches = plan.batches;
            trial.batches[index].item_ids.push_back(item.item_id);
            std::vector<PlanItem> known(sorted);
            reschedule(trial, known, machines, now);
            const TimePoint end = trial.batches[index].planned_ready_at;
            const bool better = !best_index || fill > best_fill + EPS ||
                                (std::abs(fill - best_fill) <= EPS &&
                                 (end < best_end || (end == best_end && batch.machine_id < best_machine)));
            if (better) {
                best_index = index;
                best_fill = fill;
                best_end = end;
                best_machine = batch.machine_id;
            }
        }

        if (best_index) {
            PlannedBatch& batch = plan.batches[*best_index];
            batch.item_ids.push_back(item.item_id);
            batch.total_weight_kg += item.weight_kg;
            write_off_stock(item, materials, stock_left);
            reschedule(plan, sorted, machines, now);
            continue;
        }

        double max_capacity = 0.0;
        bool has_type = false;
        for (const Machine& machine : machines) {
            if (machine.can_process(item.service_type)) {
                has_type = true;
                max_capacity = std::max(max_capacity, machine.capacity_kg());
            }
        }
        if (!has_type) {
            plan.unplaced.emplace_back(item.item_id, "нет машины для этой услуги");
            continue;
        }
        if (item.weight_kg > max_capacity + EPS) {
            plan.unplaced.emplace_back(item.item_id, "вес изделия больше вместимости машины");
            continue;
        }
        const auto available = machine_availability(plan, machines, now);
        const auto machine_id = pick_machine(item.service_type, item.weight_kg, machines, available);
        if (!machine_id) {
            plan.unplaced.emplace_back(item.item_id, "нет подходящей машины");
            continue;
        }
        PlannedBatch fresh;
        fresh.machine_id = *machine_id;
        fresh.service_type = item.service_type;
        const Machine* machine = find_machine(machines, *machine_id);
        if (const auto reason = can_add(fresh, item, graph, *machine, stock_left, materials)) {
            plan.unplaced.emplace_back(item.item_id, *reason);
            continue;
        }
        fresh.item_ids.push_back(item.item_id);
        fresh.total_weight_kg = item.weight_kg;
        plan.batches.push_back(std::move(fresh));
        write_off_stock(item, materials, stock_left);
        reschedule(plan, sorted, machines, now);
    }
    return plan;
}

Plan BatchPlanner::build_plan(std::vector<PlanItem> items, const std::vector<Machine>& machines,
                              const std::vector<Material>& materials, TimePoint now,
                              Revision source_revision) const {
    if (rules_.empty()) {
        throw PlanningException("правила совместимости не заданы");
    }
    validate_items(items);
    ConflictGraph graph = build_conflict_graph(items);
    prioritize(items, graph, now);
    Plan plan = plan_batches(items, graph, machines, materials, now);
    plan.source_revision = source_revision;
    plan.built_at = now;
    plan.conflicts = std::move(graph);
    reschedule(plan, items, machines, now);
    evaluate(plan, items, machines);

    std::map<Id, std::optional<TimePoint>> ready;
    for (const PlanItem& item : items) {
        ready.emplace(item.order_id, std::optional<TimePoint>(TimePoint{}));
    }
    std::set<Id> placed;
    std::map<Id, Id> order_of;
    for (const PlanItem& item : items) {
        order_of[item.item_id] = item.order_id;
    }
    for (const PlannedBatch& batch : plan.batches) {
        for (const Id id : batch.item_ids) {
            placed.insert(id);
            std::optional<TimePoint>& slot = ready[order_of[id]];
            if (slot) {
                slot = std::max(*slot, batch.planned_ready_at);
            }
        }
    }
    for (const auto& [id, reason] : plan.unplaced) {
        ready[order_of[id]] = std::nullopt;
    }
    plan.order_ready_at = std::move(ready);
    return plan;
}
