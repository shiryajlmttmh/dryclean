#pragma once

#include <QMainWindow>
#include <functional>

#include "shop.h"

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
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

private:
    void build_menu();
    QWidget* build_clients_tab();
    QWidget* build_orders_tab();
    void update_status();
    bool run(const std::function<void()>& action);

    DryCleaningShop& shop_;
    Id employee_id_;
    QLineEdit* client_search_ = nullptr;
    QTableWidget* clients_table_ = nullptr;
    QComboBox* status_filter_ = nullptr;
    QTableWidget* orders_table_ = nullptr;
    QLabel* status_label_ = nullptr;
};
