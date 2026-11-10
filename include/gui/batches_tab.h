#pragma once

#include <QWidget>

#include "shop.h"

class QPushButton;
class QTableWidget;

class BatchesTab : public QWidget {
    Q_OBJECT

public:
    BatchesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

public slots:
    void refresh();

signals:
    void changed();

private slots:
    void complete();
    void update_buttons();

private:
    DryCleaningShop& shop_;
    Id employee_id_;
    QPushButton* complete_button_;
    QTableWidget* table_;
};
