#include "gui/item_dialog.h"

#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

#include "display.h"
#include "exceptions.h"
#include "gui/enum_combo.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/suggestions.h"
#include "gui/table_utils.h"
#include "item_catalog.h"
#include "record_io.h"
#include "shop.h"

namespace {

constexpr int kDialogWidth = 620;
constexpr int kDefectListHeight = 130;
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

enum Step { ItemStep, ConditionStep, ServicesStep };

constexpr std::array<const char*, 3> kStepTitles{"Изделие", "Состояние", "Услуги и порядок обработки"};
constexpr std::array<const char*, 2> kStageHeaders{"Этап", "Услуга"};

void load_catalog(QComboBox* box, const auto& catalog, const std::string& selected, const QString& placeholder) {
    for (const auto& entry : catalog) {
        box->addItem(QString::fromUtf8(entry.name.data(), static_cast<qsizetype>(entry.name.size())));
    }
    box->setEditable(true);
    box->setInsertPolicy(QComboBox::NoInsert);
    box->setCurrentIndex(box->findText(qt_convert::text(selected)));
    box->lineEdit()->setPlaceholderText(placeholder);
    box->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    box->completer()->setFilterMode(Qt::MatchContains);
    box->completer()->setCompletionMode(QCompleter::PopupCompletion);
}

std::string catalog_value(const QComboBox* box, const char* field, const char* message) {
    const int index = box->findText(box->currentText().trimmed(), Qt::MatchFixedString);
    if (index < 0) throw InvalidInputException(field, message);
    return box->itemText(index).toStdString();
}

QString describe(const Item::Defect& defect) {
    const QString summary = QStringLiteral("%1 · %2 · %3 см²")
        .arg(qt_convert::text(defect.name), qt_convert::text(display_name(defect.type)),
             QString::number(defect.area_cm2, 'f', kAreaDecimals));
    return defect.description.empty() ? summary : summary + QStringLiteral(" — ") + qt_convert::text(defect.description);
}

}

ItemDialog::ItemDialog(const DryCleaningShop& shop, const Item& initial, const QString& title, QWidget* parent)
    : QDialog(parent), shop_(shop), defects_(initial.defects()), result_(initial) {
    setWindowTitle(title);
    setMinimumWidth(kDialogWidth);
    if (!initial.kind().empty()) {
        for (ServiceType type : initial.services()) stages_.push_back({next_stage_id_++, type});
    }

    step_label_ = new QLabel(this);
    auto heading = step_label_->font();
    heading.setBold(true);
    step_label_->setFont(heading);

    pages_ = new QStackedWidget(this);
    pages_->addWidget(build_item_page(initial));
    pages_->addWidget(build_condition_page(initial));
    pages_->addWidget(build_services_page());

    error_label_ = new QLabel(this);
    error_label_->setStyleSheet(kErrorStyle);
    error_label_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    back_button_ = buttons->addButton(QStringLiteral("Назад"), QDialogButtonBox::ActionRole);
    next_button_ = buttons->addButton(QStringLiteral("Далее"), QDialogButtonBox::AcceptRole);
    next_button_->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(step_label_);
    layout->addWidget(pages_, kStretch);
    layout->addWidget(error_label_);
    layout->addWidget(buttons);

    connect(back_button_, &QPushButton::clicked, this, [this] { show_step(pages_->currentIndex() - 1); });
    connect(buttons, &QDialogButtonBox::accepted, this, &ItemDialog::advance);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    show_defects();
    show_step(ItemStep);
}

QWidget* ItemDialog::build_item_page(const Item& initial) {
    auto* page = new QWidget(pages_);
    auto* form = new QFormLayout(page);

    kind_edit_ = new QLineEdit(qt_convert::text(initial.kind()), page);
    kind_edit_->setPlaceholderText(QStringLiteral("например, пальто"));
    set_suggestions(kind_edit_, input_suggestions(shop_, SuggestionKind::ItemName));
    material_box_ = new QComboBox(page);
    load_catalog(material_box_, kItemMaterials, initial.material_name(), QStringLiteral("Введите или выберите материал"));
    color_box_ = new QComboBox(page);
    load_catalog(color_box_, kItemColors, initial.color_name(), QStringLiteral("Введите или выберите цвет"));
    weight_edit_ = make_spin_box(page, kMinWeightKg, kMaxWeightKg, kWeightDecimals, kWeightStepKg,
                                QStringLiteral(" кг"),
                                initial.weight_kg() > 0.0 ? initial.weight_kg() : kDefaultWeightKg);

    form->addRow(QStringLiteral("Название"), kind_edit_);
    form->addRow(QStringLiteral("Материал"), material_box_);
    form->addRow(QStringLiteral("Цвет"), color_box_);
    form->addRow(QStringLiteral("Масса"), weight_edit_);
    return page;
}

