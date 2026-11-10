#include "gui/employee_dialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>

#include "gui/enum_combo.h"
#include "gui/qt_convert.h"

EmployeeDialog::EmployeeDialog(const QString& title, const Employee& initial, Apply apply, QWidget* parent)
    : ApplyDialog(title, parent), apply_(std::move(apply)) {
    name_edit_ = new QLineEdit(qt_convert::text(initial.name()), this);
    login_edit_ = new QLineEdit(qt_convert::text(initial.login()), this);

    role_box_ = new QComboBox(this);
    fill_enum_combo<EmployeeRole>(role_box_,
                                  {EmployeeRole::Receptionist, EmployeeRole::Operator, EmployeeRole::Administrator});
    select_enum(role_box_, initial.role());

    form()->addRow(QStringLiteral("Имя:"), name_edit_);
    form()->addRow(QStringLiteral("Логин:"), login_edit_);
    form()->addRow(QStringLiteral("Роль:"), role_box_);
}

void EmployeeDialog::apply() {
    apply_(Employee(name_edit_->text().toStdString(), login_edit_->text().toStdString(),
                    enum_value<EmployeeRole>(role_box_)));
}
