#pragma once

#include <QMainWindow>
#include <vector>

#include "shop.h"

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class BatchesTab;
class PlanningTab;
class QPushButton;
class QTabWidget;
class MachinesTab;
class TableTab;
class QTableWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(DryCleaningShop& shop, Id employee_id, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void save();
    void refresh();
    void refresh_clock_views();
    void refresh_clients();
    void refresh_orders();
    void accept_order();
    void mark_order_ready();
    void issue_order();
    void remove_order();
    void show_order_items();
    void update_order_buttons();
    void add_client();
    void edit_client();
    void remove_client();
    void update_client_buttons();

private:
    void build_menu();
    void add_record_tab(QTabWidget* tabs, TableTab* tab, const QString& title);
    QWidget* build_clients_tab();
    QWidget* build_orders_tab();
    void update_status();
    std::vector<Order> filtered_orders() const;

    DryCleaningShop& shop_;
    Id employee_id_;
    QLineEdit* client_search_ = nullptr;
    QTableWidget* clients_table_ = nullptr;
    QPushButton* add_client_button_ = nullptr;
    QPushButton* edit_client_button_ = nullptr;
    QPushButton* remove_client_button_ = nullptr;
    QComboBox* status_filter_ = nullptr;
    QPushButton* accept_order_button_ = nullptr;
    QPushButton* ready_order_button_ = nullptr;
    QPushButton* issue_order_button_ = nullptr;
    QPushButton* remove_order_button_ = nullptr;
    QPushButton* order_items_button_ = nullptr;
    PlanningTab* planning_tab_ = nullptr;
    BatchesTab* batches_tab_ = nullptr;
    MachinesTab* machines_tab_ = nullptr;
    std::vector<TableTab*> record_tabs_;
    QTableWidget* orders_table_ = nullptr;
    QLabel* status_label_ = nullptr;
};