QWidget* ItemDialog::build_condition_page(const Item& initial) {
    auto* page = new QWidget(pages_);
    auto* form = new QFormLayout;
    soil_box_ = new QComboBox(page);
    fill_enum_combo<SoilLevel>(soil_box_, {SoilLevel::Light, SoilLevel::Medium, SoilLevel::Heavy});
    select_enum(soil_box_, initial.soil_level());
    notes_edit_ = new QLineEdit(qt_convert::text(initial.condition_notes()), page);
    form->addRow(QStringLiteral("Загрязнение"), soil_box_);
    form->addRow(QStringLiteral("Примечания"), notes_edit_);

    auto* box = new QGroupBox(QStringLiteral("Пятна и дефекты"), page);
    auto* defect_form = new QFormLayout;
    defect_name_edit_ = new QLineEdit(box);
    defect_name_edit_->setPlaceholderText(QStringLiteral("например, кофе"));
    set_suggestions(defect_name_edit_, input_suggestions(shop_, SuggestionKind::DefectName));
    defect_description_edit_ = new QLineEdit(box);
    defect_description_edit_->setPlaceholderText(QStringLiteral("необязательно"));
    defect_type_box_ = new QComboBox(box);
    fill_enum_combo<StainClass>(defect_type_box_, {StainClass::Light, StainClass::Hard, StainClass::Special});
    defect_area_edit_ = make_spin_box(box, kMinAreaCm2, kMaxAreaCm2, kAreaDecimals, kAreaStepCm2,
                                     QStringLiteral(" см²"), kDefaultAreaCm2);
    defect_form->addRow(QStringLiteral("Короткое название"), defect_name_edit_);
    defect_form->addRow(QStringLiteral("Описание"), defect_description_edit_);
    defect_form->addRow(QStringLiteral("Класс пятна"), defect_type_box_);
    defect_form->addRow(QStringLiteral("Площадь"), defect_area_edit_);

    auto* add_button = new QPushButton(QStringLiteral("Добавить пятно или дефект"), box);
    defects_list_ = new QListWidget(box);
    defects_list_->setMaximumHeight(kDefectListHeight);
    defects_list_->setAccessibleName(QStringLiteral("Добавленные пятна и дефекты"));
    auto* defect_layout = new QVBoxLayout(box);
    defect_layout->addLayout(defect_form);
    defect_layout->addWidget(add_button, 0, Qt::AlignLeft);
    defect_layout->addWidget(defects_list_);

    connect(add_button, &QPushButton::clicked, this, &ItemDialog::add_defect);

    auto* layout = new QVBoxLayout(page);
    layout->addLayout(form);
    layout->addWidget(box);
    layout->addStretch();
    return page;
}

QWidget* ItemDialog::build_services_page() {
    auto* page = new QWidget(pages_);
    service_box_ = new QComboBox(page);
    service_box_->setAccessibleName(QStringLiteral("Услуга"));
    fill_enum_combo<ServiceType>(service_box_, {ServiceType::DryCleaning, ServiceType::WetCleaning,
                                               ServiceType::StainRemoval, ServiceType::Leather});
    add_stage_button_ = new QPushButton(QStringLiteral("Добавить этап"), page);
    service_hint_ = new QLabel(page);
    service_hint_->setWordWrap(true);
    stages_hint_ = new QLabel(page);
    stages_hint_->setWordWrap(true);

    auto* controls = new QHBoxLayout;
    controls->addWidget(service_box_, kStretch);
    controls->addWidget(add_stage_button_);

    stages_table_ = new TableWidget(make_headers(kStageHeaders), page, 1, true);
    stages_table_->set_empty_text(QStringLiteral("Выберите услугу и нажмите «Добавить этап»."));
    auto* order_hint = new QLabel(QStringLiteral(
        "Этапы выполняются по номерам. В базовом порядке они показаны сверху вниз."), page);
    order_hint->setWordWrap(true);

    auto* layout = new QVBoxLayout(page);
    layout->addLayout(controls);
    layout->addWidget(service_hint_);
    layout->addWidget(order_hint);
    layout->addWidget(stages_table_, kStretch);
    layout->addWidget(stages_hint_);

    connect(add_stage_button_, &QPushButton::clicked, this, &ItemDialog::add_stage);
    connect(service_box_, &QComboBox::currentIndexChanged, this, &ItemDialog::refresh_service_choice);
    return page;
}

