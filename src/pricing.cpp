#include "pricing.h"

#include "exceptions.h"

namespace {

constexpr double kDryCleaningTariff = 65.0;
constexpr double kWetCleaningTariff = 55.0;
constexpr double kStainRemovalTariff = 15.0;
constexpr double kLeatherTariff = 65.0;

constexpr Hours kDryCleaningTurnaroundHours = 144.0;
constexpr Hours kWetCleaningTurnaroundHours = 120.0;
constexpr Hours kStainRemovalTurnaroundHours = 48.0;
constexpr Hours kLeatherTurnaroundHours = 144.0;

constexpr Hours kDryCleaningCycleHours = 3.0;
constexpr Hours kWetCleaningCycleHours = 4.0;
constexpr Hours kStainRemovalCycleHours = 2.0;
constexpr Hours kLeatherCycleHours = 8.0;

constexpr double kBasicMaterialCostFactor = 1.0;
constexpr double kDelicateMaterialCostFactor = 1.2;
constexpr double kLeatherSuedeCostFactor = 1.15;
constexpr double kSheepskinVelourCostFactor = 1.3;
constexpr double kFurCostFactor = 1.45;

constexpr double kBasicMaterialTimeFactor = 1.0;
constexpr double kDelicateMaterialTimeFactor = 1.2;
constexpr double kLeatherSuedeTimeFactor = 1.3;
constexpr double kSheepskinVelourTimeFactor = 1.3;
constexpr double kFurTimeFactor = 1.5;

constexpr double kLightSoilCostFactor = 1.0;
constexpr double kMediumSoilCostFactor = 1.15;
constexpr double kHeavySoilCostFactor = 1.3;

constexpr double kLightStainSurcharge = 10.0;
constexpr double kHardStainSurcharge = 15.0;
constexpr double kSpecialStainSurcharge = 20.0;

}

double tariff(ServiceType service) {
    switch (service) {
        case ServiceType::DryCleaning:
            return kDryCleaningTariff;
        case ServiceType::WetCleaning:
            return kWetCleaningTariff;
        case ServiceType::StainRemoval:
            return kStainRemovalTariff;
        case ServiceType::Leather:
            return kLeatherTariff;
    }
    throw InvalidInputException("ServiceType", "неизвестное значение");
}

Hours base_hours(ServiceType service) {
    switch (service) {
        case ServiceType::DryCleaning:
            return kDryCleaningTurnaroundHours;
        case ServiceType::WetCleaning:
            return kWetCleaningTurnaroundHours;
        case ServiceType::StainRemoval:
            return kStainRemovalTurnaroundHours;
        case ServiceType::Leather:
            return kLeatherTurnaroundHours;
    }
    throw InvalidInputException("ServiceType", "неизвестное значение");
}

Hours cycle_hours(ServiceType service) {
    switch (service) {
        case ServiceType::DryCleaning:
            return kDryCleaningCycleHours;
        case ServiceType::WetCleaning:
            return kWetCleaningCycleHours;
        case ServiceType::StainRemoval:
            return kStainRemovalCycleHours;
        case ServiceType::Leather:
            return kLeatherCycleHours;
    }
    throw InvalidInputException("ServiceType", "неизвестное значение");
}

double material_cost_k(MaterialGroup group) {
    switch (group) {
        case MaterialGroup::Basic:
            return kBasicMaterialCostFactor;
        case MaterialGroup::Delicate:
            return kDelicateMaterialCostFactor;
        case MaterialGroup::LeatherSuede:
            return kLeatherSuedeCostFactor;
        case MaterialGroup::SheepskinVelour:
            return kSheepskinVelourCostFactor;
        case MaterialGroup::NaturalFur:
            return kFurCostFactor;
    }
    throw InvalidInputException("MaterialGroup", "неизвестное значение");
}

double material_time_k(MaterialGroup group) {
    switch (group) {
        case MaterialGroup::Basic:
            return kBasicMaterialTimeFactor;
        case MaterialGroup::Delicate:
            return kDelicateMaterialTimeFactor;
        case MaterialGroup::LeatherSuede:
            return kLeatherSuedeTimeFactor;
        case MaterialGroup::SheepskinVelour:
            return kSheepskinVelourTimeFactor;
        case MaterialGroup::NaturalFur:
            return kFurTimeFactor;
    }
    throw InvalidInputException("MaterialGroup", "неизвестное значение");
}

double soil_k(SoilLevel level) {
    switch (level) {
        case SoilLevel::Light:
            return kLightSoilCostFactor;
        case SoilLevel::Medium:
            return kMediumSoilCostFactor;
        case SoilLevel::Heavy:
            return kHeavySoilCostFactor;
    }
    throw InvalidInputException("SoilLevel", "неизвестное значение");
}

double stain_surcharge(StainClass stain_class) {
    switch (stain_class) {
        case StainClass::Light:
            return kLightStainSurcharge;
        case StainClass::Hard:
            return kHardStainSurcharge;
        case StainClass::Special:
            return kSpecialStainSurcharge;
    }
    throw InvalidInputException("StainClass", "неизвестное значение");
}

double urgency_surcharge(bool urgent) { return urgent ? kUrgentSurcharge : kRegularSurcharge; }

double urgency_time_k(bool urgent) { return urgent ? kUrgentTimeK : kRegularTimeK; }
