#include "gui/employees_tab.h"

#include <QTableWidget>
#include <array>

#include "display.h"
#include "gui/employee_dialog.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

enum EmployeeColumn { EmployeeId, EmployeeName, EmployeeLogin, EmployeeRoleName, EmployeeColumnCount };

constexpr std::array<const char*, EmployeeColumnCount> kEmployeeHeaders{"№", "Имя", "Логин", "Роль"};

}

EmployeesTab::EmployeesTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : TableTab(shop, employee_id, Permission::ManageStaff, make_headers(kEmployeeHeaders), parent) {}

void EmployeesTab::fill(QTableWidget* table) {
    table->setRowCount(static_cast<int>(shop_.employees().size()));
    int row = 0;
    for (const Employee& employee : shop_.employees()) {
        set_cell(table, row, EmployeeId, QString::number(employee.id()), true);
        set_cell(table, row, EmployeeName, qt_convert::text(employee.name()));
        set_cell(table, row, EmployeeLogin, qt_convert::text(employee.login()));
        set_cell(table, row, EmployeeRoleName, qt_convert::text(display_name(employee.role())));
        ++row;
    }
}

void EmployeesTab::add_record() {
    EmployeeDialog dialog(
        QStringLiteral("Новый сотрудник"), Employee(),
        [this](const Employee& employee) { shop_.add_employee(employee, employee_id_); }, this);
    show_dialog(dialog);
}

void EmployeesTab::edit_record(Id id) {
    EmployeeDialog dialog(
        QStringLiteral("Изменение сотрудника"), shop_.employees().get(id),
        [this, id](const Employee& employee) {
            Employee updated = employee;
            updated.set_id(id);
            shop_.update_employee(updated, employee_id_);
        },
        this);
    show_dialog(dialog);
}

void EmployeesTab::remove_record(Id id) { shop_.remove_employee(id, employee_id_); }