void ItemDialog::show_step(int page) {
    pages_->setCurrentIndex(page);
    step_label_->setText(QStringLiteral("Шаг %1 из %2 · %3").arg(page + 1).arg(kStepTitles.size())
                             .arg(QString::fromUtf8(kStepTitles[page])));
    back_button_->setEnabled(page != ItemStep);
    next_button_->setText(page == ServicesStep ? QStringLiteral("Готово") : QStringLiteral("Далее"));
    error_label_->clear();
    if (page == ServicesStep) {
        refresh_stages();
        service_box_->setFocus();
    } else {
        next_button_->setEnabled(true);
        if (page == ItemStep) kind_edit_->setFocus();
        else soil_box_->setFocus();
    }
}

Item ItemDialog::build_details() const {
    if (kind_edit_->text().trimmed().isEmpty()) {
        throw InvalidInputException("kind", "Введите название изделия.");
    }
    auto material = catalog_value(material_box_, "material", "Выберите материал из списка.");
    auto color = catalog_value(color_box_, "color", "Выберите цвет из списка.");
    Item item(kind_edit_->text().toStdString(), std::move(material), std::move(color),
              weight_edit_->value(), enum_value<SoilLevel>(soil_box_));
    item.set_condition_notes(notes_edit_->text().toStdString());
    for (const Item::Defect& defect : defects_) item.add_defect(defect);
    return item;
}

