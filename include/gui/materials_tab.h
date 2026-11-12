#pragma once

#include <functional>

#include "gui/table_tab.h"

class QComboBox;
class QLineEdit;

class MaterialsTab : public TableTab {
public:
    MaterialsTab(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

protected:
    void fill(QTableWidget* table) override;
    void add_record() override;
    void edit_record(Id id) override;
    void remove_record(Id id) override;
    void update_buttons() override;

private:
    void restock();
    void write_off();
    void change_stock(const QString& title, const QString& prompt,
                      const std::function<void(Id, double)>& apply);

    QLineEdit* search_edit_;
    QComboBox* sort_box_;
    QPushButton* restock_button_;
    QPushButton* write_off_button_;
};
