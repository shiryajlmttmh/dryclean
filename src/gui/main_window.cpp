#include "gui/main_window.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <array>

#include "display.h"
#include "exceptions.h"
#include "gui/client_dialog.h"
#include "gui/batches_tab.h"
#include "gui/employees_tab.h"
#include "gui/helpers.h"
#include "gui/machines_tab.h"
#include "gui/materials_tab.h"
#include "gui/order_dialog.h"
#include "gui/order_items_dialog.h"
#include "gui/planning_tab.h"
#include "gui/qt_convert.h"
#include "gui/table_tab.h"
#include "gui/table_utils.h"

namespace {

constexpr int kDefaultWidth = 1100;
constexpr int kDefaultHeight = 650;
constexpr int kAllStatuses = -1;
constexpr int kOverdueOrders = -2;
constexpr int kNoRow = -1;
constexpr double kNoDiscount = 0.0;
constexpr int kStatusRightMargin = 16;
constexpr const char* kStatusSeparator = "   |   ";

enum ClientColumn { ClientId, ClientName, ClientPhone, ClientDiscount, ClientOrders, ClientColumnCount };

constexpr std::array<const char*, ClientColumnCount> kClientHeaders{"№", "Имя", "Телефон", "Скидка", "Заказов"};

enum OrderColumn {
    OrderId,
    OrderClient,
    OrderService,
    OrderUrgent,
    OrderStatusName,
    OrderItems,
    OrderCost,
    OrderDeadline,
    OrderReady,
    OrderColumnCount
};

constexpr std::array<const char*, OrderColumnCount> kOrderHeaders{
    "№", "Клиент", "Услуга", "Срочный", "Статус", "Изделий", "Стоимость", "Срок", "Прогноз готовности"};

}

MainWindow::MainWindow(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QMainWindow(parent), shop_(shop), employee_id_(employee_id) {
    setWindowTitle(QStringLiteral("Химчистка"));
    resize(kDefaultWidth, kDefaultHeight);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(build_clients_tab(), QStringLiteral("Клиенты"));
    tabs->addTab(build_orders_tab(), QStringLiteral("Заказы"));
    planning_tab_ = new PlanningTab(shop_, employee_id_, this);
    batches_tab_ = new BatchesTab(shop_, employee_id_, this);
    tabs->addTab(planning_tab_, QStringLiteral("Планирование"));
    tabs->addTab(batches_tab_, QStringLiteral("Партии"));
    add_record_tab(tabs, new MaterialsTab(shop_, employee_id_, this), QStringLiteral("Склад"));
    add_record_tab(tabs, new MachinesTab(shop_, employee_id_, this), QStringLiteral("Оборудование"));
    add_record_tab(tabs, new EmployeesTab(shop_, employee_id_, this), QStringLiteral("Сотрудники"));
    connect(planning_tab_, &PlanningTab::changed, this, &MainWindow::refresh);
    connect(batches_tab_, &BatchesTab::changed, this, &MainWindow::refresh);
    setCentralWidget(tabs);

    status_label_ = new QLabel(this);
    status_label_->setContentsMargins(0, 0, kStatusRightMargin, 0);
    statusBar()->addPermanentWidget(status_label_);

    build_menu();
    refresh();
}

void MainWindow::add_record_tab(QTabWidget* tabs, TableTab* tab, const QString& title) {
    tabs->addTab(tab, title);
    record_tabs_.push_back(tab);
    connect(tab, &TableTab::changed, this, &MainWindow::refresh);
}

void MainWindow::build_menu() {
    QMenu* file_menu = menuBar()->addMenu(QStringLiteral("Файл"));
    QAction* save_action = file_menu->addAction(QStringLiteral("Сохранить"));
    save_action->setShortcut(QKeySequence::Save);
    connect(save_action, &QAction::triggered, this, &MainWindow::save);
    file_menu->addSeparator();
    QAction* quit_action = file_menu->addAction(QStringLiteral("Выход"));
    connect(quit_action, &QAction::triggered, this, &QWidget::close);

    QMenu* view_menu = menuBar()->addMenu(QStringLiteral("Вид"));
    QAction* refresh_action = view_menu->addAction(QStringLiteral("Обновить"));
    refresh_action->setShortcut(QKeySequence::Refresh);
    connect(refresh_action, &QAction::triggered, this, &MainWindow::refresh);
}

QWidget* MainWindow::build_clients_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* toolbar = new QHBoxLayout;
    client_search_ = new QLineEdit(page);
    client_search_->setPlaceholderText(QStringLiteral("Поиск по имени или телефону"));
    client_search_->setClearButtonEnabled(true);
    add_client_button_ = new QPushButton(QStringLiteral("Добавить"), page);
    edit_client_button_ = new QPushButton(QStringLiteral("Изменить"), page);
    remove_client_button_ = new QPushButton(QStringLiteral("Удалить"), page);
    toolbar->addWidget(client_search_, kStretch);
    toolbar->addWidget(add_client_button_);
    toolbar->addWidget(edit_client_button_);
    toolbar->addWidget(remove_client_button_);
    layout->addLayout(toolbar);

    clients_table_ = new QTableWidget(page);
    configure_table(clients_table_, make_headers(kClientHeaders));
    layout->addWidget(clients_table_);

    connect(client_search_, &QLineEdit::textChanged, this, &MainWindow::refresh_clients);
    connect(add_client_button_, &QPushButton::clicked, this, &MainWindow::add_client);
    connect(edit_client_button_, &QPushButton::clicked, this, &MainWindow::edit_client);
    connect(remove_client_button_, &QPushButton::clicked, this, &MainWindow::remove_client);
    connect(clients_table_, &QTableWidget::cellDoubleClicked, this, &MainWindow::edit_client);
    connect(clients_table_, &QTableWidget::itemSelectionChanged, this, &MainWindow::update_client_buttons);
    return page;
}