void ItemDialog::advance() {
    try {
        const int page = pages_->currentIndex();
        if (page == ConditionStep && (!defect_name_edit_->text().trimmed().isEmpty() ||
                                     !defect_description_edit_->text().trimmed().isEmpty())) {
            throw InvalidInputException("defect", "Добавьте введённое пятно или дефект либо очистите поля.");
        }
        Item item = build_details();
        if (page != ServicesStep) {
            if (page == ConditionStep) service_item_ = std::move(item);
            show_step(page + 1);
            return;
        }

        std::vector<ServiceType> services;
        for (const Stage& stage : stages_) {
            if (!shop_.service(stage.service).is_applicable(item)) {
                throw InvalidInputException("services", "Уберите недоступные этапы или измените данные изделия.");
            }
            services.push_back(stage.service);
        }
        item.set_services(std::move(services));
        item.set_id(result_.id());
        item.set_order_id(result_.order_id());
        item.set_tag(result_.tag());
        item.set_batch_id(result_.batch_id());
        result_ = std::move(item);
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}

void ItemDialog::show_defects() {
    defects_list_->clear();
    for (const Item::Defect& defect : defects_) {
        const QString description = describe(defect);
        auto* item = new QListWidgetItem(defects_list_);
        item->setData(Qt::AccessibleTextRole, description);
        auto* row = new QWidget(defects_list_);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(4, 1, 4, 1);
        auto* label = new QLabel(description, row);
        label->setTextFormat(Qt::PlainText);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setToolTip(description);
        layout->addWidget(label, 1);
        add_remove_action(layout, QStringLiteral("Удалить пятно или дефект «%1»").arg(qt_convert::text(defect.name)),
                          [this, item] { remove_defect(item); });
        item->setSizeHint(row->minimumSizeHint());
        defects_list_->setItemWidget(item, row);
    }
}

void ItemDialog::add_defect() {
    Item::Defect defect;
    defect.type = enum_value<StainClass>(defect_type_box_);
    defect.name = defect_name_edit_->text().toStdString();
    defect.description = defect_description_edit_->text().toStdString();
    defect.area_cm2 = defect_area_edit_->value();
    try {
        if (defect_name_edit_->text().trimmed().isEmpty()) {
            throw InvalidInputException("defect_name", "Введите короткое название пятна или дефекта.");
        }
        Item probe;
        probe.add_defect(std::move(defect));
        defects_.push_back(probe.defects().front());
        defect_name_edit_->clear();
        defect_description_edit_->clear();
        error_label_->clear();
        show_defects();
        defect_name_edit_->setFocus();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}

void ItemDialog::remove_defect(QListWidgetItem* item) {
    const int row = defects_list_->row(item);
    if (row < 0) return;
    defects_.erase(defects_.begin() + row);
    delete defects_list_->takeItem(row);
}

QString ItemDialog::service_block(ServiceType service) const {
    if (shop_.service(service).is_applicable(service_item_)) return {};
    if (service == ServiceType::StainRemoval) {
        return QStringLiteral("Добавьте пятно на шаге «Состояние».");
    }
    if (service == ServiceType::Leather) {
        return QStringLiteral("Эта услуга подходит для изделий из кожи, замши, овчины, велюра и меха.");
    }
    return QStringLiteral("Для этого материала выберите услугу «Кожа и мех».");
}

void ItemDialog::refresh_service_choice() {
    const auto blocked = service_block(enum_value<ServiceType>(service_box_));
    const bool has_room = stages_.size() < kMaxNestedElements;
    add_stage_button_->setEnabled(blocked.isEmpty() && has_room);
    const QString hint = has_room ? blocked : QStringLiteral("Достигнут предел числа этапов.");
    service_hint_->setText(hint);
    service_hint_->setVisible(!hint.isEmpty());
    add_stage_button_->setToolTip(hint);
    add_stage_button_->setAccessibleDescription(hint);
}

void ItemDialog::refresh_stages() {
    {
        const QSignalBlocker blocker(service_box_);
        for (int index = 0; index < service_box_->count(); ++index) {
            const auto type = static_cast<ServiceType>(service_box_->itemData(index).toInt());
            const auto blocked = service_block(type);
            auto label = qt_convert::text(display_name(type));
            if (!blocked.isEmpty()) label += QStringLiteral(" — недоступна");
            service_box_->setItemText(index, label);
            service_box_->setItemData(index, blocked, Qt::ToolTipRole);
        }
    }
    refresh_service_choice();

    stages_table_->reset_rows(static_cast<int>(stages_.size()));
    bool has_unavailable = false;
    for (int row = 0; row < stages_table_->rowCount(); ++row) {
        const auto& [id, service] = stages_[row];
        set_cell(stages_table_, row, 0, row + 1);
        const auto blocked = service_block(service);
        auto name = qt_convert::text(display_name(service));
        if (!blocked.isEmpty()) {
            has_unavailable = true;
            name += QStringLiteral(" — недоступна");
        }
        set_cell(stages_table_, row, 1, name);
        if (!blocked.isEmpty()) stages_table_->item(row, 1)->setToolTip(blocked);

        auto* actions = make_row_actions(stages_table_, row, id);
        auto* up = add_row_action(actions, QStringLiteral("↑"), [this, id] { move_stage(id, -1); },
                                  row == 0 ? QStringLiteral("Этап уже первый.") : QString());
        up->setAccessibleName(QStringLiteral("Поднять этап"));
        if (up->isEnabled()) up->setToolTip(up->accessibleName());
        auto* down = add_row_action(actions, QStringLiteral("↓"), [this, id] { move_stage(id, 1); },
                                    row + 1 == stages_table_->rowCount()
                                        ? QStringLiteral("Этап уже последний.") : QString());
        down->setAccessibleName(QStringLiteral("Опустить этап"));
        if (down->isEnabled()) down->setToolTip(down->accessibleName());
        add_remove_action(actions, QStringLiteral("Удалить этап"), [this, id] { remove_stage(id); });
    }
    stages_table_->finish_rows();
    const QString hint = has_unavailable
        ? QStringLiteral("Уберите недоступные этапы или измените данные изделия.")
        : QString();
    stages_hint_->setText(hint);
    stages_hint_->setVisible(!hint.isEmpty());
    next_button_->setEnabled(!stages_.empty() && !has_unavailable);
}

void ItemDialog::add_stage() {
    const auto service = enum_value<ServiceType>(service_box_);
    if (!service_block(service).isEmpty() || stages_.size() >= kMaxNestedElements) return;
    stages_.push_back({next_stage_id_++, service});
    refresh_stages();
}

void ItemDialog::move_stage(Id id, int offset) {
    const auto entry = std::ranges::find(stages_, id, &Stage::id);
    if (entry == stages_.end()) return;
    const auto target = std::distance(stages_.begin(), entry) + offset;
    if (target < 0 || target >= static_cast<std::ptrdiff_t>(stages_.size())) return;
    std::iter_swap(entry, stages_.begin() + target);
    stages_table_->reset_sort();
    refresh_stages();
}

void ItemDialog::remove_stage(Id id) {
    const auto entry = std::ranges::find(stages_, id, &Stage::id);
    if (entry == stages_.end()) return;
    stages_.erase(entry);
    refresh_stages();
}

const Item& ItemDialog::item() const noexcept { return result_; }
