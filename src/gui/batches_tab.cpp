#include "gui/batches_tab.h"

#include <QHBoxLayout>
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
constexpr const char* kInProgressText = "В работе";

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
    if (batch.status() == BatchStatus::Scheduled && now >= batch.planned_start_at()) {
        return QString::fromUtf8(kInProgressText);
    }
    return qt_convert::text(display_name(batch.status()));
}

constexpr std::array<const char*, BatchColumnCount> kBatchHeaders{
    "№", "Машина", "Услуга", "Изделия", "Масса, кг", "Загрузка", "Начало", "Конец обработки", "Статус", "Завершена"};

}

BatchesTab::BatchesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id) {
    complete_button_ = new QPushButton(QStringLiteral("Завершить партию"), this);
    table_ = new QTableWidget(this);
    configure_table(table_, make_headers(kBatchHeaders));

    auto* toolbar = new QHBoxLayout;
    toolbar->addStretch(kStretch);
    toolbar->addWidget(complete_button_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(table_);

    connect(complete_button_, &QPushButton::clicked, this, &BatchesTab::complete);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &BatchesTab::update_buttons);
    refresh();
}

void BatchesTab::refresh() {
    table_->setRowCount(static_cast<int>(shop_.batches().size()));
    const TimePoint now = qt_convert::now();
    int row = 0;
    for (const Batch& batch : shop_.batches()) {
        set_cell(table_, row, BatchId, QString::number(batch.id()), true);
        set_cell(table_, row, BatchMachine, machine_name(shop_, batch.machine_id()));
        set_cell(table_, row, BatchService, qt_convert::text(display_name(batch.service_type())));
        set_cell(table_, row, BatchItems, item_tags(shop_, batch.item_ids()));
        set_cell(table_, row, BatchWeight, QString::number(batch.total_weight_kg(), 'f', kWeightDecimals), true);
        set_cell(table_, row, BatchFill, qt_convert::text(format_percent(batch.fill_ratio())), true);
        set_cell(table_, row, BatchStart, qt_convert::format_time(batch.planned_start_at()));
        set_cell(table_, row, BatchReady, qt_convert::format_time(batch.planned_ready_at()));
        set_cell(table_, row, BatchStatusName, status_text(batch, now));
        set_cell(table_, row, BatchCompleted, qt_convert::format_time(batch.completed_at()));
        ++row;
    }
    update_buttons();
}

void BatchesTab::update_buttons() {
    const std::optional<Id> id = selected_id(table_, BatchId);
    const Batch* batch = id ? shop_.batches().find_by_id(*id) : nullptr;
    complete_button_->setEnabled(batch != nullptr && batch->status() == BatchStatus::Scheduled &&
                                 has_permission(shop_, employee_id_, Permission::ChangeStatus));
}

void BatchesTab::complete() {
    const std::optional<Id> id = selected_id(table_, BatchId);
    if (!id) {
        return;
    }
    if (run_guarded(this, [&] { shop_.complete_batch(*id, employee_id_, qt_convert::now()); })) {
        emit changed();
    }
}
