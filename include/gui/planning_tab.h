#pragma once

#include <QWidget>
#include <optional>

#include "shop.h"

class QLabel;
class QPushButton;
class QTableWidget;

class PlanningTab : public QWidget {
    Q_OBJECT

public:
    PlanningTab(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

public slots:
    void refresh();

signals:
    void changed();

private slots:
    void build();
    void validate();
    void confirm();

private:
    bool is_stale() const;
    void show_batches();
    void show_unplaced();
    void show_conflicts();
    void show_forecasts();
    void show_summary();

    DryCleaningShop& shop_;
    Id employee_id_;
    std::optional<Plan> plan_;
    QPushButton* build_button_;
    QPushButton* validate_button_;
    QPushButton* confirm_button_;
    QLabel* summary_label_;
    QTableWidget* batches_table_;
    QTableWidget* unplaced_table_;
    QTableWidget* conflicts_table_;
    QTableWidget* forecasts_table_;
};
