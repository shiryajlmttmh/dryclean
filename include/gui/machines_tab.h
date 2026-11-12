#pragma once

#include "gui/table_tab.h"

class MachinesTab : public TableTab {
public:
    MachinesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

protected:
    void fill(QTableWidget* table) override;
    void add_record() override;
    void edit_record(Id id) override;
    void remove_record(Id id) override;
};
