#include "gui/order_items_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <array>
#include <algorithm>

#include "display.h"
#include "gui/helpers.h"
#include "gui/item_dialog.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kDialogWidth = 1180;
constexpr int kDialogHeight = 420;
constexpr int kWeightDecimals = 2;

enum ItemColumn {
    ItemId,
    ItemTag,
    ItemKind,
    ItemMaterial,
    ItemColor,
    ItemWeight,
    ItemSoil,
    ItemStains,
    ItemCost,
    ItemService,
    ItemProgress,
    ItemColumnCount
};

constexpr std::array<const char*, ItemColumnCount> kItemHeaders{"№", "Метка", "Изделие", "Материал", "Цвет",
                                                                "Масса, кг", "Состояние", "Пятен", "Цена", "Услуги", "Этапы"};

}

OrderItemsDialog::OrderItemsDialog(DryCleaningShop& shop, Id employee_id, Id order_id, QWidget* parent)
    : QDialog(parent), shop_(shop), employee_id_(employee_id), order_id_(order_id) {
    setWindowTitle(QStringLiteral("Изделия заказа №%1").arg(order_id));
    resize(kDialogWidth, kDialogHeight);

    summary_label_ = new QLabel(this);
    table_ = new TableWidget(make_headers(kItemHeaders), this, true);
    table_->set_text_columns({ItemKind, ItemMaterial});
    table_->set_text_columns({ItemService}, ColumnKind::WideText);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрыть"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summary_label_);
    layout->addWidget(table_, kStretch);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refresh();
}

QString OrderItemsDialog::edit_block() const {
    if (!has_permission(shop_, employee_id_, Permission::AcceptOrders)) {
        return QStringLiteral("Для изменения изделий нужны права приёмщика.");
    }
    return shop_.is_order_editable(order_id_) ? QString()
        : QStringLiteral("Изделия можно изменить до передачи заказа в обработку.");
}

void OrderItemsDialog::refresh() {
    const Order& order = shop_.orders().get(order_id_);
    summary_label_->setText(QStringLiteral("Статус: %1. Итого: %2. Срок: %3.")
                                .arg(qt_convert::text(display_name(order.status())),
                                     qt_convert::text(format_money(order.total_cost())),
                                     qt_convert::format_time(order.deadline())));
    const std::optional<Id> selected = selected_id(table_, ItemId);
    const QSignalBlocker blocker(table_);
    const auto unavailable = edit_block();
    table_->reset_rows(static_cast<int>(order.item_ids().size()));
    int row = 0;
    std::vector<Id> item_ids = order.item_ids();
    std::stable_sort(item_ids.begin(), item_ids.end(), [this](Id left, Id right) {
        return QString::localeAwareCompare(qt_convert::text(shop_.items().get(left).tag()),
                                           qt_convert::text(shop_.items().get(right).tag())) < 0;
    });
    for (const Id id : item_ids) {
        const Item& item = shop_.items().get(id);
        set_cell(table_, row, ItemId, id);
        set_cell(table_, row, ItemTag, qt_convert::text(item.tag()));
        set_cell(table_, row, ItemKind, qt_convert::text(item.kind()));
        set_cell(table_, row, ItemMaterial, qt_convert::text(item.material_name()));
        set_cell(table_, row, ItemColor, qt_convert::text(item.color_name()));
        set_sortable_cell(table_, row, ItemWeight, QString::number(item.weight_kg(), 'f', kWeightDecimals),
                          item.weight_kg(), true);
        set_cell(table_, row, ItemSoil, qt_convert::text(display_name(item.soil_level())));
        set_cell(table_, row, ItemStains, item.stain_count());
        const double cost = shop_.calculate_item_cost(id);
        set_sortable_cell(table_, row, ItemCost, qt_convert::text(format_money(cost)), cost, true);
        set_cell(table_, row, ItemService, item_services_text(item));
        const auto completed = static_cast<qulonglong>(item.completed_batches().size());
        set_sortable_cell(table_, row, ItemProgress, QStringLiteral("%1 из %2").arg(completed).arg(item.services().size()),
                          completed);
        add_edit_action(make_row_actions(table_, row, id),
                        QStringLiteral("Изменить изделие %1").arg(qt_convert::text(item.tag())),
                        [this, id] { edit_item(id); }, unavailable);
        ++row;
    }
    table_->finish_rows();
    restore_selected_id(table_, ItemId, selected);
}

void OrderItemsDialog::edit_item(Id id) {
    const auto unavailable = edit_block();
    if (!unavailable.isEmpty()) {
        show_action_refusal(this, unavailable);
        refresh();
        return;
    }
    ItemDialog dialog(shop_, shop_.items().get(id), QStringLiteral("Изменение изделия"), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    run_guarded(this, [&] { shop_.update_item(dialog.item(), employee_id_); });
    refresh();
}
