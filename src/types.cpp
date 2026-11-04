#include "types.h"

#include <array>
#include <cmath>
#include <ratio>

#include "exceptions.h"

namespace {

constexpr std::array<const char*, 4> kService{"DryCleaning", "WetCleaning", "StainRemoval", "Leather"};
constexpr std::array<const char*, 5> kMaterial{"Basic", "Delicate", "LeatherSuede", "SheepskinVelour",
                                               "NaturalFur"};
constexpr std::array<const char*, 3> kFabric{"Dense", "Delicate", "LeatherFur"};
constexpr std::array<const char*, 2> kColor{"Light", "Dark"};
constexpr std::array<const char*, 3> kSoil{"Light", "Medium", "Heavy"};
constexpr std::array<const char*, 3> kStain{"Light", "Hard", "Special"};
constexpr std::array<const char*, 4> kOrder{"Accepted", "InProgress", "Ready", "Issued"};
constexpr std::array<const char*, 2> kBatch{"Scheduled", "Completed"};
constexpr std::array<const char*, 3> kRole{"Receptionist", "Operator", "Administrator"};
constexpr std::array<const char*, 8> kPerm{"ManageClients", "AcceptOrders", "IssueOrders", "ChangeStatus",
                                           "BuildPlan",     "ConfirmPlan",  "ManageStock", "ManageStaff"};

template <class E, std::size_t N>
std::string enum_to_string(E value, const std::array<const char*, N>& names, const char* type_name) {
    const auto index = static_cast<std::size_t>(value);
    if (index >= N) {
        throw InvalidInputException(type_name, "значение перечисления вне допустимого диапазона");
    }
    return names[index];
}

template <class E, std::size_t N>
E enum_from_string(const std::string& text, const std::array<const char*, N>& names, const char* type_name) {
    for (std::size_t i = 0; i < N; ++i) {
        if (text == names[i]) {
            return static_cast<E>(i);
        }
    }
    throw InvalidInputException(type_name, "неизвестное значение: " + text);
}

}

std::string to_string(ServiceType v) { return enum_to_string(v, kService, "ServiceType"); }
std::string to_string(MaterialGroup v) { return enum_to_string(v, kMaterial, "MaterialGroup"); }
std::string to_string(FabricGroup v) { return enum_to_string(v, kFabric, "FabricGroup"); }
std::string to_string(ColorGroup v) { return enum_to_string(v, kColor, "ColorGroup"); }
std::string to_string(SoilLevel v) { return enum_to_string(v, kSoil, "SoilLevel"); }
std::string to_string(StainClass v) { return enum_to_string(v, kStain, "StainClass"); }
std::string to_string(OrderStatus v) { return enum_to_string(v, kOrder, "OrderStatus"); }
std::string to_string(BatchStatus v) { return enum_to_string(v, kBatch, "BatchStatus"); }
std::string to_string(EmployeeRole v) { return enum_to_string(v, kRole, "EmployeeRole"); }
std::string to_string(Permission v) { return enum_to_string(v, kPerm, "Permission"); }

ServiceType parse_ServiceType(const std::string& s) {
    return enum_from_string<ServiceType>(s, kService, "ServiceType");
}
MaterialGroup parse_MaterialGroup(const std::string& s) {
    return enum_from_string<MaterialGroup>(s, kMaterial, "MaterialGroup");
}
FabricGroup parse_FabricGroup(const std::string& s) {
    return enum_from_string<FabricGroup>(s, kFabric, "FabricGroup");
}
ColorGroup parse_ColorGroup(const std::string& s) {
    return enum_from_string<ColorGroup>(s, kColor, "ColorGroup");
}
SoilLevel parse_SoilLevel(const std::string& s) {
    return enum_from_string<SoilLevel>(s, kSoil, "SoilLevel");
}
StainClass parse_StainClass(const std::string& s) {
    return enum_from_string<StainClass>(s, kStain, "StainClass");
}
OrderStatus parse_OrderStatus(const std::string& s) {
    return enum_from_string<OrderStatus>(s, kOrder, "OrderStatus");
}
BatchStatus parse_BatchStatus(const std::string& s) {
    return enum_from_string<BatchStatus>(s, kBatch, "BatchStatus");
}
EmployeeRole parse_EmployeeRole(const std::string& s) {
    return enum_from_string<EmployeeRole>(s, kRole, "EmployeeRole");
}
Permission parse_Permission(const std::string& s) {
    return enum_from_string<Permission>(s, kPerm, "Permission");
}

bool is_valid(EmployeeRole role) noexcept { return static_cast<std::size_t>(role) < kRole.size(); }

namespace {

using FractionalHours = std::chrono::duration<double, std::ratio<3600>>;
using FractionalMillis = std::chrono::duration<double, std::milli>;

}

Hours hours_between(TimePoint end, TimePoint begin) { return FractionalHours(end - begin).count(); }

TimePoint add_hours(TimePoint t, Hours h) {
    const double millis = FractionalMillis(FractionalHours(h)).count();
    return t + std::chrono::milliseconds(static_cast<std::int64_t>(std::ceil(millis)));
}

std::int64_t to_epoch_ms(TimePoint t) { return t.time_since_epoch().count(); }

TimePoint from_epoch_ms(std::int64_t ms) { return TimePoint(std::chrono::milliseconds(ms)); }
