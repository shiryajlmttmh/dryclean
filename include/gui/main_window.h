#pragma once

#include <QMainWindow>

#include "shop.h"

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class BatchesTab;
class PlanningTab;
class QPushButton;
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
    void refresh_clients();
    void refresh_orders();
    void accept_order();
    void mark_order_ready();
    void issue_order();
    void update_order_buttons();
    void add_client();
    void edit_client();
    void remove_client();
    void update_client_buttons();

private:
    void build_menu();
    QWidget* build_clients_tab();
    QWidget* build_orders_tab();
    void update_status();

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
    PlanningTab* planning_tab_ = nullptr;
    BatchesTab* batches_tab_ = nullptr;
    QTableWidget* orders_table_ = nullptr;
    QLabel* status_label_ = nullptr;
};
