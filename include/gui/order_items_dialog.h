#pragma once

#include <QDialog>

#include "shop.h"

class QLabel;
class QPushButton;
class QTableWidget;

class OrderItemsDialog : public QDialog {
    Q_OBJECT

public:
    OrderItemsDialog(DryCleaningShop& shop, Id employee_id, Id order_id, QWidget* parent = nullptr);

private slots:
    void refresh();
    void edit_item();

private:
    bool can_edit() const;

    DryCleaningShop& shop_;
    Id employee_id_;
    Id order_id_;
    QLabel* summary_label_;
    QTableWidget* table_;
    QPushButton* edit_button_;
};
