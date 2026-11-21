#include "gui/machines_tab.h"

#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextDocument>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
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
constexpr int kHoursPerDay = 24;
constexpr double kTimelineWidth = 900.0;
constexpr double kTimelineTop = 30.0;
constexpr double kTimelineHeight = 42.0;
constexpr double kMinimumBatchWidth = 1.0;
constexpr auto kFreeTimeColor = "#86cfa5";
constexpr auto kCompletedBatchColor = "#5d9bd3";
constexpr auto kScheduledBatchColor = "#efad55";

enum HistoryColumn { HistoryBatch, HistoryStart, HistoryReady, HistoryStatus, HistoryCompleted, HistoryColumnCount };

enum MachineColumn { MachineId, MachineName, MachineType, MachineCapacity, MachineFreeAt, MachineMaintenance, MachineColumnCount };

constexpr std::array<const char*, MachineColumnCount> kMachineHeaders{"№", "Название", "Тип", "Объём, кг",
                                                                      "Доступность", "До ТО, ч"};
constexpr std::array<const char*, HistoryColumnCount> kHistoryHeaders{
    "Партия", "Начало", "Конец обработки", "Статус", "Подтверждение завершения"};

void draw_timeline(QGraphicsScene& scene, const std::vector<const Batch*>& history, TimePoint center) {
    scene.clear();
    const TimePoint start = add_hours(center, -kHoursPerDay);
    const TimePoint end = add_hours(center, kHoursPerDay);
    const double span = hours_between(end, start);
    scene.addRect(0.0, kTimelineTop, kTimelineWidth, kTimelineHeight,
                  QPen(Qt::NoPen), QBrush(QColor(kFreeTimeColor)));
    for (const Batch* batch : history) {
        const TimePoint visible_start = std::max(start, batch->planned_start_at());
        const TimePoint visible_end = std::min(end, batch->planned_ready_at());
        if (visible_end <= visible_start) continue;

        const double batch_start_offset = hours_between(visible_start, start) / span * kTimelineWidth;
        const double width = hours_between(visible_end, visible_start) / span * kTimelineWidth;
        const double block_width = std::min(std::max(width, kMinimumBatchWidth), kTimelineWidth - batch_start_offset);
        const QColor color(batch->status() == BatchStatus::Completed ? kCompletedBatchColor : kScheduledBatchColor);
        auto* block = scene.addRect(batch_start_offset, kTimelineTop, block_width, kTimelineHeight,
                                    QPen(Qt::NoPen), QBrush(color));
        block->setToolTip(QStringLiteral("Партия №%1\n%2 — %3")
                             .arg(batch->id()).arg(qt_convert::format_time(batch->planned_start_at()),
                                                  qt_convert::format_time(batch->planned_ready_at())));
    }
    auto* start_label = scene.addText(qt_convert::format_time(start));
    start_label->document()->setDocumentMargin(0.0);
    start_label->setPos(0.0, 0.0);
    auto* end_label = scene.addText(qt_convert::format_time(end));
    end_label->document()->setDocumentMargin(0.0);
    end_label->setPos(kTimelineWidth - end_label->boundingRect().width(), 0.0);
}

}

MachinesTab::MachinesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : TableTab(shop, employee_id, Permission::ManageStaff, make_headers(kMachineHeaders),
               parent, MachineName, true) {}

void MachinesTab::add_extra_actions(QHBoxLayout* actions, Id id) {
    add_row_action(actions, QStringLiteral("Занятость"), [this, id] { show_history(id); });
    if (is_allowed()) {
        auto* menu = add_more_actions(actions, QStringLiteral("Обслуживание машины №%1").arg(id));
        auto* service = menu->addAction(QStringLiteral("Выполнить ТО"), this, [this, id] { service_machine(id); });
        if (!shop_.can_service_machine(id, qt_convert::now())) {
            service->setEnabled(false);
            service->setToolTip(QStringLiteral("Сначала завершите все партии машины."));
            menu->setToolTipsVisible(true);
        }
    }
}

QString MachinesTab::removal_block(Id id) const {
    return shop_.can_remove_machine(id) ? QString() : QStringLiteral("У машины есть партии.");
}

void MachinesTab::service_machine(Id id) {
    if (!is_allowed()) return;
    if (!confirm(this, QStringLiteral("Техобслуживание"),
                 QStringLiteral("Подтвердить выполненное ТО и обнулить счётчик часов?"))) return;
    if (run_guarded(this, [this, id] { shop_.service_machine(id, employee_id_, qt_convert::now()); })) {
        emit changed();
    }
}

