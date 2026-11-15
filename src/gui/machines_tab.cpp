#include "gui/machines_tab.h"

#include <QTableWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

#include "display.h"
#include "gui/machine_dialog.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"
#include "gui/helpers.h"

namespace {

constexpr int kCapacityDecimals = 1;
constexpr int kMaintenanceDecimals = 1;
constexpr int kHistoryWindowWidth = 1000;
constexpr int kHistoryWindowHeight = 560;
constexpr int kTimelineHoursAhead = 24;
constexpr int kHoursPerDay = 24;
constexpr double kTimelineWidth = 900.0;
constexpr double kTimelineTop = 30.0;
constexpr double kTimelineHeight = 42.0;
constexpr double kTimelineLabelWidth = 150.0;
constexpr double kMinimumBatchWidth = 1.0;
constexpr auto kFreeTimeColor = "#86cfa5";
constexpr auto kCompletedBatchColor = "#5d9bd3";
constexpr auto kScheduledBatchColor = "#efad55";

enum HistoryColumn { HistoryBatch, HistoryStart, HistoryReady, HistoryStatus, HistoryCompleted, HistoryColumnCount };

enum MachineColumn { MachineId, MachineName, MachineType, MachineCapacity, MachineFreeAt, MachineMaintenance, MachineColumnCount };

constexpr std::array<const char*, MachineColumnCount> kMachineHeaders{"№", "Название", "Тип", "Вместимость, кг",
                                                                      "Доступность", "До ТО, ч (с учётом плана)"};
constexpr std::array<const char*, HistoryColumnCount> kHistoryHeaders{
    "Партия", "Начало", "Конец обработки", "Статус", "Подтверждение завершения"};

}

MachinesTab::MachinesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : TableTab(shop, employee_id, Permission::ManageStaff, make_headers(kMachineHeaders), parent) {
    history_button_ = new QPushButton(QStringLiteral("История занятости"), this);
    service_button_ = new QPushButton(QStringLiteral("Выполнить ТО"), this);
    toolbar_->addWidget(history_button_);
    toolbar_->addWidget(service_button_);
    connect(history_button_, &QPushButton::clicked, this, &MachinesTab::show_history);
    connect(service_button_, &QPushButton::clicked, this, &MachinesTab::service_selected);
}

void MachinesTab::update_buttons() {
    TableTab::update_buttons();
    history_button_->setEnabled(selected().has_value());
    service_button_->setEnabled(is_allowed() && selected().has_value());
}

void MachinesTab::service_selected() {
    const auto id = selected();
    if (!id || !is_allowed()) return;
    if (!confirm(this, QStringLiteral("Техобслуживание"),
                 QStringLiteral("Подтвердить выполненное ТО и обнулить счётчик часов?"))) return;
    if (run_guarded(this, [this, id] { shop_.service_machine(*id, employee_id_, qt_convert::now()); })) {
        emit changed();
    }
}

