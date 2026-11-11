#include "gui/materials_tab.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <array>
#include <set>

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
constexpr double kMinRestock = 0.01;
constexpr double kMaxRestock = 1000000.0;
constexpr double kDefaultRestock = 1.0;
constexpr const char* kServiceSeparator = ", ";

enum SortMode { SortByName, SortByStock };

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
    sort_box_->addItem(QStringLiteral("По названию"), SortByName);
    sort_box_->addItem(QStringLiteral("По остатку"), SortByStock);

    restock_button_ = new QPushButton(QStringLiteral("Пополнить"), this);

    toolbar_->insertWidget(kSearchPosition, search_edit_, kStretch);
    toolbar_->insertWidget(kSortPosition, sort_box_);
    toolbar_->addWidget(restock_button_);

    connect(search_edit_, &QLineEdit::textChanged, this, &TableTab::refresh);
    connect(sort_box_, &QComboBox::currentIndexChanged, this, &TableTab::refresh);
    connect(restock_button_, &QPushButton::clicked, this, &MaterialsTab::restock);
}

void MaterialsTab::fill(QTableWidget* table) {
    std::set<Id> matching;
    for (const Material& material : shop_.find_materials_by_name(search_edit_->text().toStdString())) {
        matching.insert(material.id());
    }
    const std::vector<Material> sorted = sort_box_->currentData().toInt() == SortByStock
                                             ? shop_.materials_sorted_by_stock()
                                             : shop_.materials_sorted_by_name();
    table->setRowCount(static_cast<int>(matching.size()));
    int row = 0;
    for (const Material& material : sorted) {
        if (!matching.contains(material.id())) {
            continue;
        }
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
    restock_button_->setEnabled(is_allowed() && selected().has_value());
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

void MaterialsTab::restock() {
    const std::optional<Id> id = selected();
    if (!id) {
        return;
    }
    const Material& material = shop_.materials().get(*id);
    bool confirmed = false;
    const double amount = QInputDialog::getDouble(
        this, QStringLiteral("Пополнение склада"),
        QStringLiteral("Сколько добавить (%1):").arg(qt_convert::text(material.unit())), kDefaultRestock, kMinRestock,
        kMaxRestock, kStockDecimals, &confirmed);
    if (confirmed && run_guarded(this, [&] { shop_.restock_material(*id, amount, employee_id_); })) {
        emit changed();
    }
}
