#pragma once

#include <QWidget>

#include "shop.h"

class QTabBar;
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
    void fill_row(int row, const Batch& batch, TimePoint now);

    DryCleaningShop& shop_;
    Id employee_id_;
    QTabBar* filter_tabs_;
    QPushButton* complete_button_;
    QTableWidget* table_;
};
