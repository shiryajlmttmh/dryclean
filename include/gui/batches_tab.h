#pragma once

#include <QWidget>

#include "shop.h"

class QTabBar;
class QComboBox;
class QTimer;
class TableWidget;

class BatchesTab : public QWidget {
    Q_OBJECT

public:
    BatchesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

public slots:
    void refresh();

signals:
    void changed();

private:
    void start(Id id);
    void complete(Id id);
    void fill_row(int row, const Batch& batch, TimePoint now);

    DryCleaningShop& shop_;
    Id employee_id_;
    QTabBar* filter_tabs_;
    QComboBox* stage_filter_;
    QTimer* status_timer_;
    TableWidget* table_;
};
