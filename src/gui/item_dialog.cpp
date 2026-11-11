#include "gui/item_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "display.h"
#include "exceptions.h"
#include "gui/enum_combo.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kDialogWidth = 520;
constexpr int kListHeight = 90;
constexpr int kWeightDecimals = 2;
constexpr int kAreaDecimals = 1;
constexpr double kMinWeightKg = 0.1;
constexpr double kMaxWeightKg = 100.0;
constexpr double kDefaultWeightKg = 1.0;
constexpr double kWeightStepKg = 0.1;
constexpr double kMinAreaCm2 = 0.1;
constexpr double kMaxAreaCm2 = 10000.0;
constexpr double kDefaultAreaCm2 = 5.0;
constexpr double kAreaStepCm2 = 1.0;

QString describe(const Item::Defect& defect) {
    return QStringLiteral("%1, %2 см² — %3")
        .arg(qt_convert::text(display_name(defect.type)), QString::number(defect.area_cm2, 'f', kAreaDecimals),
             qt_convert::text(defect.description));
}

}

ItemDialog::ItemDialog(const Item& initial, const QString& title, QWidget* parent)
    : QDialog(parent), defects_(initial.defects()), result_(initial) {
    setWindowTitle(title);
    setMinimumWidth(kDialogWidth);

    auto* form = new QFormLayout;
    build_item_form(form, initial);

    error_label_ = new QLabel(this);
    error_label_->setStyleSheet(kErrorStyle);
    error_label_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Готово"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(build_defects_box());
    layout->addWidget(error_label_);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ItemDialog::try_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    show_defects();
}

void ItemDialog::build_item_form(QFormLayout* form, const Item& initial) {
    kind_edit_ = new QLineEdit(qt_convert::text(initial.kind()), this);
    kind_edit_->setPlaceholderText(QStringLiteral("например, пальто"));
    material_edit_ = new QLineEdit(qt_convert::text(initial.material_name()), this);
    material_edit_->setPlaceholderText(QStringLiteral("например, шерсть"));
    color_edit_ = new QLineEdit(qt_convert::text(initial.color_name()), this);
    color_edit_->setPlaceholderText(QStringLiteral("например, тёмно-синий"));
    notes_edit_ = new QLineEdit(qt_convert::text(initial.condition_notes()), this);

    material_group_box_ = new QComboBox(this);
    fill_enum_combo<MaterialGroup>(material_group_box_,
                                   {MaterialGroup::Basic, MaterialGroup::Delicate, MaterialGroup::LeatherSuede,
                                    MaterialGroup::SheepskinVelour, MaterialGroup::NaturalFur});
    select_enum(material_group_box_, initial.material_group());

    fabric_group_box_ = new QComboBox(this);
    fill_enum_combo<FabricGroup>(fabric_group_box_, {FabricGroup::Dense, FabricGroup::Delicate, FabricGroup::LeatherFur});
    select_enum(fabric_group_box_, initial.fabric_group());

    color_group_box_ = new QComboBox(this);
    fill_enum_combo<ColorGroup>(color_group_box_, {ColorGroup::Light, ColorGroup::Dark});
    select_enum(color_group_box_, initial.color_group());

    soil_box_ = new QComboBox(this);
    fill_enum_combo<SoilLevel>(soil_box_, {SoilLevel::Light, SoilLevel::Medium, SoilLevel::Heavy});
    select_enum(soil_box_, initial.soil_level());

    weight_edit_ = new QDoubleSpinBox(this);
    weight_edit_->setRange(kMinWeightKg, kMaxWeightKg);
    weight_edit_->setDecimals(kWeightDecimals);
    weight_edit_->setSingleStep(kWeightStepKg);
    weight_edit_->setSuffix(QStringLiteral(" кг"));
    weight_edit_->setValue(initial.weight_kg() > 0.0 ? initial.weight_kg() : kDefaultWeightKg);

    form->addRow(QStringLiteral("Изделие:"), kind_edit_);
    form->addRow(QStringLiteral("Материал:"), material_edit_);
    form->addRow(QStringLiteral("Группа материала:"), material_group_box_);
    form->addRow(QStringLiteral("Группа ткани:"), fabric_group_box_);
    form->addRow(QStringLiteral("Цвет:"), color_edit_);
    form->addRow(QStringLiteral("Группа цвета:"), color_group_box_);
    form->addRow(QStringLiteral("Масса:"), weight_edit_);
    form->addRow(QStringLiteral("Загрязнение:"), soil_box_);
    form->addRow(QStringLiteral("Примечания:"), notes_edit_);
}

