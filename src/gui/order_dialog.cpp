#include "gui/order_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

#include "display.h"
#include "exceptions.h"
#include "gui/enum_combo.h"
#include "gui/item_dialog.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"
#include "normalize.h"

namespace {

constexpr int kDialogWidth = 900;
constexpr int kDialogHeight = 600;
constexpr int kWeightDecimals = 2;
constexpr const char* kNoCost = "-";

enum ItemColumn { ItemKind, ItemMaterial, ItemColor, ItemWeight, ItemSoil, ItemStains, ItemCost, ItemService, ItemColumnCount };

constexpr std::array<const char*, ItemColumnCount> kItemHeaders{"Изделие", "Материал", "Цвет", "Масса",
                                                                "Состояние", "Пятен", "Цена", "Услуги"};

}

OrderDialog::OrderDialog(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QDialog(parent), shop_(shop), employee_id_(employee_id) {
    setWindowTitle(QStringLiteral("Приём заказа"));
    resize(kDialogWidth, kDialogHeight);

    auto* form = new QFormLayout;
    build_header(form);

    summary_label_ = new QLabel(this);
    error_label_ = new QLabel(this);
    style_error_label(error_label_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    accept_button_ = buttons->button(QDialogButtonBox::Ok);
    accept_button_->setText(QStringLiteral("Принять заказ"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(build_items_box(), kStretch);
    layout->addWidget(summary_label_);
    layout->addWidget(error_label_);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &OrderDialog::try_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(client_box_, &QComboBox::currentIndexChanged, this, &OrderDialog::refresh);
    connect(client_box_, &QComboBox::editTextChanged, this, &OrderDialog::refresh);
    connect(urgent_box_, &QCheckBox::toggled, this, &OrderDialog::refresh);
    refresh();
}

void OrderDialog::build_header(QFormLayout* form) {
    client_box_ = new QComboBox(this);
    for (const Client& client : shop_.search_clients(std::string())) {
        const QString label = qt_convert::text(client.name()) + QStringLiteral(" - ") +
                              qt_convert::text(format_phone(client.phone()));
        client_box_->addItem(label, static_cast<unsigned int>(client.id()));
    }

    client_box_->setEditable(true);
    client_box_->setInsertPolicy(QComboBox::NoInsert);
    client_box_->lineEdit()->setPlaceholderText(QStringLiteral("Начните вводить имя клиента"));
    client_box_->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    client_box_->completer()->setFilterMode(Qt::MatchStartsWith);
    client_box_->completer()->setCompletionMode(QCompleter::PopupCompletion);

    urgent_box_ = new QCheckBox(QStringLiteral("Срочный заказ"), this);

    form->addRow(QStringLiteral("Клиент:"), client_box_);
    form->addRow(QString(), urgent_box_);
}

QWidget* OrderDialog::build_items_box() {
    auto* box = new QGroupBox(QStringLiteral("Изделия"), this);
    auto* layout = new QVBoxLayout(box);

    auto* add_button = new QPushButton(QStringLiteral("Добавить изделие"), box);
    layout->addWidget(add_button, 0, Qt::AlignLeft);
    items_table_ = new TableWidget(make_headers(kItemHeaders), box, true);
    items_table_->set_text_columns({ItemKind, ItemMaterial});
    items_table_->set_text_columns({ItemService}, ColumnKind::WideText);
    items_table_->set_empty_text(QStringLiteral("Нажмите «Добавить изделие», чтобы заполнить заказ."));
    layout->addWidget(items_table_);

    connect(add_button, &QPushButton::clicked, this, &OrderDialog::add_item);
    return box;
}

Id OrderDialog::accepted_order_id() const noexcept { return accepted_id_; }

Id OrderDialog::selected_client() const {
    const int index = client_box_->currentIndex();
    if (index < 0 || client_box_->currentText() != client_box_->itemText(index)) return 0;
    return client_box_->itemData(index).toUInt();
}

OrderDraft OrderDialog::make_draft() const {
    OrderDraft draft;
    draft.client_id = selected_client();
    draft.items.reserve(items_.size());
    for (const DraftItem& entry : items_) {
        draft.items.push_back(entry.item);
    }
    draft.urgent = urgent_box_->isChecked();
    return draft;
}

std::optional<OrderQuote> OrderDialog::make_quote() {
    error_label_->clear();
    if (items_.empty() || selected_client() == 0) {
        return std::nullopt;
    }
    try {
        return shop_.quote_order(make_draft(), qt_convert::now());
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
        return std::nullopt;
    }
}

void OrderDialog::refresh() {
    const std::optional<OrderQuote> quote = make_quote();
    show_items(quote);
    show_summary(quote);
    accept_button_->setEnabled(quote.has_value());
}

void OrderDialog::show_items(const std::optional<OrderQuote>& quote) {
    items_table_->reset_rows(static_cast<int>(items_.size()));
    for (std::size_t index = 0; index < items_.size(); ++index) {
        const auto& [id, item] = items_[index];
        const int row = static_cast<int>(index);
        const QString cost = quote ? qt_convert::text(format_money(quote->item_costs[index])) : QString::fromUtf8(kNoCost);
        set_cell(items_table_, row, ItemKind, qt_convert::text(item.kind()));
        set_cell(items_table_, row, ItemMaterial, qt_convert::text(item.material_name()));
        set_cell(items_table_, row, ItemColor, qt_convert::text(item.color_name()));
        set_sortable_cell(items_table_, row, ItemWeight, QString::number(item.weight_kg(), 'f', kWeightDecimals),
                          item.weight_kg(), true);
        set_cell(items_table_, row, ItemSoil, qt_convert::text(display_name(item.soil_level())));
        set_cell(items_table_, row, ItemStains, item.stain_count());
        set_sortable_cell(items_table_, row, ItemCost, cost, quote ? QVariant(quote->item_costs[index]) : QVariant(),
                          true);
        set_cell(items_table_, row, ItemService, item_services_text(item));
        auto* actions = make_row_actions(items_table_, row, id);
        add_edit_action(actions, QStringLiteral("Изменить изделие «%1»").arg(qt_convert::text(item.kind())),
                        [this, id] { edit_item(id); });
        add_remove_action(actions, QStringLiteral("Удалить изделие «%1»").arg(qt_convert::text(item.kind())),
                          [this, id] { remove_item(id); });
    }
    items_table_->finish_rows();
}

void OrderDialog::show_summary(const std::optional<OrderQuote>& quote) {
    if (!quote) {
        if (client_box_->count() == 0) {
            summary_label_->setText(QStringLiteral("Добавьте клиента на вкладке «Клиенты», затем откройте приём заказа снова."));
        } else if (selected_client() == 0) {
            summary_label_->setText(QStringLiteral("Выберите клиента из списка по имени."));
        } else if (items_.empty()) {
            summary_label_->setText(QStringLiteral("Добавьте хотя бы одно изделие, чтобы принять заказ."));
        } else {
            summary_label_->clear();
        }
        return;
    }
    summary_label_->setText(QStringLiteral("Скидка клиента: %1\nИтого: %2\nСрок выполнения: %3\nГотовность не позже: %4")
                                .arg(qt_convert::text(format_percent(quote->discount)),
                                     qt_convert::text(format_money(quote->total_cost)),
                                     qt_convert::text(format_hours(quote->nominal_hours)),
                                     qt_convert::format_time(quote->deadline)));
}

void OrderDialog::add_item() {
    ItemDialog dialog(shop_, Item(), QStringLiteral("Новое изделие"), this);
    if (dialog.exec() == QDialog::Accepted) {
        items_.push_back({next_item_id_++, dialog.item()});
        refresh();
    }
}

void OrderDialog::edit_item(Id id) {
    const auto entry = std::ranges::find(items_, id, &DraftItem::id);
    if (entry == items_.end()) {
        show_action_refusal(this, QStringLiteral("Изделие уже удалено из заказа."));
        return;
    }
    ItemDialog dialog(shop_, entry->item, QStringLiteral("Изменение изделия"), this);
    if (dialog.exec() == QDialog::Accepted) {
        entry->item = dialog.item();
        refresh();
    }
}

void OrderDialog::remove_item(Id id) {
    const auto entry = std::ranges::find(items_, id, &DraftItem::id);
    if (entry == items_.end()) {
        show_action_refusal(this, QStringLiteral("Изделие уже удалено из заказа."));
        return;
    }
    if (!confirm(this, QStringLiteral("Удаление изделия"),
                 QStringLiteral("Удалить изделие «%1» из заказа?").arg(qt_convert::text(entry->item.kind())))) {
        return;
    }
    items_.erase(entry);
    refresh();
}

void OrderDialog::try_accept() {
    try {
        accepted_id_ = shop_.accept_order(make_draft(), employee_id_, qt_convert::now());
        accept();
    } catch (const ShopException& error) {
        error_label_->setText(qt_convert::text(error.what()));
    }
}
