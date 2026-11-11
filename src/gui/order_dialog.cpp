#include "gui/order_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <array>

#include "display.h"
#include "exceptions.h"
#include "gui/enum_combo.h"
#include "gui/item_dialog.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kDialogWidth = 780;
constexpr int kDialogHeight = 600;
constexpr int kWeightDecimals = 2;
constexpr const char* kNoCost = "—";

enum ItemColumn { ItemKind, ItemMaterial, ItemColor, ItemWeight, ItemSoil, ItemStains, ItemCost, ItemColumnCount };

constexpr std::array<const char*, ItemColumnCount> kItemHeaders{"Изделие", "Материал", "Цвет", "Масса",
                                                                "Загрязнение", "Пятен", "Цена"};

}

OrderDialog::OrderDialog(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QDialog(parent), shop_(shop), employee_id_(employee_id) {
    setWindowTitle(QStringLiteral("Приём заказа"));
    resize(kDialogWidth, kDialogHeight);

    auto* form = new QFormLayout;
    build_header(form);

    summary_label_ = new QLabel(this);
    error_label_ = new QLabel(this);
    error_label_->setStyleSheet(kErrorStyle);
    error_label_->setWordWrap(true);

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
    connect(service_box_, &QComboBox::currentIndexChanged, this, &OrderDialog::refresh);
    connect(urgent_box_, &QCheckBox::toggled, this, &OrderDialog::refresh);
    refresh();
}

void OrderDialog::build_header(QFormLayout* form) {
    client_box_ = new QComboBox(this);
    for (const Client& client : shop_.search_clients(std::string())) {
        client_box_->addItem(qt_convert::text(client.name()) + QStringLiteral(" — ") + qt_convert::text(client.phone()),
                             static_cast<unsigned int>(client.id()));
    }

    service_box_ = new QComboBox(this);
    fill_enum_combo<ServiceType>(service_box_, {ServiceType::DryCleaning, ServiceType::WetCleaning,
                                                ServiceType::StainRemoval, ServiceType::Leather});

    urgent_box_ = new QCheckBox(QStringLiteral("Срочный заказ"), this);

    form->addRow(QStringLiteral("Клиент:"), client_box_);
    form->addRow(QStringLiteral("Услуга:"), service_box_);
    form->addRow(QString(), urgent_box_);
}

QWidget* OrderDialog::build_items_box() {
    auto* box = new QGroupBox(QStringLiteral("Изделия"), this);
    auto* layout = new QVBoxLayout(box);

    items_table_ = new QTableWidget(box);
    configure_table(items_table_, make_headers(kItemHeaders));
    layout->addWidget(items_table_);

    auto* add_button = new QPushButton(QStringLiteral("Добавить"), box);
    edit_button_ = new QPushButton(QStringLiteral("Изменить"), box);
    remove_button_ = new QPushButton(QStringLiteral("Удалить"), box);
    auto* row = new QHBoxLayout;
    row->addStretch(kStretch);
    row->addWidget(add_button);
    row->addWidget(edit_button_);
    row->addWidget(remove_button_);
    layout->addLayout(row);

    connect(add_button, &QPushButton::clicked, this, &OrderDialog::add_item);
    connect(edit_button_, &QPushButton::clicked, this, &OrderDialog::edit_item);
    connect(remove_button_, &QPushButton::clicked, this, &OrderDialog::remove_item);
    connect(items_table_, &QTableWidget::cellDoubleClicked, this, &OrderDialog::edit_item);
    connect(items_table_, &QTableWidget::itemSelectionChanged, this, &OrderDialog::refresh);
    return box;
}

Id OrderDialog::accepted_order_id() const noexcept { return accepted_id_; }

std::optional<std::size_t> OrderDialog::selected_index() const {
    return valid_index(items_table_->currentRow(), items_.size());
}

OrderDraft OrderDialog::make_draft() const {
    OrderDraft draft;
    draft.client_id = static_cast<Id>(client_box_->currentData().toUInt());
    draft.items = items_;
    draft.service_type = enum_value<ServiceType>(service_box_);
    draft.urgent = urgent_box_->isChecked();
    return draft;
}

std::optional<OrderQuote> OrderDialog::make_quote() {
    error_label_->clear();
    if (items_.empty() || client_box_->count() == 0) {
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
    const bool has_selection = selected_index().has_value();
    edit_button_->setEnabled(has_selection);
    remove_button_->setEnabled(has_selection);
    accept_button_->setEnabled(quote.has_value());
}

void OrderDialog::show_items(const std::optional<OrderQuote>& quote) {
    const std::optional<std::size_t> selected = selected_index();
    items_table_->blockSignals(true);
    items_table_->setRowCount(static_cast<int>(items_.size()));
    for (std::size_t index = 0; index < items_.size(); ++index) {
        const Item& item = items_[index];
        const int row = static_cast<int>(index);
        const QString cost = quote ? qt_convert::text(format_money(quote->item_costs[index])) : QString::fromUtf8(kNoCost);
        set_cell(items_table_, row, ItemKind, qt_convert::text(item.kind()));
        set_cell(items_table_, row, ItemMaterial, qt_convert::text(item.material_name()));
        set_cell(items_table_, row, ItemColor, qt_convert::text(item.color_name()));
        set_cell(items_table_, row, ItemWeight, QString::number(item.weight_kg(), 'f', kWeightDecimals), true);
        set_cell(items_table_, row, ItemSoil, qt_convert::text(display_name(item.soil_level())));
        set_cell(items_table_, row, ItemStains, QString::number(item.stain_count()), true);
        set_cell(items_table_, row, ItemCost, cost, true);
    }
    if (selected) {
        items_table_->selectRow(static_cast<int>(*selected));
    }
    items_table_->blockSignals(false);
}

void OrderDialog::show_summary(const std::optional<OrderQuote>& quote) {
    if (!quote) {
        summary_label_->setText(QStringLiteral("Добавьте изделия, чтобы рассчитать стоимость и срок."));
        return;
    }
    summary_label_->setText(QStringLiteral("Скидка клиента: %1\nИтого: %2\nСрок выполнения: %3\nГотовность не позже: %4")
                                .arg(qt_convert::text(format_percent(quote->discount)),
                                     qt_convert::text(format_money(quote->total_cost)),
                                     qt_convert::text(format_hours(quote->nominal_hours)),
                                     qt_convert::format_time(quote->deadline)));
}

void OrderDialog::add_item() {
    ItemDialog dialog(Item(), QStringLiteral("Новое изделие"), this);
    if (dialog.exec() == QDialog::Accepted) {
        items_.push_back(dialog.item());
        refresh();
    }
}

void OrderDialog::edit_item() {
    const std::optional<std::size_t> index = selected_index();
    if (!index) {
        return;
    }
    ItemDialog dialog(items_[*index], QStringLiteral("Изменение изделия"), this);
    if (dialog.exec() == QDialog::Accepted) {
        items_[*index] = dialog.item();
        refresh();
    }
}

void OrderDialog::remove_item() {
    const std::optional<std::size_t> index = selected_index();
    if (!index) {
        return;
    }
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(*index));
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
