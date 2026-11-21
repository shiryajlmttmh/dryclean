#pragma once

#include <QDialog>
#include <optional>
#include <vector>

#include "shop.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QPushButton;
class TableWidget;

class OrderDialog : public QDialog {
    Q_OBJECT

public:
    OrderDialog(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

    Id accepted_order_id() const noexcept;

private slots:
    void refresh();
    void add_item();
    void try_accept();

private:
    struct DraftItem {
        Id id;
        Item item;
    };

    void edit_item(Id id);
    void remove_item(Id id);
    void build_header(QFormLayout* form);
    QWidget* build_items_box();
    OrderDraft make_draft() const;
    std::optional<OrderQuote> make_quote();
    void show_items(const std::optional<OrderQuote>& quote);
    void show_summary(const std::optional<OrderQuote>& quote);

    DryCleaningShop& shop_;
    Id employee_id_;
    QComboBox* client_box_;
    QCheckBox* urgent_box_;
    TableWidget* items_table_;
    QPushButton* accept_button_;
    QLabel* summary_label_;
    QLabel* error_label_;
    std::vector<DraftItem> items_;
    Id next_item_id_ = 1;
    Id accepted_id_ = 0;
};
