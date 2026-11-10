#pragma once

#include <QString>
#include <functional>
#include <optional>
#include <vector>

#include "shop.h"

class QGroupBox;
class QTableWidget;
class QWidget;

bool run_guarded(QWidget* parent, const std::function<void()>& action);
bool has_permission(const DryCleaningShop& shop, Id employee_id, Permission permission);
std::optional<Id> selected_id(const QTableWidget* table, int id_column);
QString item_tags(const DryCleaningShop& shop, const std::vector<Id>& item_ids);
QString machine_name(const DryCleaningShop& shop, Id machine_id);
QGroupBox* make_titled_box(const QString& title, QWidget* content, QWidget* parent);
