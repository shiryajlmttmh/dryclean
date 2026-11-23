#include "gui/batches_tab.h"

#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

#include "display.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kWeightDecimals = 2;
constexpr auto kTimestampWidthSample = "88.88.8888 88:88";
constexpr auto kCountdownWidthSample = std::chrono::hours(88) + std::chrono::minutes(58);
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

QDateTime local_date_time(TimePoint time) {
    return QDateTime::fromMSecsSinceEpoch(to_epoch_ms(time));
}

QString format_batch_time(TimePoint time, int current_year) {
    const auto local = local_date_time(time);
    return local.toString(local.date().year() == current_year ? QStringLiteral("dd.MM HH:mm")
                                                              : QStringLiteral("dd.MM.yyyy HH:mm"));
}

QString format_countdown(std::chrono::milliseconds remaining) {
    using namespace std::chrono;
    const auto total_minutes = duration_cast<minutes>(remaining);
    if (total_minutes == minutes::zero()) return QStringLiteral("Осталось меньше минуты");
    const auto whole_hours = duration_cast<hours>(total_minutes);
    const auto remaining_minutes = total_minutes - whole_hours;
    if (whole_hours == hours::zero()) return QStringLiteral("Осталось %1 мин").arg(remaining_minutes.count());
    if (remaining_minutes == minutes::zero()) return QStringLiteral("Осталось %1 ч").arg(whole_hours.count());
    return QStringLiteral("Осталось %1 ч %2 мин").arg(whole_hours.count()).arg(remaining_minutes.count());
}

QString earliest_batch_time(const QString& date) { return QStringLiteral("Не раньше %1").arg(date); }

QString batch_time_with_countdown(const QString& date, const QString& countdown) {
    return QStringLiteral("%1 · %2").arg(countdown, date);
}

void set_batch_time(TableWidget* table, int row, int column, TimePoint time, const QString& kind, TimePoint planned,
                    int current_year, bool lower_bound, const QString& countdown = {}) {
    const auto date = format_batch_time(time, current_year);
    const auto moment = lower_bound ? earliest_batch_time(date) : date;
    const auto displayed = countdown.isEmpty() ? moment : batch_time_with_countdown(moment, countdown);
    set_sortable_cell(table, row, column, displayed, time_sort_value(time));
    table->item(row, column)->setToolTip(QStringLiteral("%1: %2\nПо плану: %3")
        .arg(kind, moment, format_batch_time(planned, current_year)));
}

}

BatchesTab::BatchesTab(DryCleaningShop& shop, Id employee_id, const ShopForecast& forecast, QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id), forecast_(forecast) {
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
    read_only_hint_ = new QLabel(QStringLiteral("Для запуска и завершения партий нужны права оператора."), this);
    read_only_hint_->setWordWrap(true);
    table_ = new TableWidget(make_headers(kBatchHeaders), this, true);
    table_->set_text_columns({BatchMachine, BatchService, BatchItems});
    const auto timestamp_sample = QString::fromLatin1(kTimestampWidthSample);
    table_->reserve_column_text(BatchStart, {earliest_batch_time(timestamp_sample)});
    table_->reserve_column_text(BatchEnd, {
        batch_time_with_countdown(timestamp_sample, format_countdown(kCountdownWidthSample)),
        batch_time_with_countdown(timestamp_sample, format_countdown(std::chrono::seconds(1)))
    });
    table_->reserve_column_text(BatchStatusName, {qt_convert::text(display_name(BatchStatus::AwaitingConfirmation))});
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(filter_tabs_);
    layout->addWidget(stage_filter_, 0, Qt::AlignLeft);
    layout->addWidget(read_only_hint_);
    layout->addWidget(table_);

    connect(filter_tabs_, &QTabBar::currentChanged, this, &BatchesTab::refresh);
    connect(stage_filter_, &QComboBox::currentIndexChanged, this, &BatchesTab::refresh);
    refresh();
}

