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
constexpr std::size_t kDefaultStaffCount = 1;
constexpr const char* kHintStyle = "color: #666666;";

}

LoginDialog::LoginDialog(const DryCleaningShop& shop, QWidget* parent) : QDialog(parent), shop_(shop) {
    setWindowTitle(QStringLiteral("Химчистка — вход"));
    setMinimumWidth(kDialogWidth);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("Логин сотрудника:"), this));

    login_edit_ = new QLineEdit(this);
    layout->addWidget(login_edit_);

    error_label_ = new QLabel(this);
    error_label_->setStyleSheet(kErrorStyle);
    error_label_->setWordWrap(true);
    layout->addWidget(error_label_);

    if (shop_.employees().size() == kDefaultStaffCount) {
        auto* hint = new QLabel(QStringLiteral("Первый запуск: войдите с логином admin."), this);
        hint->setStyleSheet(kHintStyle);
        hint->setWordWrap(true);
        layout->addWidget(hint);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Войти"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &LoginDialog::try_login);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(login_edit_, &QLineEdit::returnPressed, this, &LoginDialog::try_login);
}

Id LoginDialog::employee_id() const noexcept { return employee_id_; }

void LoginDialog::try_login() {
    try {
        employee_id_ = shop_.authenticate(login_edit_->text().toStdString()).id();
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}
