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
    void update_buttons() override;

private:
    void show_history();
    void service_selected();
    QPushButton* history_button_;
    QPushButton* service_button_;
};
