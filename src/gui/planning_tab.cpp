#include "gui/planning_tab.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <array>

#include "display.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kWeightDecimals = 2;
constexpr int kObjectiveDecimals = 2;

enum BatchColumn {
    BatchMachine,
    BatchService,
    BatchItems,
    BatchWeight,
    BatchFill,
    BatchStart,
    BatchReady,
    BatchColumnCount
};

constexpr std::array<const char*, BatchColumnCount> kBatchHeaders{"Машина", "Услуга", "Изделия", "Масса, кг",
                                                                  "Загрузка", "Начало", "Готовность"};

enum UnplacedColumn { UnplacedItem, UnplacedReason, UnplacedColumnCount };

constexpr std::array<const char*, UnplacedColumnCount> kUnplacedHeaders{"Изделие", "Причина"};

enum ForecastColumn { ForecastOrder, ForecastDeadline, ForecastReady, ForecastVerdict, ForecastColumnCount };

constexpr std::array<const char*, ForecastColumnCount> kForecastHeaders{"Заказ №", "Срок", "Прогноз готовности",
                                                                        "Оценка"};

QTableWidget* make_table(const QStringList& headers, QWidget* parent) {
    auto* table = new QTableWidget(parent);
    configure_table(table, headers);
    return table;
}

}

PlanningTab::PlanningTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id) {
    build_button_ = new QPushButton(QStringLiteral("Построить план"), this);
    confirm_button_ = new QPushButton(QStringLiteral("Подтвердить план"), this);
    summary_label_ = new QLabel(this);
    summary_label_->setWordWrap(true);

    batches_table_ = make_table(make_headers(kBatchHeaders), this);
    unplaced_table_ = make_table(make_headers(kUnplacedHeaders), this);
    forecasts_table_ = make_table(make_headers(kForecastHeaders), this);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(build_button_);
    toolbar->addWidget(confirm_button_);
    toolbar->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(summary_label_);
    layout->addWidget(make_titled_box(QStringLiteral("Партии плана"), batches_table_, this), 2);
    layout->addWidget(make_titled_box(QStringLiteral("Не размещено"), unplaced_table_, this), 1);
    layout->addWidget(make_titled_box(QStringLiteral("Прогноз по заказам"), forecasts_table_, this), 2);

    connect(build_button_, &QPushButton::clicked, this, &PlanningTab::build);
    connect(confirm_button_, &QPushButton::clicked, this, &PlanningTab::confirm);
    refresh();
}

bool PlanningTab::is_stale() const { return plan_ && plan_->source_revision != shop_.revision(); }

void PlanningTab::refresh() {
    show_summary();
    show_batches();
    show_unplaced();
    show_forecasts();
    build_button_->setEnabled(has_permission(shop_, employee_id_, Permission::BuildPlan));
    confirm_button_->setEnabled(plan_ && !plan_->batches.empty() && !is_stale() &&
                                has_permission(shop_, employee_id_, Permission::ConfirmPlan));
}

void PlanningTab::show_summary() {
    if (!plan_) {
        summary_label_->setText(QStringLiteral("План не построен."));
        return;
    }
    QString text = QStringLiteral("Партий: %1. Не размещено изделий: %2. Средняя загрузка: %3. Опозданий: %4. "
                                  "Целевая функция: %5.")
                       .arg(plan_->batches.size())
                       .arg(plan_->unplaced.size())
                       .arg(qt_convert::text(format_percent(plan_->average_load)))
                       .arg(plan_->late_count)
                       .arg(plan_->objective, 0, 'f', kObjectiveDecimals);
    if (is_stale()) {
        text += QStringLiteral("\nДанные изменились после построения плана — постройте его заново.");
    }
    summary_label_->setText(text);
}

void PlanningTab::show_batches() {
    const std::size_t count = plan_ ? plan_->batches.size() : 0;
    batches_table_->setRowCount(static_cast<int>(count));
    for (std::size_t index = 0; index < count; ++index) {
        const PlannedBatch& batch = plan_->batches[index];
        const int row = static_cast<int>(index);
        set_cell(batches_table_, row, BatchMachine, machine_name(shop_, batch.machine_id));
        set_cell(batches_table_, row, BatchService, qt_convert::text(display_name(batch.service_type)));
        set_cell(batches_table_, row, BatchItems, item_tags(shop_, batch.item_ids));
        set_cell(batches_table_, row, BatchWeight, QString::number(batch.total_weight_kg, 'f', kWeightDecimals), true);
        set_cell(batches_table_, row, BatchFill, qt_convert::text(format_percent(batch.fill_ratio)), true);
        set_cell(batches_table_, row, BatchStart, qt_convert::format_time(batch.planned_start_at));
        set_cell(batches_table_, row, BatchReady, qt_convert::format_time(batch.planned_ready_at));
    }
}

void PlanningTab::show_unplaced() {
    const std::size_t count = plan_ ? plan_->unplaced.size() : 0;
    unplaced_table_->setRowCount(static_cast<int>(count));
    for (std::size_t index = 0; index < count; ++index) {
        const auto& [item_id, reason] = plan_->unplaced[index];
        const int row = static_cast<int>(index);
        set_cell(unplaced_table_, row, UnplacedItem, item_tags(shop_, {item_id}));
        set_cell(unplaced_table_, row, UnplacedReason, qt_convert::text(reason));
    }
}

void PlanningTab::show_forecasts() {
    forecasts_table_->setRowCount(plan_ ? static_cast<int>(plan_->order_ready_at.size()) : 0);
    if (!plan_) {
        return;
    }
    int row = 0;
    for (const auto& [order_id, ready_at] : plan_->order_ready_at) {
        const Order* order = shop_.orders().find_by_id(order_id);
        QString verdict = QStringLiteral("нет прогноза");
        if (ready_at && order != nullptr) {
            verdict = *ready_at > order->deadline() ? QStringLiteral("опоздание") : QStringLiteral("в срок");
        }
        set_cell(forecasts_table_, row, ForecastOrder, QString::number(order_id), true);
        set_cell(forecasts_table_, row, ForecastDeadline,
                 order == nullptr ? QStringLiteral("?") : qt_convert::format_time(order->deadline()));
        set_cell(forecasts_table_, row, ForecastReady, qt_convert::format_time(ready_at));
        set_cell(forecasts_table_, row, ForecastVerdict, verdict);
        ++row;
    }
}

void PlanningTab::build() {
    std::optional<Plan> built;
    if (run_guarded(this, [&] { built = shop_.build_plan(qt_convert::now(), employee_id_); })) {
        plan_ = std::move(built);
    }
    refresh();
}

void PlanningTab::confirm() {
    std::vector<Id> created;
    if (!run_guarded(this, [&] { created = shop_.confirm_plan(*plan_, employee_id_, qt_convert::now()); })) {
        refresh();
        return;
    }
    plan_.reset();
    emit changed();
    QMessageBox::information(this, QStringLiteral("План подтверждён"),
                             QStringLiteral("Создано партий: %1.").arg(created.size()));
}
