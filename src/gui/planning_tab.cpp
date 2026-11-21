#include "gui/planning_tab.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <array>

#include "display.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kWeightDecimals = 2;
constexpr int kObjectiveDecimals = 2;
constexpr int kAutoFieldWidth = 0;
constexpr int kLargeTableStretch = 2;
constexpr const char* kReasonSeparator = "; ";

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
                                                                  "Загрузка", "Начало", "До конца обработки"};

enum UnplacedColumn { UnplacedItem, UnplacedReason, UnplacedColumnCount };

constexpr std::array<const char*, UnplacedColumnCount> kUnplacedHeaders{"Изделие", "Причина"};

enum ConflictColumn { ConflictFirst, ConflictSecond, ConflictReasons, ConflictColumnCount };

constexpr std::array<const char*, ConflictColumnCount> kConflictHeaders{"Изделие", "Изделие", "Причины"};

enum ForecastColumn { ForecastOrder, ForecastDeadline, ForecastReady, ForecastVerdict, ForecastColumnCount };

constexpr std::array<const char*, ForecastColumnCount> kForecastHeaders{"Заказ №", "Срок", "Прогноз готовности",
                                                                        "Оценка"};

}

PlanningTab::PlanningTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id) {
    build_button_ = new QPushButton(QStringLiteral("Построить план"), this);
    validate_button_ = new QPushButton(QStringLiteral("Проверить план"), this);
    confirm_button_ = new QPushButton(QStringLiteral("Подтвердить план"), this);
    summary_label_ = new QLabel(this);
    summary_label_->setWordWrap(true);

    batches_table_ = new TableWidget(make_headers(kBatchHeaders), this, BatchItems);
    unplaced_table_ = new TableWidget(make_headers(kUnplacedHeaders), this);
    conflicts_table_ = new TableWidget(make_headers(kConflictHeaders), this);
    forecasts_table_ = new TableWidget(make_headers(kForecastHeaders), this);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(build_button_);
    toolbar->addWidget(validate_button_);
    toolbar->addWidget(confirm_button_);
    toolbar->addStretch(kStretch);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(summary_label_);
    layout->addWidget(make_titled_box(QStringLiteral("Партии плана"), batches_table_, this), kLargeTableStretch);
    layout->addWidget(make_titled_box(QStringLiteral("Не размещено"), unplaced_table_, this), kStretch);
    layout->addWidget(make_titled_box(QStringLiteral("Конфликты изделий"), conflicts_table_, this), kStretch);
    layout->addWidget(make_titled_box(QStringLiteral("Прогноз по заказам"), forecasts_table_, this), kLargeTableStretch);

    connect(build_button_, &QPushButton::clicked, this, &PlanningTab::build);
    connect(validate_button_, &QPushButton::clicked, this, &PlanningTab::validate);
    connect(confirm_button_, &QPushButton::clicked, this, &PlanningTab::confirm);
    refresh();
}

bool PlanningTab::is_stale() const { return plan_ && plan_->source_revision != shop_.revision(); }

void PlanningTab::refresh() {
    show_summary();
    show_batches();
    show_unplaced();
    show_conflicts();
    show_forecasts();
    build_button_->setEnabled(has_permission(shop_, employee_id_, Permission::BuildPlan));
    validate_button_->setEnabled(plan_.has_value());
    confirm_button_->setEnabled(plan_ && !plan_->batches.empty() && !is_stale() &&
                                has_permission(shop_, employee_id_, Permission::ConfirmPlan));
}

void PlanningTab::show_summary() {
    if (!plan_) {
        summary_label_->setText(has_permission(shop_, employee_id_, Permission::BuildPlan)
            ? QStringLiteral("Нажмите «Построить план», чтобы распределить изделия по машинам.")
            : QStringLiteral("Для построения плана нужны права оператора."));
        return;
    }
    QString text = QStringLiteral("Партий: %1. Не размещено изделий: %2. Средняя загрузка: %3. Опозданий: %4. "
                                  "Целевая функция: %5.")
                       .arg(plan_->batches.size())
                       .arg(plan_->unplaced.size())
                       .arg(qt_convert::text(format_percent(plan_->average_load)))
                       .arg(plan_->late_count)
                       .arg(plan_->objective, kAutoFieldWidth, 'f', kObjectiveDecimals);
    if (is_stale()) {
        text += QStringLiteral("\nДанные изменились после построения плана — постройте его заново.");
    }
    summary_label_->setText(text);
}

