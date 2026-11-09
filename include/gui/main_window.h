#pragma once

#include <QMainWindow>
#include <functional>
#include <optional>

#include "shop.h"

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
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
    void add_client();
    void edit_client();
    void remove_client();
    void update_client_buttons();

private:
    void build_menu();
    QWidget* build_clients_tab();
    QWidget* build_orders_tab();
    void update_status();
    bool run(const std::function<void()>& action);
    bool can(Permission permission) const;
    std::optional<Id> selected_client() const;

    DryCleaningShop& shop_;
    Id employee_id_;
    QLineEdit* client_search_ = nullptr;
    QTableWidget* clients_table_ = nullptr;
    QPushButton* add_client_button_ = nullptr;
    QPushButton* edit_client_button_ = nullptr;
    QPushButton* remove_client_button_ = nullptr;
    QComboBox* status_filter_ = nullptr;
    QTableWidget* orders_table_ = nullptr;
    QLabel* status_label_ = nullptr;
};