void MachinesTab::show_history() {
    const auto id = selected();
    if (!id) return;
    const Machine& machine = shop_.machines().get(*id);
    std::vector<const Batch*> history;
    for (const Batch& batch : shop_.batches()) {
        if (batch.machine_id() == *id) history.push_back(&batch);
    }
    std::sort(history.begin(), history.end(), [](const Batch* left, const Batch* right) {
        return left->planned_start_at() < right->planned_start_at();
    });
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Занятость — ") + qt_convert::text(machine.name()));
    dialog.resize(kHistoryWindowWidth, kHistoryWindowHeight);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QStringLiteral(
        "Зелёный — свободна · синий — завершённая обработка · оранжевый — запланированная обработка"), &dialog));
    auto* scene = new QGraphicsScene(&dialog);
    auto* view = new QGraphicsView(scene, &dialog);
    layout->addWidget(view);
    const TimePoint now = qt_convert::now();
    const TimePoint start = history.empty() ? now : std::min(now, history.front()->planned_start_at());
    TimePoint end = add_hours(now, kTimelineHoursAhead);
    for (const Batch* batch : history) end = std::max(end, batch->planned_ready_at());
    const double span = hours_between(end, start);
    scene->addRect(0.0, kTimelineTop, kTimelineWidth, kTimelineHeight,
                   QPen(Qt::NoPen), QBrush(QColor(kFreeTimeColor)));
    for (const Batch* batch : history) {
        const double batch_start_offset =
            hours_between(batch->planned_start_at(), start) / span * kTimelineWidth;
        const double width = hours_between(batch->planned_ready_at(), batch->planned_start_at()) / span * kTimelineWidth;
        const QColor color(batch->status() == BatchStatus::Completed ? kCompletedBatchColor : kScheduledBatchColor);
        auto* block = scene->addRect(batch_start_offset, kTimelineTop,
                                    std::max(width, kMinimumBatchWidth), kTimelineHeight,
                                    QPen(Qt::NoPen), QBrush(color));
        block->setToolTip(QStringLiteral("Партия №%1\n%2 — %3")
                             .arg(batch->id()).arg(qt_convert::format_time(batch->planned_start_at()),
                                                  qt_convert::format_time(batch->planned_ready_at())));
    }
    scene->addText(qt_convert::format_time(start))->setPos(0.0, 0.0);
    scene->addText(qt_convert::format_time(end))->setPos(kTimelineWidth - kTimelineLabelWidth, 0.0);
    auto* table = new QTableWidget(&dialog);
    configure_table(table, make_headers(kHistoryHeaders));
    table->setRowCount(static_cast<int>(history.size()));
    for (int row = 0; row < static_cast<int>(history.size()); ++row) {
        const Batch& batch = *history[row];
        set_cell(table, row, HistoryBatch, QString::number(batch.id()), true);
        set_cell(table, row, HistoryStart, qt_convert::format_time(batch.planned_start_at()));
        set_cell(table, row, HistoryReady, qt_convert::format_time(batch.planned_ready_at()));
        set_cell(table, row, HistoryStatus, qt_convert::text(display_name(batch.status())));
        set_cell(table, row, HistoryCompleted, qt_convert::format_time(batch.completed_at()));
    }
    layout->addWidget(table);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MachinesTab::fill(QTableWidget* table) {
    table->setRowCount(static_cast<int>(shop_.machines().size()));
    const TimePoint now = qt_convert::now();
    int row = 0;
    std::vector<Machine> machines(shop_.machines().begin(), shop_.machines().end());
    std::stable_sort(machines.begin(), machines.end(), [](const Machine& left, const Machine& right) {
        if (left.free_at() != right.free_at()) return left.free_at() < right.free_at();
        return QString::localeAwareCompare(qt_convert::text(left.name()), qt_convert::text(right.name())) < 0;
    });
    for (const Machine& machine : machines) {
        set_cell(table, row, MachineId, QString::number(machine.id()), true);
        set_cell(table, row, MachineName, qt_convert::text(machine.name()));
        set_cell(table, row, MachineType, qt_convert::text(display_name(machine.type())));
        set_cell(table, row, MachineCapacity, QString::number(machine.capacity_kg(), 'f', kCapacityDecimals), true);
        set_cell(table, row, MachineFreeAt,
                 machine.is_free_at(now) ? QStringLiteral("свободна") : qt_convert::format_time(machine.free_at()));
        set_cell(table, row, MachineMaintenance,
                 QString::number(machine.maintenance_remaining(), 'f', kMaintenanceDecimals), true);
        ++row;
    }
}

void MachinesTab::add_record() {
    MachineDialog dialog(
        QStringLiteral("Новая машина"), Machine(),
        [this](const Machine& machine) { shop_.add_machine(machine, employee_id_); }, this);
    show_dialog(dialog);
}

void MachinesTab::edit_record(Id id) {
    MachineDialog dialog(
        QStringLiteral("Изменение машины"), shop_.machines().get(id),
        [this, id](const Machine& machine) {
            Machine updated = machine;
            updated.set_id(id);
            shop_.update_machine(updated, employee_id_);
        },
        this);
    show_dialog(dialog);
}

void MachinesTab::remove_record(Id id) { shop_.remove_machine(id, employee_id_); }