QWidget* MainWindow::build_orders_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* toolbar = new QHBoxLayout;
    status_filter_ = new QComboBox(page);
    status_filter_->addItem(QStringLiteral("Все статусы"), kAllStatuses);
    status_filter_->addItem(QStringLiteral("Просроченные"), kOverdueOrders);
    for (const OrderStatus status : {OrderStatus::Accepted, OrderStatus::InProgress, OrderStatus::Ready,
                                     OrderStatus::Issued}) {
        status_filter_->addItem(qt_convert::text(display_name(status)), static_cast<int>(status));
    }
    accept_order_button_ = new QPushButton(QStringLiteral("Принять заказ"), page);
    ready_order_button_ = new QPushButton(QStringLiteral("Отметить готовым"), page);
    issue_order_button_ = new QPushButton(QStringLiteral("Выдать клиенту"), page);
    order_items_button_ = new QPushButton(QStringLiteral("Изделия заказа"), page);
    remove_order_button_ = new QPushButton(QStringLiteral("Удалить заказ"), page);
    toolbar->addWidget(status_filter_);
    toolbar->addStretch(kStretch);
    toolbar->addWidget(order_items_button_);
    toolbar->addWidget(accept_order_button_);
    toolbar->addWidget(ready_order_button_);
    toolbar->addWidget(issue_order_button_);
    toolbar->addWidget(remove_order_button_);
    layout->addLayout(toolbar);

    orders_table_ = new QTableWidget(page);
    configure_table(orders_table_, make_headers(kOrderHeaders));
    layout->addWidget(orders_table_);

    connect(status_filter_, &QComboBox::currentIndexChanged, this, &MainWindow::refresh_orders);
    connect(accept_order_button_, &QPushButton::clicked, this, &MainWindow::accept_order);
    connect(order_items_button_, &QPushButton::clicked, this, &MainWindow::show_order_items);
    connect(orders_table_, &QTableWidget::cellDoubleClicked, this, &MainWindow::show_order_items);
    connect(ready_order_button_, &QPushButton::clicked, this, &MainWindow::mark_order_ready);
    connect(issue_order_button_, &QPushButton::clicked, this, &MainWindow::issue_order);
    connect(remove_order_button_, &QPushButton::clicked, this, &MainWindow::remove_order);
    connect(orders_table_, &QTableWidget::itemSelectionChanged, this, &MainWindow::update_order_buttons);
    return page;
}

void MainWindow::save() {
    run_guarded(this, [this] { shop_.save_all(); });
    update_status();
}

