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
class QTableWidget;

class OrderDialog : public QDialog {
    Q_OBJECT

public:
    OrderDialog(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

    Id accepted_order_id() const noexcept;

private slots:
    void refresh();
    void add_item();
    void edit_item();
    void remove_item();
    void try_accept();

private:
    void build_header(QFormLayout* form);
    QWidget* build_items_box();
    OrderDraft make_draft() const;
    std::optional<OrderQuote> make_quote();
    void show_items(const std::optional<OrderQuote>& quote);
    void show_summary(const std::optional<OrderQuote>& quote);
    int selected_row() const;

    DryCleaningShop& shop_;
    Id employee_id_;
    QComboBox* client_box_;
    QComboBox* service_box_;
    QCheckBox* urgent_box_;
    QTableWidget* items_table_;
    QPushButton* edit_button_;
    QPushButton* remove_button_;
    QPushButton* accept_button_;
    QLabel* summary_label_;
    QLabel* error_label_;
    std::vector<Item> items_;
    Id accepted_id_ = 0;
};