void PlanningTab::show_batches() {
    const TimePoint now = qt_convert::now();
    const std::size_t count = plan_ ? plan_->batches.size() : 0;
    batches_table_->reset_rows(static_cast<int>(count));
    for (std::size_t index = 0; index < count; ++index) {
        const PlannedBatch& batch = plan_->batches[index];
        const int row = static_cast<int>(index);
        set_cell(batches_table_, row, BatchMachine, machine_name(shop_, batch.machine_id));
        set_cell(batches_table_, row, BatchService, qt_convert::text(display_name(batch.service_type)));
        set_cell(batches_table_, row, BatchItems, item_tags(shop_, batch.item_ids));
        set_sortable_cell(batches_table_, row, BatchWeight,
                          QString::number(batch.total_weight_kg, 'f', kWeightDecimals), batch.total_weight_kg, true);
        set_sortable_cell(batches_table_, row, BatchFill, qt_convert::text(format_percent(batch.fill_ratio)),
                          batch.fill_ratio, true);
        set_time_cell(batches_table_, row, BatchStart, batch.planned_start_at);
        set_sortable_cell(batches_table_, row, BatchReady, qt_convert::format_time_remaining(batch.planned_ready_at, now),
                          time_sort_value(batch.planned_ready_at));
    }
    batches_table_->finish_rows();
}

void PlanningTab::show_unplaced() {
    const std::size_t count = plan_ ? plan_->unplaced.size() : 0;
    unplaced_table_->reset_rows(static_cast<int>(count));
    for (std::size_t index = 0; index < count; ++index) {
        const auto& [item_id, reason] = plan_->unplaced[index];
        const int row = static_cast<int>(index);
        set_cell(unplaced_table_, row, UnplacedItem, item_tag(shop_, item_id));
        set_cell(unplaced_table_, row, UnplacedReason, qt_convert::text(reason));
    }
    unplaced_table_->finish_rows();
}

void PlanningTab::show_conflicts() {
    conflicts_table_->reset_rows(0);
    if (!plan_) {
        return;
    }
    int row = 0;
    for (const auto& [first, neighbours] : plan_->conflicts) {
        for (const auto& [second, reasons] : neighbours) {
            if (first > second) {
                continue;
            }
            QStringList texts;
            for (const std::string& reason : reasons) {
                texts << qt_convert::text(reason);
            }
            conflicts_table_->insertRow(row);
            set_cell(conflicts_table_, row, ConflictFirst, item_tag(shop_, first));
            set_cell(conflicts_table_, row, ConflictSecond, item_tag(shop_, second));
            set_cell(conflicts_table_, row, ConflictReasons, texts.join(QString::fromUtf8(kReasonSeparator)));
            ++row;
        }
    }
    conflicts_table_->finish_rows();
}

void PlanningTab::show_forecasts() {
    forecasts_table_->reset_rows(plan_ ? static_cast<int>(plan_->order_ready_at.size()) : 0);
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
        set_cell(forecasts_table_, row, ForecastOrder, order_id);
        set_time_cell(forecasts_table_, row, ForecastDeadline, order ? std::optional(order->deadline()) : std::nullopt);
        set_time_cell(forecasts_table_, row, ForecastReady, ready_at);
        set_cell(forecasts_table_, row, ForecastVerdict, verdict);
        ++row;
    }
    forecasts_table_->finish_rows();
}

void PlanningTab::build() {
    std::optional<Plan> built;
    if (run_guarded(this, [&] { built = shop_.build_plan(qt_convert::now(), employee_id_); })) {
        plan_ = std::move(built);
    }
    refresh();
}

void PlanningTab::validate() {
    if (run_guarded(this, [&] { shop_.validate_plan(*plan_, qt_convert::now()); })) {
        QMessageBox::information(this, QStringLiteral("Проверка плана"),
                                 QStringLiteral("План корректен и может быть подтверждён."));
    }
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
