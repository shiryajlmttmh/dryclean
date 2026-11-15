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

    color_group_box_ = new QComboBox(this);
    fill_enum_combo<ColorGroup>(color_group_box_, {ColorGroup::Light, ColorGroup::Dark});
    select_enum(color_group_box_, initial.color_group());

    soil_box_ = new QComboBox(this);
    fill_enum_combo<SoilLevel>(soil_box_, {SoilLevel::Light, SoilLevel::Medium, SoilLevel::Heavy});
    select_enum(soil_box_, initial.soil_level());
    service_box_ = new QComboBox(this);
    fill_enum_combo<ServiceType>(service_box_, {ServiceType::DryCleaning, ServiceType::WetCleaning,
                                               ServiceType::StainRemoval, ServiceType::Leather});
    services_list_ = new QListWidget(this);
    services_list_->setMaximumHeight(kListHeight);
    for (ServiceType type : initial.services()) {
        auto* entry = new QListWidgetItem(qt_convert::text(display_name(type)), services_list_);
        entry->setData(Qt::UserRole, static_cast<int>(type));
    }
    auto* add_service = new QPushButton(QStringLiteral("Добавить"), this);
    auto* remove_service = new QPushButton(QStringLiteral("Удалить"), this);
    auto* move_up = new QPushButton(QStringLiteral("Выше"), this);
    auto* move_down = new QPushButton(QStringLiteral("Ниже"), this);
    auto* service_buttons = new QHBoxLayout;
    service_buttons->addWidget(add_service);
    service_buttons->addWidget(remove_service);
    service_buttons->addWidget(move_up);
    service_buttons->addWidget(move_down);
    connect(add_service, &QPushButton::clicked, this, [this] {
        auto* entry = new QListWidgetItem(service_box_->currentText(), services_list_);
        entry->setData(Qt::UserRole, static_cast<int>(enum_value<ServiceType>(service_box_)));
        services_list_->setCurrentItem(entry);
    });
    connect(remove_service, &QPushButton::clicked, this, [this] {
        delete services_list_->takeItem(services_list_->currentRow());
    });
    connect(move_up, &QPushButton::clicked, this, [this] {
        const int row = services_list_->currentRow();
        if (row <= 0) return;
        auto* entry = services_list_->takeItem(row);
        services_list_->insertItem(row - 1, entry);
        services_list_->setCurrentItem(entry);
    });
    connect(move_down, &QPushButton::clicked, this, [this] {
        const int row = services_list_->currentRow();
        if (row < 0 || row + 1 >= services_list_->count()) return;
        auto* entry = services_list_->takeItem(row);
        services_list_->insertItem(row + 1, entry);
        services_list_->setCurrentItem(entry);
    });

    weight_edit_ = new QDoubleSpinBox(this);
    weight_edit_->setRange(kMinWeightKg, kMaxWeightKg);
    weight_edit_->setDecimals(kWeightDecimals);
    weight_edit_->setSingleStep(kWeightStepKg);
    weight_edit_->setSuffix(QStringLiteral(" кг"));
    weight_edit_->setValue(initial.weight_kg() > 0.0 ? initial.weight_kg() : kDefaultWeightKg);

    form->addRow(QStringLiteral("Изделие:"), kind_edit_);
    form->addRow(QStringLiteral("Услуга:"), service_box_);
    form->addRow(QStringLiteral("Порядок обработки:"), services_list_);
    form->addRow(service_buttons);
    form->addRow(QStringLiteral("Материал:"), material_edit_);
    form->addRow(QStringLiteral("Группа материала:"), material_group_box_);
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
    for (std::size_t position = 0; position < defects_.size(); ++position) {
        add_positioned_item(defects_list_, describe(defects_[position]), position);
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
    const std::optional<std::size_t> index = selected_position(defects_list_, defects_.size());
    if (!index) {
        return;
    }
    defects_.erase(defects_.begin() + static_cast<std::ptrdiff_t>(*index));
    show_defects();
}

Item ItemDialog::build_item() const {
    Item built(kind_edit_->text().toStdString(), material_edit_->text().toStdString(),
               enum_value<MaterialGroup>(material_group_box_),
               color_edit_->text().toStdString(), enum_value<ColorGroup>(color_group_box_), weight_edit_->value(),
               enum_value<SoilLevel>(soil_box_));
    built.set_condition_notes(notes_edit_->text().toStdString());
    std::vector<ServiceType> services;
    for (int row = 0; row < services_list_->count(); ++row) {
        services.push_back(static_cast<ServiceType>(services_list_->item(row)->data(Qt::UserRole).toInt()));
    }
    built.set_services(std::move(services));
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
