#include "gui/materials_tab.h"

#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QStringList>
#include <array>

#include "display.h"
#include "gui/helpers.h"
#include "gui/material_dialog.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"
#include "gui/suggestions.h"

namespace {

constexpr int kStockDecimals = 2;
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
    : TableTab(shop, employee_id, Permission::ManageStock, make_headers(kMaterialHeaders),
               {MaterialName, MaterialServices}, parent) {
    search_edit_ = new QLineEdit(this);
    search_edit_->setPlaceholderText(QStringLiteral("Поиск по названию"));
    search_edit_->setClearButtonEnabled(true);
    search_edit_->setAccessibleName(QStringLiteral("Поиск материалов"));

    sort_tabs_ = new QTabBar(this);
    sort_tabs_->setExpanding(false);
    sort_tabs_->setTabData(sort_tabs_->addTab(QStringLiteral("По названию")), static_cast<int>(MaterialOrder::ByName));
    sort_tabs_->setTabData(sort_tabs_->addTab(QStringLiteral("По остатку")), static_cast<int>(MaterialOrder::ByStock));

    auto* filters = new QHBoxLayout;
    filters->addWidget(search_edit_, kStretch);
    filters->addWidget(sort_tabs_);
    static_cast<QVBoxLayout*>(layout())->insertLayout(1, filters);

    connect(search_edit_, &QLineEdit::textChanged, this, &TableTab::refresh);
    connect(sort_tabs_, &QTabBar::currentChanged, this, &TableTab::refresh);
}

void MaterialsTab::fill(TableWidget* table) {
    set_suggestions(search_edit_, record_suggestions(shop_, SuggestionKind::MaterialName));
    const QString query = search_edit_->text().trimmed();
    if (!query.isEmpty()) {
        table->set_empty_text(QStringLiteral("Материалы не найдены. Измените поисковый запрос."));
    }
    const std::vector<Material> found = shop_.find_materials(
        query.toStdString(),
        static_cast<MaterialOrder>(sort_tabs_->tabData(sort_tabs_->currentIndex()).toInt()));
    table->reset_rows(static_cast<int>(found.size()));
    int row = 0;
    for (const Material& material : found) {
        set_cell(table, row, MaterialId, material.id());
        set_cell(table, row, MaterialName, qt_convert::text(material.name()));
        set_cell(table, row, MaterialUnit, qt_convert::text(material.unit()));
        set_sortable_cell(table, row, MaterialStock, QString::number(material.stock(), 'f', kStockDecimals),
                          material.stock(), true);
        set_sortable_cell(table, row, MaterialConsumption,
                          QString::number(material.consumption_per_kg(), 'f', kConsumptionDecimals),
                          material.consumption_per_kg(), true);
        set_cell(table, row, MaterialServices, services_text(material));
        ++row;
    }
}

void MaterialsTab::add_extra_actions(QHBoxLayout* actions, Id id) {
    if (!is_allowed()) return;
    auto* menu = add_more_actions(actions, QStringLiteral("Операции со складской записью №%1").arg(id));
    menu->addAction(QStringLiteral("Пополнить"), this, [this, id] { restock(id); });
    auto* write_off = menu->addAction(QStringLiteral("Списать"), this, [this, id] { this->write_off(id); });
    if (shop_.materials().get(id).stock() <= 0.0) {
        set_menu_action_unavailable(write_off, QStringLiteral("сначала пополните остаток"));
    }
}

void MaterialsTab::add_record() {
    MaterialDialog dialog(
        shop_,         QStringLiteral("Новый материал"), Material(), true,
        [this](const Material& material) { shop_.add_material(material, employee_id_); }, this);
    show_dialog(dialog);
}

void MaterialsTab::edit_record(Id id) {
    MaterialDialog dialog(
        shop_,         QStringLiteral("Изменение материала"), shop_.materials().get(id), false,
        [this, id](const Material& material) {
            Material updated = material;
            updated.set_id(id);
            shop_.update_material(updated, employee_id_);
        },
        this);
    show_dialog(dialog);
}

void MaterialsTab::remove_record(Id id) { shop_.remove_material(id, employee_id_); }

void MaterialsTab::change_stock(Id id, const QString& title, const QString& prompt,
                                const std::function<void(Id, double)>& apply) {
    QInputDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setLabelText(prompt.arg(qt_convert::text(shop_.materials().get(id).unit())));
    dialog.setInputMode(QInputDialog::DoubleInput);
    dialog.setDoubleRange(kMinAmount, kMaxAmount);
    dialog.setDoubleDecimals(kStockDecimals);
    dialog.setDoubleValue(kDefaultAmount);
    dialog.setOkButtonText(QStringLiteral("ОК"));
    dialog.setCancelButtonText(QStringLiteral("Отмена"));
    if (dialog.exec() == QDialog::Accepted && run_guarded(this, [&] { apply(id, dialog.doubleValue()); })) {
        emit changed();
    }
}

void MaterialsTab::restock(Id id) {
    if (!check_action_permission(this, shop_, employee_id_, Permission::ManageStock)) return;
    change_stock(id, QStringLiteral("Пополнение склада"), QStringLiteral("Сколько добавить (%1):"),
                 [this](Id material_id, double amount) { shop_.restock_material(material_id, amount, employee_id_); });
}

void MaterialsTab::write_off(Id id) {
    if (!check_action_permission(this, shop_, employee_id_, Permission::ManageStock)) return;
    if (shop_.materials().get(id).stock() <= 0.0) {
        show_action_refusal(this, QStringLiteral("Сначала пополните остаток материала."));
        refresh();
        return;
    }
    change_stock(id, QStringLiteral("Списание со склада"), QStringLiteral("Сколько списать (%1):"),
                 [this](Id material_id, double amount) { shop_.write_off_material(material_id, amount, employee_id_); });
}
