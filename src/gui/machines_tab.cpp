#include "gui/machines_tab.h"

#include <QTableWidget>
#include <array>

#include "display.h"
#include "gui/machine_dialog.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kCapacityDecimals = 1;

enum MachineColumn { MachineId, MachineName, MachineType, MachineCapacity, MachineFreeAt, MachineColumnCount };

constexpr std::array<const char*, MachineColumnCount> kMachineHeaders{"№", "Название", "Тип", "Вместимость, кг",
                                                                      "Доступность"};

}

MachinesTab::MachinesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : TableTab(shop, employee_id, Permission::ManageStaff, make_headers(kMachineHeaders), parent) {}

void MachinesTab::fill(QTableWidget* table) {
    table->setRowCount(static_cast<int>(shop_.machines().size()));
    const TimePoint now = qt_convert::now();
    int row = 0;
    for (const Machine& machine : shop_.machines()) {
        set_cell(table, row, MachineId, QString::number(machine.id()), true);
        set_cell(table, row, MachineName, qt_convert::text(machine.name()));
        set_cell(table, row, MachineType, qt_convert::text(display_name(machine.type())));
        set_cell(table, row, MachineCapacity, QString::number(machine.capacity_kg(), 'f', kCapacityDecimals), true);
        set_cell(table, row, MachineFreeAt,
                 machine.is_free_at(now) ? QStringLiteral("свободна") : qt_convert::format_time(machine.free_at()));
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
