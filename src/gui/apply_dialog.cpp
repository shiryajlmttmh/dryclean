#include "gui/apply_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "exceptions.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kDialogWidth = 380;

}

ApplyDialog::ApplyDialog(const QString& title, QWidget* parent) : QDialog(parent) {
    setWindowTitle(title);
    setMinimumWidth(kDialogWidth);

    form_ = new QFormLayout;
    error_label_ = new QLabel(this);
    error_label_->setStyleSheet(kErrorStyle);
    error_label_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Сохранить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form_);
    layout->addWidget(error_label_);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ApplyDialog::try_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QFormLayout* ApplyDialog::form() const noexcept { return form_; }

void ApplyDialog::try_accept() {
    try {
        apply();
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}
