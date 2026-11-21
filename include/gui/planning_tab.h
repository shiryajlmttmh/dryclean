#pragma once

#include <QWidget>
#include <optional>

#include "shop.h"

class QLabel;
class QPushButton;
class TableWidget;
class QToolButton;
class QVBoxLayout;

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
    void confirm();

private:
    void show_queue(const std::vector<PlanItem>& candidates, const Plan* preview);
    void show_batches(const Plan* preview);
    void show_details(const Plan* preview);
    void update_controls(const Plan* preview, const QString& stale);

    DryCleaningShop& shop_;
    Id employee_id_;
    std::optional<Plan> plan_;
    std::optional<Revision> rendered_revision_;
    bool rendered_preview_ = false;
    std::size_t candidate_count_ = 0;
    std::optional<Revision> confirmed_revision_;
    std::size_t confirmed_batches_ = 0;
    std::map<Id, std::string> confirmed_unplaced_;
    QPushButton* build_button_;
    QPushButton* confirm_button_;
    QLabel* summary_label_;
    QVBoxLayout* queue_cards_;
    QVBoxLayout* batch_cards_;
    QToolButton* details_button_;
    QWidget* details_;
    TableWidget* conflicts_table_;
    TableWidget* forecasts_table_;
};
