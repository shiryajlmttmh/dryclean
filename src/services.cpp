#include "services.h"

#include <cmath>

#include "exceptions.h"
#include "pricing.h"
#include "validation.h"

namespace {

constexpr double kCentsPerUnit = 100.0;
constexpr StainClass kLeatherStainClass = StainClass::Special;

bool is_leather_group(MaterialGroup group) {
    return group == MaterialGroup::LeatherSuede || group == MaterialGroup::SheepskinVelour ||
           group == MaterialGroup::NaturalFur;
}

}

double Service::round_money(double amount) { return std::round(amount * kCentsPerUnit) / kCentsPerUnit; }

double Service::defects_surcharge(const Item& item) const {
    const bool leather = is_leather_group(item.material_group());
    double total = 0.0;
    for (const Item::Defect& defect : item.defects()) {
        total += leather ? stain_surcharge(kLeatherStainClass) : defect.surcharge();
    }
    return total;
}

void Service::require_applicable(const Item& item) const {
    item.validate();
    if (!is_applicable(item)) {
        throw InvalidInputException("service", "услуга «" + name() + "» неприменима к этому изделию");
    }
}

double Service::compute_cost(const Item& item, bool urgent, double client_discount, double soil_factor) const {
    require_applicable(item);
    check_discount(client_discount);
    const double base =
        tariff(type()) * material_cost_k(item.material_group()) * soil_factor + defects_surcharge(item);
    return round_money(base * (1.0 + urgency_surcharge(urgent)) * (1.0 - client_discount));
}

Hours Service::compute_duration(const Item& item, bool urgent) const {
    require_applicable(item);
    const double stain_hours = static_cast<double>(item.stain_count()) * kStainHours;
    return urgency_time_k(urgent) *
           (base_hours(type()) * material_time_k(item.material_group()) + stain_hours);
}

Hours Service::calculate_processing(const Item& item, bool urgent) const {
    require_applicable(item);
    return urgency_time_k(urgent) * cycle_hours(type()) * material_time_k(item.material_group());
}

ServiceType DryCleaningService::type() const noexcept { return ServiceType::DryCleaning; }

std::string DryCleaningService::name() const { return "Сухая (химическая) чистка"; }

bool DryCleaningService::is_applicable(const Item& item) const { return !is_leather_group(item.material_group()); }

double DryCleaningService::calculate_cost(const Item& item, bool urgent, double client_discount) const {
    return compute_cost(item, urgent, client_discount, soil_k(item.soil_level()));
}

Hours DryCleaningService::calculate_duration(const Item& item, bool urgent) const {
    return compute_duration(item, urgent);
}

ServiceType WetCleaningService::type() const noexcept { return ServiceType::WetCleaning; }

std::string WetCleaningService::name() const { return "Влажная чистка (аквачистка)"; }

bool WetCleaningService::is_applicable(const Item& item) const { return !is_leather_group(item.material_group()); }

double WetCleaningService::calculate_cost(const Item& item, bool urgent, double client_discount) const {
    return compute_cost(item, urgent, client_discount, soil_k(item.soil_level()));
}

Hours WetCleaningService::calculate_duration(const Item& item, bool urgent) const {
    return compute_duration(item, urgent);
}

ServiceType StainRemovalService::type() const noexcept { return ServiceType::StainRemoval; }

std::string StainRemovalService::name() const { return "Выведение сложных пятен"; }

bool StainRemovalService::is_applicable(const Item& item) const { return item.stain_count() > 0; }

double StainRemovalService::calculate_cost(const Item& item, bool urgent, double client_discount) const {
    return compute_cost(item, urgent, client_discount, kStainRemovalSoilK);
}

Hours StainRemovalService::calculate_duration(const Item& item, bool urgent) const {
    return compute_duration(item, urgent);
}

ServiceType LeatherService::type() const noexcept { return ServiceType::Leather; }

std::string LeatherService::name() const { return "Чистка изделий из кожи и меха"; }

bool LeatherService::is_applicable(const Item& item) const { return is_leather_group(item.material_group()); }

double LeatherService::calculate_cost(const Item& item, bool urgent, double client_discount) const {
    return compute_cost(item, urgent, client_discount, soil_k(item.soil_level()));
}

Hours LeatherService::calculate_duration(const Item& item, bool urgent) const {
    return compute_duration(item, urgent);
}

std::unique_ptr<Service> make_service(ServiceType type) {
    switch (type) {
        case ServiceType::DryCleaning:
            return std::make_unique<DryCleaningService>();
        case ServiceType::WetCleaning:
            return std::make_unique<WetCleaningService>();
        case ServiceType::StainRemoval:
            return std::make_unique<StainRemovalService>();
        case ServiceType::Leather:
            return std::make_unique<LeatherService>();
    }
    throw InvalidInputException("service_type", "неизвестный вид услуги");
}
