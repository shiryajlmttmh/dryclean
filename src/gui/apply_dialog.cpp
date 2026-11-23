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
    style_error_label(error_label_);

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
    } catch (const InvalidInputException& error) {
        QString field;
        if (error.field_name() == "name") field = QStringLiteral("Имя");
        else if (error.field_name() == "phone") field = QStringLiteral("Телефон");
        else if (error.field_name() == "discount") field = QStringLiteral("Скидка");
        const QString message = qt_convert::text(error.what());
        const bool field_in_message = !field.isEmpty() && message.startsWith(field, Qt::CaseInsensitive);
        error_label_->setText(field.isEmpty() || field_in_message ? message : field + QStringLiteral(": ") + message);
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}
