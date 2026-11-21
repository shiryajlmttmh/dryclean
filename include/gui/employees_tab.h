#pragma once

#include "gui/table_tab.h"

class EmployeesTab : public TableTab {
public:
    EmployeesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

protected:
    void fill(TableWidget* table) override;
    void add_record() override;
    void edit_record(Id id) override;
    void remove_record(Id id) override;
    QString removal_block(Id id) const override;
};
