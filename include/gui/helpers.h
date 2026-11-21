#pragma once

#include <QString>
#include <functional>
#include <optional>
#include <vector>

#include "shop.h"

inline constexpr int kStretch = 1;
inline constexpr const char* kErrorStyle = "color: #b00020;";

class QDoubleSpinBox;
class QTableWidget;
class QWidget;

bool run_guarded(QWidget* parent, const std::function<void()>& action);
bool confirm(QWidget* parent, const QString& title, const QString& text);
bool has_permission(const DryCleaningShop& shop, Id employee_id, Permission permission);
std::optional<Id> selected_id(const QTableWidget* table, int id_column);
void restore_selected_id(QTableWidget* table, int id_column, std::optional<Id> id);
QString item_tag(const DryCleaningShop& shop, Id item_id);
QString item_services_text(const Item& item);
QString item_tags(const DryCleaningShop& shop, const std::vector<Id>& item_ids);
QString machine_name(const DryCleaningShop& shop, Id machine_id);
QWidget* make_titled_box(const QString& title, QWidget* content, QWidget* parent);
QDoubleSpinBox* make_spin_box(QWidget* parent, double minimum, double maximum, int decimals, double step,
                              const QString& suffix, double value);
