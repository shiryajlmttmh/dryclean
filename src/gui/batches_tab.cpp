#include "gui/batches_tab.h"

#include <QTabBar>
#include <QToolButton>
#include <cmath>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <array>
#include <algorithm>
#include <vector>

#include "display.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kWeightDecimals = 2;
constexpr const char* kInProgressText = "В работе";
constexpr const char* kAwaitingText = "Ждёт завершения";
constexpr int kAllBatches = -1;

enum BatchColumn {
    BatchId,
    BatchMachine,
    BatchService,
    BatchItems,
    BatchWeight,
    BatchFill,
    BatchStart,
    BatchReady,
    BatchStatusName,
    BatchCompleted,
    BatchColumnCount
};

QString status_text(const Batch& batch, TimePoint now) {
    if (batch.status() == BatchStatus::Scheduled && now >= batch.planned_ready_at()) {
        return QString::fromUtf8(kAwaitingText);
    }
    if (batch.status() == BatchStatus::Scheduled && now >= batch.planned_start_at()) {
        return QString::fromUtf8(kInProgressText);
    }
    return qt_convert::text(display_name(batch.status()));
}

bool matches_filter(const Batch& batch, int filter) {
    return filter == kAllBatches || static_cast<int>(batch.status()) == filter;
}

constexpr std::array<const char*, BatchColumnCount> kBatchHeaders{
    "№", "Машина", "Услуга", "Изделия", "Масса", "Загрузка", "Начало", "Конец", "Статус", "Завершена"};

QString short_time(TimePoint time) {
    return QDateTime::fromMSecsSinceEpoch(to_epoch_ms(time)).toString(QStringLiteral("dd.MM HH:mm"));
}

void set_batch_time(TableWidget* table, int row, int column, std::optional<TimePoint> time, const QString& label) {
    set_sortable_cell(table, row, column, label, time_sort_value(time));
    table->item(row, column)->setToolTip(qt_convert::format_time(time));
}

}

BatchesTab::BatchesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id) {
    filter_tabs_ = new QTabBar(this);
    filter_tabs_->setExpanding(false);
    filter_tabs_->setTabData(filter_tabs_->addTab(QStringLiteral("Запланированные и в работе")),
                             static_cast<int>(BatchStatus::Scheduled));
    filter_tabs_->setTabData(filter_tabs_->addTab(QStringLiteral("Завершённые")),
                             static_cast<int>(BatchStatus::Completed));
    filter_tabs_->setTabData(filter_tabs_->addTab(QStringLiteral("Все партии")), kAllBatches);
    table_ = new TableWidget(make_headers(kBatchHeaders), this, BatchItems, true);
    table_->add_stretch_column(BatchMachine);
    table_->add_stretch_column(BatchService);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(filter_tabs_);
    layout->addWidget(table_);

    connect(filter_tabs_, &QTabBar::currentChanged, this, &BatchesTab::refresh);
    refresh();
}

void BatchesTab::refresh() {
    const int filter = filter_tabs_->tabData(filter_tabs_->currentIndex()).toInt();
    const std::optional<Id> selected = selected_id(table_, BatchId);
    std::vector<const Batch*> shown;
    for (const Batch& batch : shop_.batches()) {
        if (matches_filter(batch, filter)) {
            shown.push_back(&batch);
        }
    }
    std::stable_sort(shown.begin(), shown.end(), [](const Batch* left, const Batch* right) {
        return left->planned_start_at() < right->planned_start_at();
    });
    const QSignalBlocker blocker(table_);
    table_->set_empty_text(shop_.batches().empty()
        ? QStringLiteral("Партий пока нет. Они появятся после подтверждения плана.")
        : QStringLiteral("Для выбранного фильтра партий нет."));
    const bool has_scheduled = std::any_of(shown.begin(), shown.end(), [](const Batch* batch) {
        return batch->status() == BatchStatus::Scheduled;
    });
    table_->setColumnHidden(table_->actions_column(),
                            !has_scheduled || !has_permission(shop_, employee_id_, Permission::ChangeStatus));
    table_->reset_rows(static_cast<int>(shown.size()));
    const TimePoint now = qt_convert::now();
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
    set_batch_time(table_, row, BatchStart, batch.planned_start_at(), short_time(batch.planned_start_at()));
    const qint64 hours_left = static_cast<qint64>(std::ceil(hours_between(batch.planned_ready_at(), now)));
    const QString end = batch.status() == BatchStatus::Completed || now >= batch.planned_ready_at()
        ? short_time(batch.planned_ready_at())
        : QStringLiteral("Через %1 ч").arg(hours_left);
    set_batch_time(table_, row, BatchReady, batch.planned_ready_at(), end);
    set_cell(table_, row, BatchStatusName, status_text(batch, now));
    const auto completed_at = batch.completed_at();
    set_batch_time(table_, row, BatchCompleted, completed_at,
                   completed_at ? short_time(*completed_at) : QStringLiteral("—"));
    if (!table_->isColumnHidden(table_->actions_column()) && batch.status() == BatchStatus::Scheduled) {
        const Id id = batch.id();
        const QString unavailable = now < batch.planned_ready_at()
            ? QStringLiteral("Завершить партию можно после %1.").arg(qt_convert::format_time(batch.planned_ready_at()))
            : QString();
        auto* action = add_row_action(make_row_actions(table_, row, id), QStringLiteral("Завершить"),
                                      [this, id] { complete(id); }, unavailable);
        action->setAccessibleName(QStringLiteral("Завершить партию №%1").arg(id));
        if (unavailable.isEmpty()) action->setToolTip(action->accessibleName());
    }
}

void BatchesTab::complete(Id id) {
    if (run_guarded(this, [&] { shop_.complete_batch(id, employee_id_, qt_convert::now()); })) {
        emit changed();
    }
}
