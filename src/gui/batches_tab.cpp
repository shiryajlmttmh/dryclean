#include "gui/batches_tab.h"

#include <QComboBox>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <array>
#include <algorithm>
#include <limits>
#include <vector>

#include "display.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kWeightDecimals = 2;
enum BatchFilter { Unfinished, Completed, AllBatches };
constexpr int kAllStages = -1;

enum BatchColumn {
    BatchId,
    BatchMachine,
    BatchService,
    BatchItems,
    BatchWeight,
    BatchFill,
    BatchStart,
    BatchEnd,
    BatchStatusName,
    BatchColumnCount
};

bool matches_filter(BatchStatus status, int filter, int stage) {
    if (filter == AllBatches) return true;
    if (filter == Completed) return status == BatchStatus::Completed;
    return status != BatchStatus::Completed && (stage == kAllStages || static_cast<int>(status) == stage);
}

constexpr std::array<const char*, BatchColumnCount> kBatchHeaders{
    "№", "Машина", "Услуга", "Изделия", "Масса", "Загрузка", "Начало", "Окончание", "Статус"};

QString short_time(TimePoint time) {
    return QDateTime::fromMSecsSinceEpoch(to_epoch_ms(time)).toString(QStringLiteral("dd.MM HH:mm"));
}

void set_batch_time(TableWidget* table, int row, int column, TimePoint time, const QString& kind) {
    set_sortable_cell(table, row, column, short_time(time), time_sort_value(time));
    table->item(row, column)->setToolTip(QStringLiteral("%1: %2").arg(kind, qt_convert::format_time(time)));
}

}

BatchesTab::BatchesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id) {
    filter_tabs_ = new QTabBar(this);
    filter_tabs_->setExpanding(false);
    filter_tabs_->addTab(QStringLiteral("Незавершённые"));
    filter_tabs_->addTab(QStringLiteral("Завершённые"));
    filter_tabs_->addTab(QStringLiteral("Все партии"));
    stage_filter_ = new QComboBox(this);
    stage_filter_->setAccessibleName(QStringLiteral("Этап обработки партии"));
    stage_filter_->addItem(QStringLiteral("Все этапы"), kAllStages);
    for (BatchStatus stage : {BatchStatus::Scheduled, BatchStatus::InProgress, BatchStatus::AwaitingConfirmation}) {
        stage_filter_->addItem(qt_convert::text(display_name(stage)), static_cast<int>(stage));
    }
    table_ = new TableWidget(make_headers(kBatchHeaders), this, true);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(filter_tabs_);
    layout->addWidget(stage_filter_, 0, Qt::AlignLeft);
    layout->addWidget(table_);

    status_timer_ = new QTimer(this);
    status_timer_->setSingleShot(true);
    status_timer_->setTimerType(Qt::PreciseTimer);
    connect(status_timer_, &QTimer::timeout, this, &BatchesTab::refresh);
    connect(filter_tabs_, &QTabBar::currentChanged, this, &BatchesTab::refresh);
    connect(stage_filter_, &QComboBox::currentIndexChanged, this, &BatchesTab::refresh);
    refresh();
}

