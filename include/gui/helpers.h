#pragma once

#include <QString>
#include <functional>
#include <optional>
#include <vector>

#include "shop.h"

inline constexpr const char* kErrorStyle = "color: #b00020;";

class QDoubleSpinBox;
class QGroupBox;
class QTableWidget;
class QWidget;

bool run_guarded(QWidget* parent, const std::function<void()>& action);
bool has_permission(const DryCleaningShop& shop, Id employee_id, Permission permission);
std::optional<Id> selected_id(const QTableWidget* table, int id_column);
QString item_tags(const DryCleaningShop& shop, const std::vector<Id>& item_ids);
QString machine_name(const DryCleaningShop& shop, Id machine_id);
QGroupBox* make_titled_box(const QString& title, QWidget* content, QWidget* parent);
QDoubleSpinBox* make_spin_box(QWidget* parent, double minimum, double maximum, int decimals, double step,
                              const QString& suffix, double value);
