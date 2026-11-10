#pragma once

#include <functional>

#include "gui/apply_dialog.h"

class QDoubleSpinBox;
class QLineEdit;

class ClientDialog : public ApplyDialog {
public:
    using Apply = std::function<void(const QString& name, const QString& phone, double discount)>;

    ClientDialog(const QString& title, const QString& name, const QString& phone, double discount, Apply apply,
                 QWidget* parent = nullptr);

protected:
    void apply() override;

private:
    QLineEdit* name_edit_;
    QLineEdit* phone_edit_;
    QDoubleSpinBox* discount_edit_;
    Apply apply_;
};