void MachinesTab::show_history(Id id) {
    const Machine& machine = shop_.machines().get(id);
    std::vector<const Batch*> history;
    for (const Batch& batch : shop_.batches()) {
        if (batch.machine_id() == id) history.push_back(&batch);
    }
    std::sort(history.begin(), history.end(), [](const Batch* left, const Batch* right) {
        return left->planned_start_at() < right->planned_start_at();
    });
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Занятость — ") + qt_convert::text(machine.name()));
    dialog.resize(kHistoryWindowWidth, kHistoryWindowHeight);
    auto* layout = new QVBoxLayout(&dialog);
    auto* date_edit = new QDateTimeEdit(QDateTime::currentDateTime(), &dialog);
    date_edit->setDisplayFormat(QStringLiteral("dd.MM.yyyy HH:mm"));
    date_edit->setCalendarPopup(true);
    auto* now_button = new QPushButton(QStringLiteral("Сейчас"), &dialog);
    auto* date_controls = new QHBoxLayout;
    date_controls->addWidget(new QLabel(QStringLiteral("Центр шкалы:"), &dialog));
    date_controls->addWidget(date_edit);
    date_controls->addWidget(now_button);
    date_controls->addStretch();
    layout->addLayout(date_controls);
    layout->addWidget(new QLabel(QStringLiteral(
        "Зелёный — свободна · синий — завершённая обработка · оранжевый — запланированная обработка"), &dialog));
    auto* scene = new QGraphicsScene(&dialog);
    auto* view = new QGraphicsView(scene, &dialog);
    layout->addWidget(view);
    const auto update_timeline = [scene, &history](const QDateTime& date_time) {
        draw_timeline(*scene, history, from_epoch_ms(date_time.toMSecsSinceEpoch()));
    };
    connect(date_edit, &QDateTimeEdit::dateTimeChanged, &dialog, update_timeline);
    connect(now_button, &QPushButton::clicked, &dialog, [date_edit] {
        date_edit->setDateTime(QDateTime::currentDateTime());
    });
    update_timeline(date_edit->dateTime());
    auto* table = new TableWidget(make_headers(kHistoryHeaders), &dialog, HistoryStatus);
    table->set_empty_text(QStringLiteral("У этой машины пока нет партий."));
    table->reset_rows(static_cast<int>(history.size()));
    for (int row = 0; row < static_cast<int>(history.size()); ++row) {
        const Batch& batch = *history[row];
        set_cell(table, row, HistoryBatch, batch.id());
        set_time_cell(table, row, HistoryStart, batch.planned_start_at());
        set_time_cell(table, row, HistoryReady, batch.planned_ready_at());
        set_cell(table, row, HistoryStatus, qt_convert::text(display_name(batch.status())));
        set_time_cell(table, row, HistoryCompleted, batch.completed_at());
    }
    layout->addWidget(table);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрыть"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MachinesTab::fill(TableWidget* table) {
    table->reset_rows(static_cast<int>(shop_.machines().size()));
    const TimePoint now = qt_convert::now();
    int row = 0;
    std::vector<Machine> machines(shop_.machines().begin(), shop_.machines().end());
    std::stable_sort(machines.begin(), machines.end(), [](const Machine& left, const Machine& right) {
        if (left.free_at() != right.free_at()) return left.free_at() < right.free_at();
        return QString::localeAwareCompare(qt_convert::text(left.name()), qt_convert::text(right.name())) < 0;
    });
    for (const Machine& machine : machines) {
        set_cell(table, row, MachineId, machine.id());
        set_cell(table, row, MachineName, qt_convert::text(machine.name()));
        set_cell(table, row, MachineType, qt_convert::text(display_name(machine.type())));
        set_sortable_cell(table, row, MachineCapacity, QString::number(machine.capacity_kg(), 'f', kCapacityDecimals),
                          machine.capacity_kg(), true);
        const bool is_free = machine.is_free_at(now);
        set_sortable_cell(table, row, MachineFreeAt,
                          is_free ? QStringLiteral("свободна") : qt_convert::format_time(machine.free_at()),
                          time_sort_value(is_free ? std::nullopt : std::optional(machine.free_at())));
        set_sortable_cell(table, row, MachineMaintenance,
                          QString::number(machine.maintenance_remaining(), 'f', kMaintenanceDecimals),
                          machine.maintenance_remaining());
        table->item(row, MachineMaintenance)->setToolTip(QStringLiteral("%1 ч с учётом плана")
                                                           .arg(table->item(row, MachineMaintenance)->text()));
        ++row;
    }
}

void MachinesTab::add_record() {
    MachineDialog dialog(
        shop_,         QStringLiteral("Новая машина"), Machine(),
        [this](const Machine& machine) { shop_.add_machine(machine, employee_id_); }, this);
    show_dialog(dialog);
}

void MachinesTab::edit_record(Id id) {
    MachineDialog dialog(
        shop_,         QStringLiteral("Изменение машины"), shop_.machines().get(id),
        [this, id](const Machine& machine) {
            Machine updated = machine;
            updated.set_id(id);
            shop_.update_machine(updated, employee_id_);
        },
        this);
    show_dialog(dialog);
}

void MachinesTab::remove_record(Id id) { shop_.remove_machine(id, employee_id_); }
