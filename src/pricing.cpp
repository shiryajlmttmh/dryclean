#include "pricing.h"

#include <stdexcept>

double tariff(ServiceType service) {
    switch (service) {
        case ServiceType::DryCleaning:
            return 65.0;
        case ServiceType::WetCleaning:
            return 55.0;
        case ServiceType::StainRemoval:
            return 15.0;
        case ServiceType::Leather:
            return 65.0;
    }
    throw std::invalid_argument("unknown ServiceType");
}

Hours base_hours(ServiceType service) {
    switch (service) {
        case ServiceType::DryCleaning:
            return 144.0;
        case ServiceType::WetCleaning:
            return 120.0;
        case ServiceType::StainRemoval:
            return 48.0;
        case ServiceType::Leather:
            return 144.0;
    }
    throw std::invalid_argument("unknown ServiceType");
}

double material_cost_k(MaterialGroup group) {
    switch (group) {
        case MaterialGroup::Basic:
            return 1.0;
        case MaterialGroup::Delicate:
            return 1.2;
        case MaterialGroup::LeatherSuede:
            return 1.15;
        case MaterialGroup::SheepskinVelour:
            return 1.3;
        case MaterialGroup::NaturalFur:
            return 1.45;
    }
    throw std::invalid_argument("unknown MaterialGroup");
}

double material_time_k(MaterialGroup group) {
    switch (group) {
        case MaterialGroup::Basic:
            return 1.0;
        case MaterialGroup::Delicate:
            return 1.2;
        case MaterialGroup::LeatherSuede:
            return 1.3;
        case MaterialGroup::SheepskinVelour:
            return 1.3;
        case MaterialGroup::NaturalFur:
            return 1.5;
    }
    throw std::invalid_argument("unknown MaterialGroup");
}

double soil_k(SoilLevel level) {
    switch (level) {
        case SoilLevel::Light:
            return 1.0;
        case SoilLevel::Medium:
            return 1.15;
        case SoilLevel::Heavy:
            return 1.3;
    }
    throw std::invalid_argument("unknown SoilLevel");
}

double stain_surcharge(StainClass stain_class) {
    switch (stain_class) {
        case StainClass::Light:
            return 10.0;
        case StainClass::Hard:
            return 15.0;
        case StainClass::Special:
            return 20.0;
    }
    throw std::invalid_argument("unknown StainClass");
}
