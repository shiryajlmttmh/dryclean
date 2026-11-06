#pragma once

#include "types.h"

constexpr double kUrgentSurcharge = 0.5;
constexpr double kUrgentTimeK = 0.4;
constexpr Hours kStainHours = 0.5;
constexpr double kLightStainMaxAreaCm2 = 25.0;
constexpr double kRegularSurcharge = 0.0;
constexpr double kRegularTimeK = 1.0;
constexpr double kStainRemovalSoilK = 1.0;

double tariff(ServiceType service);
Hours base_hours(ServiceType service);
double material_cost_k(MaterialGroup group);
double material_time_k(MaterialGroup group);
double soil_k(SoilLevel level);
double stain_surcharge(StainClass stain_class);
double urgency_surcharge(bool urgent);
double urgency_time_k(bool urgent);
