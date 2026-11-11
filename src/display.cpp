#include "display.h"

#include <array>
#include <format>

#include "exceptions.h"

namespace {

constexpr double kPercent = 100.0;
constexpr const char* kCurrency = "BYN";
constexpr const char* kHoursSuffix = "ч";
constexpr const char* kPercentSuffix = "%";
constexpr const char* kTrailingZero = ".0";

constexpr std::array kService{"Сухая чистка", "Влажная чистка", "Выведение пятен", "Кожа и мех"};
constexpr std::array kMaterial{"Обычный", "Деликатный", "Кожа и замша", "Овчина и велюр",
                                               "Натуральный мех"};
constexpr std::array kFabric{"Плотная", "Деликатная", "Кожа и мех"};
constexpr std::array kColor{"Светлый", "Тёмный"};
constexpr std::array kSoil{"Лёгкое", "Среднее", "Сильное"};
constexpr std::array kStain{"Лёгкое", "Стойкое", "Особое"};
constexpr std::array kOrder{"Принят", "В обработке", "Готов", "Выдан"};
constexpr std::array kBatch{"Запланирована", "Завершена"};
constexpr std::array kRole{"Приёмщик", "Оператор", "Администратор"};

template <class E, std::size_t N>
std::string name_of(E value, const std::array<const char*, N>& names) {
    const auto index = static_cast<std::size_t>(value);
    if (index >= N) {
        throw InvalidInputException("value", "неизвестное значение перечисления");
    }
    return names[index];
}

}

std::string display_name(ServiceType value) { return name_of(value, kService); }
std::string display_name(MaterialGroup value) { return name_of(value, kMaterial); }
std::string display_name(FabricGroup value) { return name_of(value, kFabric); }
std::string display_name(ColorGroup value) { return name_of(value, kColor); }
std::string display_name(SoilLevel value) { return name_of(value, kSoil); }
std::string display_name(StainClass value) { return name_of(value, kStain); }
std::string display_name(OrderStatus value) { return name_of(value, kOrder); }
std::string display_name(BatchStatus value) { return name_of(value, kBatch); }
std::string display_name(EmployeeRole value) { return name_of(value, kRole); }

std::string format_money(double amount) { return std::format("{:.2f} {}", amount, kCurrency); }

std::string format_percent(double ratio) {
    std::string text = std::format("{:.1f}", ratio * kPercent);
    const std::string zero = kTrailingZero;
    if (text.size() > zero.size() && text.compare(text.size() - zero.size(), zero.size(), zero) == 0) {
        text.erase(text.size() - zero.size());
    }
    return text + kPercentSuffix;
}

std::string format_hours(Hours hours) { return std::format("{:.1f} {}", hours, kHoursSuffix); }
