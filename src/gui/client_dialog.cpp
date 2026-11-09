#include "gui/client_dialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "exceptions.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kDialogWidth = 380;
constexpr double kPercent = 100.0;
constexpr double kMinPercent = 0.0;
constexpr double kPercentStep = 0.5;
constexpr int kPercentDecimals = 1;
constexpr const char* kErrorStyle = "color: #b00020;";

}

ClientDialog::ClientDialog(const QString& title, const QString& name, const QString& phone, double discount,
                           Apply apply, QWidget* parent)
    : QDialog(parent), apply_(std::move(apply)) {
    setWindowTitle(title);
    setMinimumWidth(kDialogWidth);

    name_edit_ = new QLineEdit(name, this);
    phone_edit_ = new QLineEdit(phone, this);
    phone_edit_->setPlaceholderText(QStringLiteral("+375 29 123-45-67"));

    discount_edit_ = new QDoubleSpinBox(this);
    discount_edit_->setRange(kMinPercent, kPercent);
    discount_edit_->setDecimals(kPercentDecimals);
    discount_edit_->setSingleStep(kPercentStep);
    discount_edit_->setSuffix(QStringLiteral(" %"));
    discount_edit_->setValue(discount * kPercent);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Имя:"), name_edit_);
    form->addRow(QStringLiteral("Телефон:"), phone_edit_);
    form->addRow(QStringLiteral("Скидка:"), discount_edit_);

    error_label_ = new QLabel(this);
    error_label_->setStyleSheet(kErrorStyle);
    error_label_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Сохранить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(error_label_);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ClientDialog::try_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ClientDialog::try_accept() {
    try {
        apply_(name_edit_->text(), phone_edit_->text(), discount_edit_->value() / kPercent);
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}
