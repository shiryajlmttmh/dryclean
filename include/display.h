#pragma once

#include <string>

#include "types.h"

std::string display_name(ServiceType value);
std::string display_name(MaterialGroup value);
std::string display_name(FabricGroup value);
std::string display_name(ColorGroup value);
std::string display_name(SoilLevel value);
std::string display_name(StainClass value);
std::string display_name(OrderStatus value);
std::string display_name(BatchStatus value);
std::string display_name(EmployeeRole value);

std::string format_money(double amount);
std::string format_percent(double ratio);
std::string format_hours(Hours hours);
