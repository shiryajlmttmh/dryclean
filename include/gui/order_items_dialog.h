#pragma once

#include <QDialog>

#include "shop.h"

class QLabel;
class TableWidget;

class OrderItemsDialog : public QDialog {
    Q_OBJECT

public:
    OrderItemsDialog(DryCleaningShop& shop, Id employee_id, Id order_id, QWidget* parent = nullptr);

private:
    void refresh();
    void edit_item(Id id);
    bool can_edit() const;

    DryCleaningShop& shop_;
    Id employee_id_;
    Id order_id_;
    QLabel* summary_label_;
    TableWidget* table_;
    QLabel* edit_hint_;
};
