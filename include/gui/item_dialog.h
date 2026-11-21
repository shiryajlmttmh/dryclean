#pragma once

#include <QDialog>
#include <vector>

#include "item.h"

class DryCleaningShop;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class TableWidget;

class ItemDialog : public QDialog {
    Q_OBJECT

public:
    ItemDialog(const DryCleaningShop& shop, const Item& initial, const QString& title,
               QWidget* parent = nullptr);

    const Item& item() const noexcept;

private:
    struct Stage {
        Id id;
        ServiceType service;
    };

    QWidget* build_item_page(const Item& initial);
    QWidget* build_condition_page(const Item& initial);
    QWidget* build_services_page();
    void show_step(int page);
    void advance();
    Item build_details() const;

    void show_defects();
    void add_defect();
    void remove_defect(QListWidgetItem* item);

    QString service_block(ServiceType service) const;
    void refresh_service_choice();
    void refresh_stages();
    void add_stage();
    void move_stage(Id id, int offset);
    void remove_stage(Id id);

    const DryCleaningShop& shop_;
    QStackedWidget* pages_;
    QLabel* step_label_;
    QLabel* error_label_;
    QPushButton* back_button_;
    QPushButton* next_button_;

    QLineEdit* kind_edit_;
    QComboBox* material_box_;
    QComboBox* color_box_;
    QDoubleSpinBox* weight_edit_;
    QComboBox* soil_box_;
    QLineEdit* notes_edit_;
    QListWidget* defects_list_;
    QComboBox* defect_type_box_;
    QLineEdit* defect_name_edit_;
    QLineEdit* defect_description_edit_;
    QDoubleSpinBox* defect_area_edit_;

    QComboBox* service_box_;
    QPushButton* add_stage_button_;
    QLabel* service_hint_;
    QLabel* stages_hint_;
    TableWidget* stages_table_;

    std::vector<Item::Defect> defects_;
    std::vector<Stage> stages_;
    Id next_stage_id_ = 1;
    Item service_item_;
    Item result_;
};
