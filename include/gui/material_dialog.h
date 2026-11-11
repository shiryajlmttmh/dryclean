#pragma once

#include <functional>
#include <set>
#include <utility>
#include <vector>

#include "gui/apply_dialog.h"
#include "material.h"

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;

class MaterialDialog : public ApplyDialog {
public:
    using Apply = std::function<void(const Material&)>;

    MaterialDialog(const QString& title, const Material& initial, bool is_new, Apply apply,
                   QWidget* parent = nullptr);

protected:
    void apply() override;

private:
    std::set<ServiceType> chosen_services() const;

    QLineEdit* name_edit_;
    QLineEdit* unit_edit_;
    QDoubleSpinBox* stock_edit_;
    QDoubleSpinBox* consumption_edit_;
    std::vector<std::pair<ServiceType, QCheckBox*>> service_boxes_;
    Apply apply_;
};
