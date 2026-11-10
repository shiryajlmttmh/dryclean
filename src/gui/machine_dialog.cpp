#include "gui/machine_dialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>

#include "gui/enum_combo.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kCapacityDecimals = 1;
constexpr double kMinCapacityKg = 0.5;
constexpr double kMaxCapacityKg = 1000.0;
constexpr double kDefaultCapacityKg = 10.0;
constexpr double kCapacityStepKg = 0.5;

}

MachineDialog::MachineDialog(const QString& title, const Machine& initial, Apply apply, QWidget* parent)
    : ApplyDialog(title, parent), apply_(std::move(apply)) {
    name_edit_ = new QLineEdit(qt_convert::text(initial.name()), this);

    type_box_ = new QComboBox(this);
    fill_enum_combo<ServiceType>(type_box_, {ServiceType::DryCleaning, ServiceType::WetCleaning,
                                             ServiceType::StainRemoval, ServiceType::Leather});
    select_enum(type_box_, initial.type());

    capacity_edit_ = make_spin_box(this, kMinCapacityKg, kMaxCapacityKg, kCapacityDecimals, kCapacityStepKg,
                                   QStringLiteral(" кг"),
                                   initial.capacity_kg() > 0.0 ? initial.capacity_kg() : kDefaultCapacityKg);

    form()->addRow(QStringLiteral("Название:"), name_edit_);
    form()->addRow(QStringLiteral("Тип услуги:"), type_box_);
    form()->addRow(QStringLiteral("Вместимость:"), capacity_edit_);
}

void MachineDialog::apply() {
    apply_(Machine(name_edit_->text().toStdString(), enum_value<ServiceType>(type_box_), capacity_edit_->value()));
}
