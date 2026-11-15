#include "gui/employee_dialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>

#include "gui/enum_combo.h"
#include "gui/qt_convert.h"
#include "exceptions.h"

EmployeeDialog::EmployeeDialog(const QString& title, const Employee& initial, Apply apply, QWidget* parent)
    : ApplyDialog(title, parent), initial_(initial), apply_(std::move(apply)) {
    name_edit_ = new QLineEdit(qt_convert::text(initial.name()), this);
    login_edit_ = new QLineEdit(qt_convert::text(initial.login()), this);
    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    password_edit_->setPlaceholderText(initial.has_password() ? QStringLiteral("Пусто — сохранить пароль")
                                                            : QStringLiteral("Новый пароль, минимум 8 символов"));
    confirmation_edit_ = new QLineEdit(this);
    confirmation_edit_->setEchoMode(QLineEdit::Password);

    role_box_ = new QComboBox(this);
    fill_enum_combo<EmployeeRole>(role_box_,
                                  {EmployeeRole::Receptionist, EmployeeRole::Operator, EmployeeRole::Administrator});
    select_enum(role_box_, initial.role());

    form()->addRow(QStringLiteral("Имя:"), name_edit_);
    form()->addRow(QStringLiteral("Логин:"), login_edit_);
    form()->addRow(QStringLiteral("Пароль:"), password_edit_);
    form()->addRow(QStringLiteral("Повтор пароля:"), confirmation_edit_);
    form()->addRow(QStringLiteral("Роль:"), role_box_);
}

void EmployeeDialog::apply() {
    Employee updated = initial_;
    updated.set_name(name_edit_->text().toStdString());
    updated.set_login(login_edit_->text().toStdString());
    updated.set_role(enum_value<EmployeeRole>(role_box_));
    if (password_edit_->text() != confirmation_edit_->text()) {
        throw InvalidInputException("password", "пароли не совпадают");
    }
    if (!password_edit_->text().isEmpty() || !updated.has_password()) {
        updated.set_password(password_edit_->text().toStdString());
    }
    apply_(updated);
}
