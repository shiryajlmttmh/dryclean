#pragma once

#include <chrono>
#include <cstdint>
#include <string>

using Id = std::uint32_t;
using Revision = std::uint64_t;
using TimePoint = std::chrono::sys_time<std::chrono::milliseconds>;
using Hours = double;

constexpr double EPS = 1e-9;

enum class ServiceType { DryCleaning, WetCleaning, StainRemoval, Leather };
enum class MaterialGroup { Basic, Delicate, LeatherSuede, SheepskinVelour, NaturalFur };
enum class FabricGroup { Dense, Delicate, LeatherFur };
enum class ColorGroup { Light, Dark };
enum class SoilLevel { Light, Medium, Heavy };
enum class StainClass { Light, Hard, Special };
enum class OrderStatus { Accepted, InProgress, Ready, Issued };
enum class BatchStatus { Scheduled, Completed };
enum class EmployeeRole { Receptionist, Operator, Administrator };
enum class Permission {
    ManageClients,
    AcceptOrders,
    IssueOrders,
    ChangeStatus,
    BuildPlan,
    ConfirmPlan,
    ManageStock,
    ManageStaff
};

std::string to_string(ServiceType v);
std::string to_string(MaterialGroup v);
std::string to_string(FabricGroup v);
std::string to_string(ColorGroup v);
std::string to_string(SoilLevel v);
std::string to_string(StainClass v);
std::string to_string(OrderStatus v);
std::string to_string(BatchStatus v);
std::string to_string(EmployeeRole v);
std::string to_string(Permission v);

ServiceType parse_ServiceType(const std::string& s);
MaterialGroup parse_MaterialGroup(const std::string& s);
FabricGroup parse_FabricGroup(const std::string& s);
ColorGroup parse_ColorGroup(const std::string& s);
SoilLevel parse_SoilLevel(const std::string& s);
StainClass parse_StainClass(const std::string& s);
OrderStatus parse_OrderStatus(const std::string& s);
BatchStatus parse_BatchStatus(const std::string& s);
EmployeeRole parse_EmployeeRole(const std::string& s);
Permission parse_Permission(const std::string& s);

Hours hours_between(TimePoint end, TimePoint begin);

TimePoint add_hours(TimePoint t, Hours h);

std::int64_t to_epoch_ms(TimePoint t);
TimePoint from_epoch_ms(std::int64_t ms);
