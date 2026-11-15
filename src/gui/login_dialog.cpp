#include "gui/login_dialog.h"

#include <QDialogButtonBox>
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
    setWindowTitle(QStringLiteral("Химчистка — вход"));
    setMinimumWidth(kDialogWidth);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("Логин сотрудника:"), this));

    login_edit_ = new QLineEdit(this);
    layout->addWidget(login_edit_);

    const bool setup = shop_.needs_password_setup();
    layout->addWidget(new QLabel(setup ? QStringLiteral("Новый пароль администратора:")
                                      : QStringLiteral("Пароль:"), this));
    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    layout->addWidget(password_edit_);
    if (setup) {
        layout->addWidget(new QLabel(QStringLiteral("Повторите пароль:"), this));
        confirmation_edit_ = new QLineEdit(this);
        confirmation_edit_->setEchoMode(QLineEdit::Password);
        layout->addWidget(confirmation_edit_);
    }

    error_label_ = new QLabel(this);
    error_label_->setStyleSheet(kErrorStyle);
    error_label_->setWordWrap(true);
    layout->addWidget(error_label_);

    if (setup) {
        auto* hint = new QLabel(QStringLiteral(
            "Введите логин admin и задайте пароль длиной не менее 8 символов."), this);
        hint->setStyleSheet(kHintStyle);
        hint->setWordWrap(true);
        layout->addWidget(hint);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(setup ? QStringLiteral("Задать пароль и войти")
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
        const auto password = password_edit_->text().toStdString();
        if (shop_.needs_password_setup()) {
            if (!confirmation_edit_ || password_edit_->text() != confirmation_edit_->text()) {
                throw InvalidInputException("password", "пароли не совпадают");
            }
            shop_.initialize_admin_password(login, password);
        }
        employee_id_ = shop_.authenticate(login, password).id();
        password_edit_->clear();
        if (confirmation_edit_) confirmation_edit_->clear();
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}