void MainWindow::refresh() {
    refresh_clients();
    refresh_orders();
    planning_tab_->refresh();
    batches_tab_->refresh();
    for (TableTab* tab : record_tabs_) {
        tab->refresh();
    }
    update_status();
}

void MainWindow::update_client_buttons() {
    const bool allowed = has_permission(shop_, employee_id_, Permission::ManageClients);
    const bool selected = selected_id(clients_table_, ClientId).has_value();
    add_client_button_->setEnabled(allowed);
    edit_client_button_->setEnabled(allowed && selected);
    remove_client_button_->setEnabled(allowed && selected);
}

void MainWindow::refresh_clients() {
    run_guarded(this, [this] {
        const std::vector<Client> found = shop_.search_clients(client_search_->text().toStdString());
        clients_table_->setRowCount(static_cast<int>(found.size()));
        int row = 0;
        for (const Client& client : found) {
            set_cell(clients_table_, row, ClientId, QString::number(client.id()), true);
            set_cell(clients_table_, row, ClientName, qt_convert::text(client.name()));
            set_cell(clients_table_, row, ClientPhone, qt_convert::text(client.phone()));
            set_cell(clients_table_, row, ClientDiscount, qt_convert::text(format_percent(client.discount())));
            set_cell(clients_table_, row, ClientOrders, QString::number(client.orders_count()));
            ++row;
        }
    });
    update_client_buttons();
}

std::vector<Order> MainWindow::filtered_orders() const {
    const int filter = status_filter_->currentData().toInt();
    if (filter == kOverdueOrders) {
        return shop_.find_overdue_orders(qt_convert::now());
    }
    if (filter == kAllStatuses) {
        return std::vector<Order>(shop_.orders().begin(), shop_.orders().end());
    }
    return shop_.orders_by_status(static_cast<OrderStatus>(filter));
}

void MainWindow::refresh_orders() {
    run_guarded(this, [this] {
        const std::vector<Order> shown = filtered_orders();
        orders_table_->setRowCount(static_cast<int>(shown.size()));
        int row = 0;
        for (const Order& order : shown) {
            const Client* client = shop_.clients().find_by_id(order.client_id());
            const QString client_name = client == nullptr ? QStringLiteral("?") : qt_convert::text(client->name());
            set_cell(orders_table_, row, OrderId, QString::number(order.id()), true);
            set_cell(orders_table_, row, OrderClient, client_name);
            set_cell(orders_table_, row, OrderService, qt_convert::text(display_name(order.service_type())));
            set_cell(orders_table_, row, OrderUrgent, order.is_urgent() ? QStringLiteral("Да") : QStringLiteral("Нет"));
            set_cell(orders_table_, row, OrderStatusName, qt_convert::text(display_name(order.status())));
            set_cell(orders_table_, row, OrderItems, QString::number(order.item_ids().size()), true);
            set_cell(orders_table_, row, OrderCost, qt_convert::text(format_money(order.total_cost())), true);
            set_cell(orders_table_, row, OrderDeadline, qt_convert::format_time(order.deadline()));
            set_cell(orders_table_, row, OrderReady, qt_convert::format_time(order.planned_ready_at()));
            ++row;
        }
    });
    update_order_buttons();
}

void MainWindow::update_order_buttons() {
    const std::optional<Id> id = selected_id(orders_table_, OrderId);
    const Order* order = id ? shop_.orders().find_by_id(*id) : nullptr;
    const bool in_progress = order != nullptr && order->status() == OrderStatus::InProgress;
    const bool ready = order != nullptr && order->status() == OrderStatus::Ready;
    const bool accepted = order != nullptr && order->status() == OrderStatus::Accepted;
    order_items_button_->setEnabled(order != nullptr);
    accept_order_button_->setEnabled(has_permission(shop_, employee_id_, Permission::AcceptOrders));
    ready_order_button_->setEnabled(in_progress && has_permission(shop_, employee_id_, Permission::ChangeStatus));
    issue_order_button_->setEnabled(ready && has_permission(shop_, employee_id_, Permission::IssueOrders));
    remove_order_button_->setEnabled(accepted && has_permission(shop_, employee_id_, Permission::AcceptOrders));
}

