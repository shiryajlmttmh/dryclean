#pragma once

#include <functional>

#include "gui/table_tab.h"

class QTabBar;
class QLineEdit;

class MaterialsTab : public TableTab {
public:
    MaterialsTab(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

protected:
    void fill(TableWidget* table) override;
    void add_record() override;
    void edit_record(Id id) override;
    void remove_record(Id id) override;
    void add_extra_actions(QHBoxLayout* actions, Id id) override;

private:
    void restock(Id id);
    void write_off(Id id);
    void change_stock(Id id, const QString& title, const QString& prompt,
                      const std::function<void(Id, double)>& apply);

    QLineEdit* search_edit_;
    QTabBar* sort_tabs_;
};
