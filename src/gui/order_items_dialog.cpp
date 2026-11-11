#include "gui/order_items_dialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <array>

#include "display.h"
#include "gui/helpers.h"
#include "gui/item_dialog.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kDialogWidth = 820;
constexpr int kDialogHeight = 420;
constexpr int kWeightDecimals = 2;

enum ItemColumn {
    ItemTag,
    ItemKind,
    ItemMaterial,
    ItemColor,
    ItemWeight,
    ItemSoil,
    ItemStains,
    ItemCost,
    ItemColumnCount
};

constexpr std::array<const char*, ItemColumnCount> kItemHeaders{"Метка", "Изделие", "Материал", "Цвет",
                                                                "Масса, кг", "Загрязнение", "Пятен", "Цена"};

}

OrderItemsDialog::OrderItemsDialog(DryCleaningShop& shop, Id employee_id, Id order_id, QWidget* parent)
    : QDialog(parent), shop_(shop), employee_id_(employee_id), order_id_(order_id) {
    setWindowTitle(QStringLiteral("Изделия заказа №%1").arg(order_id));
    resize(kDialogWidth, kDialogHeight);

    summary_label_ = new QLabel(this);
    table_ = new QTableWidget(this);
    configure_table(table_, make_headers(kItemHeaders));

    edit_button_ = new QPushButton(QStringLiteral("Изменить"), this);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрыть"));

    auto* row = new QHBoxLayout;
    row->addWidget(edit_button_);
    row->addStretch(kStretch);
    row->addWidget(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summary_label_);
    layout->addWidget(table_, kStretch);
    layout->addLayout(row);

    connect(edit_button_, &QPushButton::clicked, this, &OrderItemsDialog::edit_item);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &OrderItemsDialog::edit_item);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &OrderItemsDialog::refresh);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refresh();
}

bool OrderItemsDialog::can_edit() const {
    return has_permission(shop_, employee_id_, Permission::AcceptOrders) &&
           shop_.orders().get(order_id_).status() == OrderStatus::Accepted;
}

void OrderItemsDialog::refresh() {
    const Order& order = shop_.orders().get(order_id_);
    summary_label_->setText(QStringLiteral("Статус: %1. Итого: %2. Срок: %3.")
                                .arg(qt_convert::text(display_name(order.status())),
                                     qt_convert::text(format_money(order.total_cost())),
                                     qt_convert::format_time(order.deadline())));
    const std::optional<std::size_t> selected = valid_index(table_->currentRow(), order.item_ids().size());
    table_->blockSignals(true);
    table_->setRowCount(static_cast<int>(order.item_ids().size()));
    int row = 0;
    for (const Id id : order.item_ids()) {
        const Item& item = shop_.items().get(id);
        set_cell(table_, row, ItemTag, qt_convert::text(item.tag()));
        set_cell(table_, row, ItemKind, qt_convert::text(item.kind()));
        set_cell(table_, row, ItemMaterial, qt_convert::text(item.material_name()));
        set_cell(table_, row, ItemColor, qt_convert::text(item.color_name()));
        set_cell(table_, row, ItemWeight, QString::number(item.weight_kg(), 'f', kWeightDecimals), true);
        set_cell(table_, row, ItemSoil, qt_convert::text(display_name(item.soil_level())));
        set_cell(table_, row, ItemStains, QString::number(item.stain_count()), true);
        set_cell(table_, row, ItemCost, qt_convert::text(format_money(shop_.calculate_item_cost(id))), true);
        ++row;
    }
    if (selected) {
        table_->selectRow(static_cast<int>(*selected));
    }
    table_->blockSignals(false);
    edit_button_->setEnabled(can_edit() && valid_index(table_->currentRow(), order.item_ids().size()));
}

void OrderItemsDialog::edit_item() {
    const std::vector<Id>& ids = shop_.orders().get(order_id_).item_ids();
    const std::optional<std::size_t> index = valid_index(table_->currentRow(), ids.size());
    if (!index || !can_edit()) {
        return;
    }
    ItemDialog dialog(shop_.items().get(ids[*index]), QStringLiteral("Изменение изделия"), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    run_guarded(this, [&] { shop_.update_item(dialog.item(), employee_id_); });
    refresh();
}
