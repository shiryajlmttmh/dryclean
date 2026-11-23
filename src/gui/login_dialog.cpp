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
#include "password.h"

namespace {

constexpr int kDialogWidth = 360;
constexpr const char* kHintStyle = "color: #666666;";

}

LoginDialog::LoginDialog(DryCleaningShop& shop, QWidget* parent) : QDialog(parent), shop_(shop) {
    const bool setup = shop_.needs_password_setup();
    setWindowTitle(setup ? QStringLiteral("Первый запуск") : QStringLiteral("Химчистка - вход"));
    setMinimumWidth(kDialogWidth);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    if (setup) {
        auto* hint = new QLabel(QStringLiteral(
            "Задайте логин и пароль администратора. Минимум %1 символов в пароле.")
            .arg(password::kMinPasswordCharacters), this);
        hint->setStyleSheet(kHintStyle);
        hint->setWordWrap(true);
        layout->insertWidget(0, hint);
    }

    login_edit_ = new QLineEdit(this);
    if (setup) login_edit_->setText(QStringLiteral("admin"));
    form->addRow(QStringLiteral("Логин"), login_edit_);
    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("Пароль"), password_edit_);
    if (setup) {
        confirmation_edit_ = new QLineEdit(this);
        confirmation_edit_->setEchoMode(QLineEdit::Password);
        form->addRow(QStringLiteral("Повторите пароль"), confirmation_edit_);
    }

    error_label_ = new QLabel(this);
    style_error_label(error_label_);
    layout->addWidget(error_label_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(setup ? QStringLiteral("Создать и войти")
                                                       : QStringLiteral("Войти"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &LoginDialog::try_login);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(login_edit_, &QLineEdit::returnPressed, password_edit_, [this] { password_edit_->setFocus(); });
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
        const auto login = login_edit_->text().toStdString();
        const auto value = password_edit_->text().toStdString();
        if (confirmation_edit_) {
            password::check_confirmation(value, confirmation_edit_->text().toStdString());
            employee_id_ = shop_.initialize_admin(login, value);
        } else {
            employee_id_ = shop_.authenticate(login, value).id();
        }
        password_edit_->clear();
        if (confirmation_edit_) confirmation_edit_->clear();
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}
