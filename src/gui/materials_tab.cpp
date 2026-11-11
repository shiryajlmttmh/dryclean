#include "gui/materials_tab.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <array>

#include "display.h"
#include "gui/helpers.h"
#include "gui/material_dialog.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kStockDecimals = 2;
constexpr int kSearchPosition = 0;
constexpr int kSortPosition = 1;
constexpr int kConsumptionDecimals = 3;
constexpr double kMinAmount = 0.01;
constexpr double kMaxAmount = 1000000.0;
constexpr double kDefaultAmount = 1.0;
constexpr const char* kServiceSeparator = ", ";

enum MaterialColumn {
    MaterialId,
    MaterialName,
    MaterialUnit,
    MaterialStock,
    MaterialConsumption,
    MaterialServices,
    MaterialColumnCount
};

constexpr std::array<const char*, MaterialColumnCount> kMaterialHeaders{"№", "Название", "Единица", "Остаток",
                                                                        "Расход на кг", "Услуги"};

QString services_text(const Material& material) {
    QStringList names;
    for (const ServiceType service : material.services()) {
        names << qt_convert::text(display_name(service));
    }
    return names.join(QString::fromUtf8(kServiceSeparator));
}

}

MaterialsTab::MaterialsTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : TableTab(shop, employee_id, Permission::ManageStock, make_headers(kMaterialHeaders), parent) {
    search_edit_ = new QLineEdit(this);
    search_edit_->setPlaceholderText(QStringLiteral("Поиск по названию"));
    search_edit_->setClearButtonEnabled(true);

    sort_box_ = new QComboBox(this);
    sort_box_->addItem(QStringLiteral("По названию"), static_cast<int>(MaterialOrder::ByName));
    sort_box_->addItem(QStringLiteral("По остатку"), static_cast<int>(MaterialOrder::ByStock));

    restock_button_ = new QPushButton(QStringLiteral("Пополнить"), this);
    write_off_button_ = new QPushButton(QStringLiteral("Списать"), this);

    toolbar_->insertWidget(kSearchPosition, search_edit_, kStretch);
    toolbar_->insertWidget(kSortPosition, sort_box_);
    toolbar_->addWidget(restock_button_);
    toolbar_->addWidget(write_off_button_);

    connect(search_edit_, &QLineEdit::textChanged, this, &TableTab::refresh);
    connect(sort_box_, &QComboBox::currentIndexChanged, this, &TableTab::refresh);
    connect(restock_button_, &QPushButton::clicked, this, &MaterialsTab::restock);
    connect(write_off_button_, &QPushButton::clicked, this, &MaterialsTab::write_off);
}

void MaterialsTab::fill(QTableWidget* table) {
    const std::vector<Material> found = shop_.find_materials(
        search_edit_->text().toStdString(), static_cast<MaterialOrder>(sort_box_->currentData().toInt()));
    table->setRowCount(static_cast<int>(found.size()));
    int row = 0;
    for (const Material& material : found) {
        set_cell(table, row, MaterialId, QString::number(material.id()), true);
        set_cell(table, row, MaterialName, qt_convert::text(material.name()));
        set_cell(table, row, MaterialUnit, qt_convert::text(material.unit()));
        set_cell(table, row, MaterialStock, QString::number(material.stock(), 'f', kStockDecimals), true);
        set_cell(table, row, MaterialConsumption, QString::number(material.consumption_per_kg(), 'f', kConsumptionDecimals),
                 true);
        set_cell(table, row, MaterialServices, services_text(material));
        ++row;
    }
}

void MaterialsTab::update_buttons() {
    TableTab::update_buttons();
    const bool can_change = is_allowed() && selected().has_value();
    restock_button_->setEnabled(can_change);
    write_off_button_->setEnabled(can_change);
}

void MaterialsTab::add_record() {
    MaterialDialog dialog(
        QStringLiteral("Новый материал"), Material(), true,
        [this](const Material& material) { shop_.add_material(material, employee_id_); }, this);
    show_dialog(dialog);
}

void MaterialsTab::edit_record(Id id) {
    MaterialDialog dialog(
        QStringLiteral("Изменение материала"), shop_.materials().get(id), false,
        [this, id](const Material& material) {
            Material updated = material;
            updated.set_id(id);
            shop_.update_material(updated, employee_id_);
        },
        this);
    show_dialog(dialog);
}

void MaterialsTab::change_stock(const QString& title, const QString& prompt,
                                const std::function<void(Id, double)>& apply) {
    const std::optional<Id> id = selected();
    if (!id) {
        return;
    }
    bool confirmed = false;
    const double amount =
        QInputDialog::getDouble(this, title, prompt.arg(qt_convert::text(shop_.materials().get(*id).unit())),
                                kDefaultAmount, kMinAmount, kMaxAmount, kStockDecimals, &confirmed);
    if (confirmed && run_guarded(this, [&] { apply(*id, amount); })) {
        emit changed();
    }
}

void MaterialsTab::restock() {
    change_stock(QStringLiteral("Пополнение склада"), QStringLiteral("Сколько добавить (%1):"),
                 [this](Id id, double amount) { shop_.restock_material(id, amount, employee_id_); });
}

void MaterialsTab::write_off() {
    change_stock(QStringLiteral("Списание со склада"), QStringLiteral("Сколько списать (%1):"),
                 [this](Id id, double amount) { shop_.write_off_material(id, amount, employee_id_); });
}
