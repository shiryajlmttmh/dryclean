#pragma once

#include <functional>

#include "employee.h"
#include "gui/apply_dialog.h"

class QComboBox;
class QLineEdit;
class DryCleaningShop;

class EmployeeDialog : public ApplyDialog {
public:
    using Apply = std::function<void(const Employee&)>;

    EmployeeDialog(const DryCleaningShop& shop, const QString& title, const Employee& initial, Apply apply, QWidget* parent = nullptr);

protected:
    void apply() override;

private:
    QLineEdit* name_edit_;
    QLineEdit* login_edit_;
    QLineEdit* password_edit_;
    QLineEdit* confirmation_edit_;
    QComboBox* role_box_;
    Employee initial_;
    Apply apply_;
};
