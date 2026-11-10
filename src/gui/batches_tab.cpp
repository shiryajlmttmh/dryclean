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

constexpr std::array<const char*, BatchColumnCount> kBatchHeaders{
    "№", "Машина", "Услуга", "Изделия", "Масса, кг", "Загрузка", "Начало", "Готовность", "Статус", "Завершена"};

}

BatchesTab::BatchesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id) {
    complete_button_ = new QPushButton(QStringLiteral("Завершить партию"), this);
    table_ = new QTableWidget(this);
    configure_table(table_, make_headers(kBatchHeaders));

    auto* toolbar = new QHBoxLayout;
    toolbar->addStretch(1);
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
        set_cell(table_, row, BatchStatusName, qt_convert::text(display_name(batch.status())));
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
