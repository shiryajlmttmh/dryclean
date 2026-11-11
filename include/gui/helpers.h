#pragma once

#include <QString>
#include <functional>
#include <cstddef>
#include <optional>
#include <vector>

#include "shop.h"

inline constexpr int kStretch = 1;
inline constexpr const char* kErrorStyle = "color: #b00020;";

class QDoubleSpinBox;
class QListWidget;
class QTableWidget;
class QWidget;

bool run_guarded(QWidget* parent, const std::function<void()>& action);
bool has_permission(const DryCleaningShop& shop, Id employee_id, Permission permission);
std::optional<Id> selected_id(const QTableWidget* table, int id_column);
void set_row_position(QTableWidget* table, int row, std::size_t position);
std::optional<std::size_t> selected_position(const QTableWidget* table, std::size_t count);
void add_positioned_item(QListWidget* list, const QString& text, std::size_t position);
std::optional<std::size_t> selected_position(const QListWidget* list, std::size_t count);
QString item_tag(const DryCleaningShop& shop, Id item_id);
QString item_tags(const DryCleaningShop& shop, const std::vector<Id>& item_ids);
QString machine_name(const DryCleaningShop& shop, Id machine_id);
QWidget* make_titled_box(const QString& title, QWidget* content, QWidget* parent);
QDoubleSpinBox* make_spin_box(QWidget* parent, double minimum, double maximum, int decimals, double step,
                              const QString& suffix, double value);
