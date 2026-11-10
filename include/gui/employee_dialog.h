#pragma once

#include <functional>

#include "employee.h"
#include "gui/apply_dialog.h"

class QComboBox;
class QLineEdit;

class EmployeeDialog : public ApplyDialog {
public:
    using Apply = std::function<void(const Employee&)>;

    EmployeeDialog(const QString& title, const Employee& initial, Apply apply, QWidget* parent = nullptr);

protected:
    void apply() override;

private:
    QLineEdit* name_edit_;
    QLineEdit* login_edit_;
    QComboBox* role_box_;
    Apply apply_;
};
