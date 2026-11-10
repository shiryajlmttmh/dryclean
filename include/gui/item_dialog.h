#pragma once

#include <QDialog>
#include <vector>

#include "item.h"

class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QWidget;

class ItemDialog : public QDialog {
    Q_OBJECT

public:
    ItemDialog(const Item& initial, const QString& title, QWidget* parent = nullptr);

    const Item& item() const noexcept;

private slots:
    void try_accept();
    void add_defect();
    void remove_defect();

private:
    void build_item_form(QFormLayout* form, const Item& initial);
    QWidget* build_defects_box();
    void show_defects();
    Item build_item() const;

    QLineEdit* kind_edit_;
    QLineEdit* material_edit_;
    QComboBox* material_group_box_;
    QComboBox* fabric_group_box_;
    QLineEdit* color_edit_;
    QComboBox* color_group_box_;
    QDoubleSpinBox* weight_edit_;
    QComboBox* soil_box_;
    QLineEdit* notes_edit_;
    QListWidget* defects_list_;
    QComboBox* defect_type_box_;
    QLineEdit* defect_description_edit_;
    QDoubleSpinBox* defect_area_edit_;
    QLabel* error_label_;
    std::vector<Item::Defect> defects_;
    Item result_;
};