void MainWindow::show_order_items() {
    const std::optional<Id> id = selected_id(orders_table_, OrderId);
    if (!id) {
        return;
    }
    OrderItemsDialog dialog(shop_, employee_id_, *id, this);
    dialog.exec();
    refresh();
}

void MainWindow::mark_order_ready() {
    const std::optional<Id> id = selected_id(orders_table_, OrderId);
    if (id && run_guarded(this, [&] {
            shop_.change_order_status(*id, OrderStatus::Ready, employee_id_, qt_convert::now());
        })) {
        refresh();
    }
}

void MainWindow::issue_order() {
    const std::optional<Id> id = selected_id(orders_table_, OrderId);
    if (id && run_guarded(this, [&] { shop_.issue_order(*id, employee_id_, qt_convert::now()); })) {
        refresh();
    }
}

void MainWindow::remove_order() {
    const std::optional<Id> id = selected_id(orders_table_, OrderId);
    if (!id) {
        return;
    }
    const QString question = QStringLiteral("Удалить заказ №%1 вместе с его изделиями?").arg(*id);
    if (!confirm(this, QStringLiteral("Удаление заказа"), question)) {
        return;
    }
    if (run_guarded(this, [this, id] { shop_.remove_order(*id, employee_id_); })) {
        refresh();
    }
}

void MainWindow::accept_order() {
    OrderDialog dialog(shop_, employee_id_, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    refresh();
    QMessageBox::information(this, QStringLiteral("Заказ принят"),
                             QStringLiteral("Заказ №%1 принят.").arg(dialog.accepted_order_id()));
}

void MainWindow::add_client() {
    ClientDialog dialog(
        QStringLiteral("Новый клиент"), QString(), QString(), kNoDiscount,
        [this](const QString& name, const QString& phone, double discount) {
            shop_.register_client(name.toStdString(), phone.toStdString(), discount, employee_id_);
        },
        this);
    if (dialog.exec() == QDialog::Accepted) {
        refresh();
    }
}

void MainWindow::edit_client() {
    const std::optional<Id> id = selected_id(clients_table_, ClientId);
    if (!id || !has_permission(shop_, employee_id_, Permission::ManageClients)) {
        return;
    }
    const Client current = shop_.clients().get(*id);
    ClientDialog dialog(
        QStringLiteral("Изменение клиента"), qt_convert::text(current.name()), qt_convert::text(current.phone()),
        current.discount(),
        [this, id](const QString& name, const QString& phone, double discount) {
            Client updated = shop_.clients().get(*id);
            updated.set_name(name.toStdString());
            updated.set_phone(phone.toStdString());
            updated.set_discount(discount);
            shop_.update_client(updated, employee_id_);
        },
        this);
    if (dialog.exec() == QDialog::Accepted) {
        refresh();
    }
}

void MainWindow::remove_client() {
    const std::optional<Id> id = selected_id(clients_table_, ClientId);
    if (!id) {
        return;
    }
    const QString name = qt_convert::text(shop_.clients().get(*id).name());
    if (!confirm(this, QStringLiteral("Удаление клиента"), QStringLiteral("Удалить клиента «%1»?").arg(name))) {
        return;
    }
    if (run_guarded(this, [this, id] { shop_.remove_client(*id, employee_id_); })) {
        refresh();
    }
}

void MainWindow::update_status() {
    const Employee* employee = shop_.employees().find_by_id(employee_id_);
    const QString who = employee == nullptr ? QStringLiteral("?")
                                            : qt_convert::text(employee->name()) + QStringLiteral(" (") +
                                                  qt_convert::text(display_name(employee->role())) +
                                                  QStringLiteral(")");
    const QString revision = QStringLiteral("изменений: %1").arg(shop_.revision());
    const QString saved = shop_.is_dirty() ? QStringLiteral("есть несохранённые изменения")
                                           : QStringLiteral("всё сохранено");
    status_label_->setText(QStringList{who, revision, saved}.join(QString::fromUtf8(kStatusSeparator)));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!shop_.is_dirty()) {
        event->accept();
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Несохранённые изменения"), QStringLiteral("Сохранить изменения перед выходом?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
        event->ignore();
        return;
    }
    if (answer == QMessageBox::Save && !run_guarded(this, [this] { shop_.save_all(); })) {
        event->ignore();
        return;
    }
    event->accept();
}