QWidget* ItemDialog::build_defects_box() {
    auto* box = new QGroupBox(QStringLiteral("Пятна и дефекты"), this);
    auto* layout = new QVBoxLayout(box);

    defects_list_ = new QListWidget(box);
    defects_list_->setMaximumHeight(kListHeight);
    layout->addWidget(defects_list_);

    defect_type_box_ = new QComboBox(box);
    fill_enum_combo<StainClass>(defect_type_box_, {StainClass::Light, StainClass::Hard, StainClass::Special});
    defect_description_edit_ = new QLineEdit(box);
    defect_description_edit_->setPlaceholderText(QStringLiteral("описание"));
    defect_area_edit_ = new QDoubleSpinBox(box);
    defect_area_edit_->setRange(kMinAreaCm2, kMaxAreaCm2);
    defect_area_edit_->setDecimals(kAreaDecimals);
    defect_area_edit_->setSingleStep(kAreaStepCm2);
    defect_area_edit_->setSuffix(QStringLiteral(" см²"));
    defect_area_edit_->setValue(kDefaultAreaCm2);

    auto* add_button = new QPushButton(QStringLiteral("Добавить"), box);
    auto* remove_button = new QPushButton(QStringLiteral("Удалить выбранное"), box);

    auto* row = new QHBoxLayout;
    row->addWidget(defect_type_box_);
    row->addWidget(defect_description_edit_, kStretch);
    row->addWidget(defect_area_edit_);
    row->addWidget(add_button);
    layout->addLayout(row);
    layout->addWidget(remove_button);

    connect(add_button, &QPushButton::clicked, this, &ItemDialog::add_defect);
    connect(remove_button, &QPushButton::clicked, this, &ItemDialog::remove_defect);
    return box;
}

void ItemDialog::show_defects() {
    defects_list_->clear();
    for (const Item::Defect& defect : defects_) {
        defects_list_->addItem(describe(defect));
    }
}

void ItemDialog::add_defect() {
    Item::Defect defect;
    defect.type = enum_value<StainClass>(defect_type_box_);
    defect.description = defect_description_edit_->text().toStdString();
    defect.area_cm2 = defect_area_edit_->value();
    try {
        Item probe;
        probe.add_defect(defect);
        defects_.push_back(probe.defects().front());
        error_label_->clear();
        defect_description_edit_->clear();
        show_defects();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}

void ItemDialog::remove_defect() {
    const std::optional<std::size_t> index = valid_index(defects_list_->currentRow(), defects_.size());
    if (!index) {
        return;
    }
    defects_.erase(defects_.begin() + static_cast<std::ptrdiff_t>(*index));
    show_defects();
}

Item ItemDialog::build_item() const {
    Item built(kind_edit_->text().toStdString(), material_edit_->text().toStdString(),
               enum_value<MaterialGroup>(material_group_box_), enum_value<FabricGroup>(fabric_group_box_),
               color_edit_->text().toStdString(), enum_value<ColorGroup>(color_group_box_), weight_edit_->value(),
               enum_value<SoilLevel>(soil_box_));
    built.set_condition_notes(notes_edit_->text().toStdString());
    for (const Item::Defect& defect : defects_) {
        built.add_defect(defect);
    }
    built.set_id(result_.id());
    built.set_order_id(result_.order_id());
    built.set_tag(result_.tag());
    built.set_batch_id(result_.batch_id());
    return built;
}

void ItemDialog::try_accept() {
    try {
        result_ = build_item();
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}

const Item& ItemDialog::item() const noexcept { return result_; }
