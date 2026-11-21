#include "gui/client_dialog.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

#include "gui/helpers.h"

namespace {

constexpr double kPercent = 100.0;
constexpr double kMinPercent = 0.0;
constexpr double kPercentStep = 0.5;
constexpr int kPercentDecimals = 1;

}

ClientDialog::ClientDialog(const QString& title, const QString& name, const QString& phone, double discount,
                           Apply apply, QWidget* parent)
    : ApplyDialog(title, parent), apply_(std::move(apply)) {
    name_edit_ = new QLineEdit(name, this);
    phone_edit_ = new QLineEdit(phone, this);
    auto* phone_hint = new QLabel(QStringLiteral("Например, +375 (29) 123-45-67"), this);
    phone_edit_->setAccessibleDescription(phone_hint->text());
    discount_edit_ = make_spin_box(this, kMinPercent, kPercent, kPercentDecimals, kPercentStep, QStringLiteral(" %"),
                                   discount * kPercent);

    form()->addRow(QStringLiteral("Имя:"), name_edit_);
    form()->addRow(QStringLiteral("Мобильный телефон:"), phone_edit_);
    form()->addRow(QString(), phone_hint);
    form()->addRow(QStringLiteral("Скидка:"), discount_edit_);
}

void ClientDialog::apply() {
    apply_(name_edit_->text(), phone_edit_->text(), discount_edit_->value() / kPercent);
}
