#pragma once

#include <QWidget>

#include "shop.h"

class QTabBar;
class QComboBox;
class QLabel;
class TableWidget;

class BatchesTab : public QWidget {
    Q_OBJECT

public:
    BatchesTab(DryCleaningShop& shop, Id employee_id, const ShopForecast& forecast, QWidget* parent = nullptr);

public slots:
    void refresh();

signals:
    void changed();

private:
    void start(Id id);
    void complete(Id id);
    void fill_row(int row, const Batch& batch, int current_year);

    DryCleaningShop& shop_;
    Id employee_id_;
    const ShopForecast& forecast_;
    QTabBar* filter_tabs_;
    QComboBox* stage_filter_;
    QLabel* read_only_hint_;
    TableWidget* table_;
};
