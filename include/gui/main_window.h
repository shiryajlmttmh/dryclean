#pragma once

#include <QMainWindow>
#include <vector>

#include "shop.h"

class QCloseEvent;
class QButtonGroup;
class QLabel;
class QLineEdit;
class BatchesTab;
class PlanningTab;
class QPushButton;
class QTabWidget;
class MachinesTab;
class TableTab;
class TableWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void save();
    void refresh();
    void on_clock_timeout();
    void refresh_clock_views();
    void refresh_clients();
    void refresh_orders();
    void accept_order();
    void add_client();

private:
    void mark_order_ready(Id id);
    void issue_order(Id id);
    void remove_order(Id id);
    void show_order_items(Id id);
    void show_order_history(Id id);
    void fill_order_actions(int row, const Order& order, TimePoint now);
    void edit_client(Id id);
    void remove_client(Id id);
    void build_menu();
    void add_record_tab(QTabWidget* tabs, TableTab* tab, const QString& title);
    QWidget* build_clients_tab();
    QWidget* build_orders_tab();
    void update_status();
    std::vector<Order> filtered_orders() const;

    DryCleaningShop& shop_;
    Id employee_id_;
    ShopForecast forecast_;
    QLineEdit* client_search_ = nullptr;
    TableWidget* clients_table_ = nullptr;
    QPushButton* add_client_button_ = nullptr;
    QLabel* clients_read_only_hint_ = nullptr;
    QButtonGroup* order_filters_ = nullptr;
    QPushButton* accept_order_button_ = nullptr;
    PlanningTab* planning_tab_ = nullptr;
    BatchesTab* batches_tab_ = nullptr;
    MachinesTab* machines_tab_ = nullptr;
    std::vector<TableTab*> record_tabs_;
    TableWidget* orders_table_ = nullptr;
    QLabel* status_label_ = nullptr;
};
