#pragma once

#include <functional>

#include "gui/apply_dialog.h"
#include "machine.h"

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class DryCleaningShop;

class MachineDialog : public ApplyDialog {
public:
    using Apply = std::function<void(const Machine&)>;

    MachineDialog(const DryCleaningShop& shop, const QString& title, const Machine& initial, Apply apply, QWidget* parent = nullptr);

protected:
    void apply() override;

private:
    QLineEdit* name_edit_;
    QComboBox* type_box_;
    QDoubleSpinBox* capacity_edit_;
    QDoubleSpinBox* maintenance_edit_;
    Apply apply_;
};
