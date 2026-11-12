#include "gui/material_dialog.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>

#include "display.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kStockDecimals = 2;
constexpr double kMinStock = 0.0;
constexpr double kMaxStock = 1000000.0;
constexpr double kStockStep = 1.0;
constexpr int kConsumptionDecimals = 3;
constexpr double kMinConsumption = 0.001;
constexpr double kMaxConsumption = 1000.0;
constexpr double kDefaultConsumption = 0.1;
constexpr double kConsumptionStep = 0.01;

constexpr ServiceType kServices[] = {ServiceType::DryCleaning, ServiceType::WetCleaning, ServiceType::StainRemoval,
                                     ServiceType::Leather};

}

MaterialDialog::MaterialDialog(const QString& title, const Material& initial, bool is_new, Apply apply,
                               QWidget* parent)
    : ApplyDialog(title, parent), apply_(std::move(apply)) {
    name_edit_ = new QLineEdit(qt_convert::text(initial.name()), this);
    unit_edit_ = new QLineEdit(qt_convert::text(initial.unit()), this);
    unit_edit_->setPlaceholderText(QStringLiteral("например, л или кг"));

    stock_edit_ = make_spin_box(this, kMinStock, kMaxStock, kStockDecimals, kStockStep, QString(), initial.stock());
    consumption_edit_ =
        make_spin_box(this, kMinConsumption, kMaxConsumption, kConsumptionDecimals, kConsumptionStep, QString(),
                      initial.consumption_per_kg() > 0.0 ? initial.consumption_per_kg() : kDefaultConsumption);

    unit_edit_->setEnabled(is_new);
    stock_edit_->setEnabled(is_new);

    form()->addRow(QStringLiteral("Название:"), name_edit_);
    form()->addRow(QStringLiteral("Единица измерения:"), unit_edit_);
    form()->addRow(QStringLiteral("Остаток:"), stock_edit_);
    form()->addRow(QStringLiteral("Расход на 1 кг белья:"), consumption_edit_);
    bool first_row = true;
    for (const ServiceType service : kServices) {
        auto* box = new QCheckBox(qt_convert::text(display_name(service)), this);
        box->setChecked(initial.is_used_for(service));
        service_boxes_.emplace_back(service, box);
        form()->addRow(first_row ? QStringLiteral("Используется в услугах:") : QString(), box);
        first_row = false;
    }
}

std::set<ServiceType> MaterialDialog::chosen_services() const {
    std::set<ServiceType> chosen;
    for (const auto& [service, box] : service_boxes_) {
        if (box->isChecked()) {
            chosen.insert(service);
        }
    }
    return chosen;
}

void MaterialDialog::apply() {
    apply_(Material(name_edit_->text().toStdString(), unit_edit_->text().toStdString(), stock_edit_->value(),
                    consumption_edit_->value(), chosen_services()));
}