void BatchesTab::refresh() {
    const int filter = filter_tabs_->currentIndex();
    stage_filter_->setVisible(filter == Unfinished);
    const int stage = stage_filter_->currentData().toInt();
    const TimePoint now = qt_convert::now();
    const std::optional<Id> selected = selected_id(table_, BatchId);
    std::vector<const Batch*> shown;
    std::optional<TimePoint> next_transition;
    for (const Batch& batch : shop_.batches()) {
        const BatchStatus status = batch.status_at(now);
        if (matches_filter(status, filter, stage)) shown.push_back(&batch);
        if (status == BatchStatus::InProgress && (!next_transition || batch.expected_ready_at() < *next_transition)) {
            next_transition = batch.expected_ready_at();
        }
    }
    status_timer_->stop();
    if (next_transition) {
        const auto delay = (*next_transition - now).count();
        status_timer_->start(static_cast<int>(std::min<decltype(delay)>(delay, std::numeric_limits<int>::max())));
    }
    std::stable_sort(shown.begin(), shown.end(), [](const Batch* left, const Batch* right) {
        const auto left_end = left->completed_at();
        const auto right_end = right->completed_at();
        if (left_end && right_end) return *left_end > *right_end;
        if (left_end || right_end) return !left_end;
        return left->expected_ready_at() < right->expected_ready_at();
    });
    const QSignalBlocker blocker(table_);
    table_->set_empty_text(shop_.batches().empty()
        ? QStringLiteral("Партий пока нет. Они появятся после подтверждения плана.")
        : QStringLiteral("Для выбранного фильтра партий нет."));
    const bool has_unfinished = std::any_of(shown.begin(), shown.end(), [](const Batch* batch) {
        return batch->status() != BatchStatus::Completed;
    });
    table_->setColumnHidden(table_->actions_column(),
                            !has_unfinished || !has_permission(shop_, employee_id_, Permission::ChangeStatus));
    table_->reset_rows(static_cast<int>(shown.size()));
    int row = 0;
    for (const Batch* batch : shown) {
        fill_row(row++, *batch, now);
    }
    table_->finish_rows();
    restore_selected_id(table_, BatchId, selected);
}

void BatchesTab::fill_row(int row, const Batch& batch, TimePoint now) {
    set_cell(table_, row, BatchId, batch.id());
    set_cell(table_, row, BatchMachine, machine_name(shop_, batch.machine_id()));
    set_cell(table_, row, BatchService, qt_convert::text(display_name(batch.service_type())));
    set_cell(table_, row, BatchItems, item_tags(shop_, batch.item_ids()));
    set_sortable_cell(table_, row, BatchWeight,
                      QStringLiteral("%1 кг").arg(batch.total_weight_kg(), 0, 'f', kWeightDecimals),
                      batch.total_weight_kg(), true);
    set_sortable_cell(table_, row, BatchFill, qt_convert::text(format_percent(batch.fill_ratio())),
                      batch.fill_ratio(), true);
    set_batch_time(table_, row, BatchStart, batch.effective_start_at(), batch.started_at()
        ? QStringLiteral("Фактическое начало") : QStringLiteral("Плановое начало"));
    const auto completed = batch.completed_at();
    set_batch_time(table_, row, BatchEnd, batch.occupied_until(), completed
        ? QStringLiteral("Подтверждение завершения") : QStringLiteral("Расчётный конец цикла"));
    if (completed) {
        auto* end = table_->item(row, BatchEnd);
        end->setToolTip(end->toolTip() + QStringLiteral("\nРасчётный конец цикла: %1")
                                          .arg(qt_convert::format_time(batch.expected_ready_at())));
    }
    const auto status = batch.status_at(now);
    set_cell(table_, row, BatchStatusName, qt_convert::text(display_name(status)));
    if (table_->isColumnHidden(table_->actions_column()) || status == BatchStatus::Completed) return;

    const Id id = batch.id();
    auto* actions = make_row_actions(table_, row, id);
    if (status == BatchStatus::Scheduled) {
        auto* action = add_row_action(actions, QStringLiteral("Начать обработку"), [this, id] { start(id); },
                                      qt_convert::text(shop_.batch_start_block(id, now)));
        action->setAccessibleName(QStringLiteral("Начать обработку партии №%1").arg(id));
    } else {
        const QString unavailable = status == BatchStatus::InProgress
            ? QStringLiteral("Завершить партию можно после %1.").arg(qt_convert::format_time(batch.expected_ready_at()))
            : QString();
        auto* action = add_row_action(actions, QStringLiteral("Завершить партию"), [this, id] { complete(id); }, unavailable);
        action->setAccessibleName(QStringLiteral("Завершить партию №%1").arg(id));
    }
}

void BatchesTab::start(Id id) {
    if (run_guarded(this, [&] { shop_.start_batch(id, employee_id_, qt_convert::now()); })) emit changed();
}

void BatchesTab::complete(Id id) {
    if (run_guarded(this, [&] { shop_.complete_batch(id, employee_id_, qt_convert::now()); })) {
        emit changed();
    }
}
