#include "gui/login_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "exceptions.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kDialogWidth = 360;
constexpr const char* kHintStyle = "color: #666666;";

}

LoginDialog::LoginDialog(DryCleaningShop& shop, QWidget* parent) : QDialog(parent), shop_(shop) {
    const bool setup = shop_.needs_password_setup();
    setWindowTitle(setup ? QStringLiteral("Первый запуск") : QStringLiteral("Химчистка — вход"));
    setMinimumWidth(kDialogWidth);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    if (setup) {
        auto* hint = new QLabel(QStringLiteral(
            "Задайте пароль для учётной записи admin. Минимум 8 символов."), this);
        hint->setStyleSheet(kHintStyle);
        hint->setWordWrap(true);
        layout->insertWidget(0, hint);
    } else {
        login_edit_ = new QLineEdit(this);
        form->addRow(QStringLiteral("&Логин"), login_edit_);
    }

    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("&Пароль"), password_edit_);
    if (setup) {
        confirmation_edit_ = new QLineEdit(this);
        confirmation_edit_->setEchoMode(QLineEdit::Password);
        form->addRow(QStringLiteral("Пов&торите пароль"), confirmation_edit_);
    }

    error_label_ = new QLabel(this);
    error_label_->setStyleSheet(kErrorStyle);
    error_label_->setWordWrap(true);
    layout->addWidget(error_label_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(setup ? QStringLiteral("Задать пароль и войти")
                                                       : QStringLiteral("Войти"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &LoginDialog::try_login);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (login_edit_) {
        connect(login_edit_, &QLineEdit::returnPressed, password_edit_, [this] { password_edit_->setFocus(); });
    }
    connect(password_edit_, &QLineEdit::returnPressed, this, [this] {
        if (confirmation_edit_) confirmation_edit_->setFocus();
        else try_login();
    });
    if (confirmation_edit_) {
        connect(confirmation_edit_, &QLineEdit::returnPressed, this, &LoginDialog::try_login);
    }
}

Id LoginDialog::employee_id() const noexcept { return employee_id_; }

void LoginDialog::try_login() {
    try {
        const auto password = password_edit_->text().toStdString();
        if (confirmation_edit_) {
            if (password_edit_->text() != confirmation_edit_->text()) {
                throw InvalidInputException("password", "пароли не совпадают");
            }
            employee_id_ = shop_.initialize_admin_password(password);
        } else {
            employee_id_ = shop_.authenticate(login_edit_->text().toStdString(), password).id();
        }
        password_edit_->clear();
        if (confirmation_edit_) confirmation_edit_->clear();
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}
