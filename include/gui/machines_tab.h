#pragma once

#include "gui/table_tab.h"

class MachinesTab : public TableTab {
public:
    MachinesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

protected:
    void fill(TableWidget* table) override;
    void add_record() override;
    void edit_record(Id id) override;
    void remove_record(Id id) override;
    void add_extra_actions(QHBoxLayout* actions, Id id) override;
    QString removal_block(Id id) const override;

private:
    void show_history(Id id);
    void service_machine(Id id);
};