void BatchesTab::refresh() {
    const int filter = filter_tabs_->currentIndex();
    stage_filter_->setVisible(filter == Unfinished);
    const int stage = stage_filter_->currentData().toInt();
    const std::optional<Id> selected = selected_id(table_, BatchId);
    std::vector<const Batch*> shown;
    for (const Batch& batch : shop_.batches()) {
        const BatchStatus status = forecast_.batches.at(batch.id()).status;
        if (matches_filter(status, filter, stage)) shown.push_back(&batch);
    }
    std::stable_sort(shown.begin(), shown.end(), [this](const Batch* left, const Batch* right) {
        const auto left_end = left->completed_at();
        const auto right_end = right->completed_at();
        if (left_end && right_end) return *left_end > *right_end;
        if (left_end || right_end) return !left_end;
        return forecast_.batches.at(left->id()).end < forecast_.batches.at(right->id()).end;
    });
    const QSignalBlocker blocker(table_);
    table_->set_empty_text(shop_.batches().empty()
        ? QStringLiteral("Партий пока нет. Они появятся после подтверждения плана.")
        : QStringLiteral("Для выбранного фильтра партий нет."));
    const bool has_unfinished = std::any_of(shown.begin(), shown.end(), [](const Batch* batch) {
        return batch->status() != BatchStatus::Completed;
    });
    const bool may_process = has_permission(shop_, employee_id_, Permission::ChangeStatus);
    table_->setColumnHidden(table_->actions_column(), !has_unfinished || !may_process);
    read_only_hint_->setVisible(!may_process && filter != Completed);
    table_->reset_rows(static_cast<int>(shown.size()));
    const int current_year = local_date_time(forecast_.now).date().year();
    int row = 0;
    for (const Batch* batch : shown) {
        fill_row(row++, *batch, current_year);
    }
    table_->finish_rows();
    restore_selected_id(table_, BatchId, selected);
}

void BatchesTab::fill_row(int row, const Batch& batch, int current_year) {
    set_cell(table_, row, BatchId, batch.id());
    set_cell(table_, row, BatchMachine, machine_name(shop_, batch.machine_id()));
    set_cell(table_, row, BatchService, qt_convert::text(display_name(batch.service_type())));
    set_cell(table_, row, BatchItems, item_tags(shop_, batch.item_ids()));
    set_sortable_cell(table_, row, BatchWeight,
                      QStringLiteral("%1 кг").arg(batch.total_weight_kg(), 0, 'f', kWeightDecimals),
                      batch.total_weight_kg(), true);
    set_sortable_cell(table_, row, BatchFill, qt_convert::text(format_percent(batch.fill_ratio())),
                      batch.fill_ratio(), true);
    const auto& projection = forecast_.batches.at(batch.id());
    const auto status = projection.status;
    set_batch_time(table_, row, BatchStart, projection.start, batch.started_at()
        ? QStringLiteral("Фактическое начало") : QStringLiteral("Прогноз начала"),
        batch.planned_start_at(), current_year, projection.lower_bound);
    const auto countdown = status == BatchStatus::InProgress ? format_countdown(projection.end - forecast_.now) : QString();
    set_batch_time(table_, row, BatchEnd, projection.end, batch.completed_at()
        ? QStringLiteral("Подтверждение завершения") : QStringLiteral("Расчётный конец цикла"),
        batch.planned_ready_at(), current_year, projection.lower_bound, countdown);
    if (batch.completed_at()) {
        auto* end = table_->item(row, BatchEnd);
        end->setToolTip(end->toolTip() + QStringLiteral("\nРасчётный конец цикла: %1")
                                          .arg(format_batch_time(batch.expected_ready_at(), current_year)));
    }
    set_cell(table_, row, BatchStatusName, qt_convert::text(display_name(status)));
    if (table_->isColumnHidden(table_->actions_column()) || status == BatchStatus::Completed) return;

    const Id id = batch.id();
    auto* actions = make_row_actions(table_, row, id);
    if (status == BatchStatus::Scheduled) {
        auto* action = add_row_action(actions, QStringLiteral("Начать обработку"), [this, id] { start(id); },
                                      qt_convert::text(shop_.batch_start_block(id, forecast_.now)));
        action->setAccessibleName(QStringLiteral("Начать обработку партии №%1").arg(id));
    } else {
        const QString unavailable = status == BatchStatus::InProgress
            ? QStringLiteral("Конец цикла в %1.").arg(format_batch_time(projection.end, current_year))
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
