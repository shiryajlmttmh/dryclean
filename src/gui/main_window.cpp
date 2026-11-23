#include "gui/main_window.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QTabWidget>
#include <QVBoxLayout>
#include <array>
#include <algorithm>

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
#include "gui/suggestions.h"
#include "normalize.h"

namespace {

constexpr int kDefaultWidth = 1220;
constexpr int kDefaultHeight = 650;
constexpr int kAllStatuses = -3;
constexpr int kOverdueOrders = -2;
constexpr double kNoDiscount = 0.0;
constexpr int kStatusRightMargin = 16;
constexpr int kClockRefreshMs = 30000;
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
    "№", "Клиент", "Услуги", "Срочный", "Статус", "Изделий", "Цена", "Срок", "Прогноз"};

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
    machines_tab_ = new MachinesTab(shop_, employee_id_, this);
    add_record_tab(tabs, machines_tab_, QStringLiteral("Оборудование"));
    add_record_tab(tabs, new EmployeesTab(shop_, employee_id_, this), QStringLiteral("Сотрудники"));
    connect(planning_tab_, &PlanningTab::changed, this, &MainWindow::refresh);
    connect(batches_tab_, &BatchesTab::changed, this, &MainWindow::refresh);
    setCentralWidget(tabs);

    status_label_ = new QLabel(this);
    status_label_->setContentsMargins(0, 0, kStatusRightMargin, 0);
    statusBar()->addPermanentWidget(status_label_);

    build_menu();
    refresh();

    auto* clock_timer = new QTimer(this);
    connect(clock_timer, &QTimer::timeout, this, &MainWindow::refresh_clock_views);
    clock_timer->start(kClockRefreshMs);
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

    add_client_button_ = new QPushButton(QStringLiteral("Добавить"), page);
    layout->addWidget(add_client_button_, 0, Qt::AlignLeft);
    client_search_ = new QLineEdit(page);
    client_search_->setPlaceholderText(QStringLiteral("Поиск по имени или телефону"));
    client_search_->setAccessibleName(QStringLiteral("Поиск клиентов"));
    client_search_->setClearButtonEnabled(true);
    layout->addWidget(client_search_);

    clients_table_ = new TableWidget(make_headers(kClientHeaders), page, true);
    layout->addWidget(clients_table_);

    connect(client_search_, &QLineEdit::textChanged, this, &MainWindow::refresh_clients);
    connect(add_client_button_, &QPushButton::clicked, this, &MainWindow::add_client);
    return page;
}

QWidget* MainWindow::build_orders_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    accept_order_button_ = new QPushButton(QStringLiteral("Принять заказ"), page);
    layout->addWidget(accept_order_button_, 0, Qt::AlignLeft);

    auto* filters = new QHBoxLayout;
    order_filters_ = new QButtonGroup(page);
    const auto add_filter = [&](const QString& label, int id) {
        auto* button = new QPushButton(label, page);
        button->setCheckable(true);
        order_filters_->addButton(button, id);
        filters->addWidget(button);
        return button;
    };
    add_filter(QStringLiteral("Принятые"), static_cast<int>(OrderStatus::Accepted))->setChecked(true);
    add_filter(QStringLiteral("В обработке"), static_cast<int>(OrderStatus::InProgress));
    add_filter(QStringLiteral("Готовые"), static_cast<int>(OrderStatus::Ready));
    add_filter(QStringLiteral("Выданные"), static_cast<int>(OrderStatus::Issued));

    auto* separator = new QFrame(page);
    separator->setFrameShape(QFrame::VLine);
    filters->addSpacing(6);
    filters->addWidget(separator);
    filters->addSpacing(6);
    add_filter(QStringLiteral("Просроченные"), kOverdueOrders);
    add_filter(QStringLiteral("Все статусы"), kAllStatuses);
    filters->addStretch();
    layout->addLayout(filters);

    orders_table_ = new TableWidget(make_headers(kOrderHeaders), page, true);
    layout->addWidget(orders_table_);

    connect(order_filters_, &QButtonGroup::idToggled, this, [this](int, bool checked) {
        if (checked) refresh_orders();
    });
    connect(accept_order_button_, &QPushButton::clicked, this, &MainWindow::accept_order);
    return page;
}

void MainWindow::save() {
    run_guarded(this, [this] { shop_.save_all(); });
    update_status();
}

void MainWindow::refresh() {
    set_suggestions(client_search_, record_suggestions(shop_, SuggestionKind::ClientName));
    refresh_clients();
    refresh_orders();
    planning_tab_->refresh();
    batches_tab_->refresh();
    for (TableTab* tab : record_tabs_) {
        tab->refresh();
    }
    update_status();
}

void MainWindow::refresh_clock_views() {
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) {
        return;
    }
    planning_tab_->refresh();
    batches_tab_->refresh();
    machines_tab_->refresh();
    refresh_orders();
}

void MainWindow::refresh_clients() {
    run_guarded(this, [this] {
        const auto id = selected_id(clients_table_, ClientId);
        const bool allowed = has_permission(shop_, employee_id_, Permission::ManageClients);
        add_client_button_->setVisible(allowed);
        clients_table_->setColumnHidden(clients_table_->actions_column(), !allowed);
        const QSignalBlocker blocker(clients_table_);
        std::vector<Client> found = shop_.search_clients(client_search_->text().toStdString());
        std::stable_sort(found.begin(), found.end(), [](const Client& left, const Client& right) {
            return QString::localeAwareCompare(qt_convert::text(left.name()), qt_convert::text(right.name())) < 0;
        });
        clients_table_->reset_rows(static_cast<int>(found.size()));
        int row = 0;
        for (const Client& client : found) {
            set_cell(clients_table_, row, ClientId, client.id());
            set_cell(clients_table_, row, ClientName, qt_convert::text(client.name()));
            set_sortable_cell(clients_table_, row, ClientPhone, qt_convert::text(format_phone(client.phone())),
                              qt_convert::text(client.phone()));
            set_sortable_cell(clients_table_, row, ClientDiscount, qt_convert::text(format_percent(client.discount())),
                              client.discount(), true);
            set_cell(clients_table_, row, ClientOrders, client.orders_count());
            if (allowed) {
                const Id client_id = client.id();
                auto* actions = make_row_actions(clients_table_, row, client_id);
                add_edit_action(actions, QStringLiteral("Изменить клиента №%1").arg(client_id),
                                [this, client_id] { edit_client(client_id); });
                add_remove_action(actions, QStringLiteral("Удалить клиента №%1").arg(client_id),
                                  [this, client_id] { remove_client(client_id); },
                                  shop_.can_remove_client(client_id) ? QString()
                                      : QStringLiteral("У клиента есть заказы."));
            }
            ++row;
        }
        clients_table_->set_empty_text(!client_search_->text().trimmed().isEmpty()
            ? QStringLiteral("Клиенты не найдены. Измените поисковый запрос.")
            : allowed ? QStringLiteral("Клиентов пока нет. Нажмите «Добавить».")
                      : QStringLiteral("Клиентов пока нет."));
        clients_table_->finish_rows();
        restore_selected_id(clients_table_, ClientId, id);
    });
}

std::vector<Order> MainWindow::filtered_orders() const {
    const int filter = order_filters_->checkedId();
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
        const auto id = selected_id(orders_table_, OrderId);
        const QSignalBlocker blocker(orders_table_);
        std::vector<Order> shown = filtered_orders();
        std::stable_sort(shown.begin(), shown.end(), [](const Order& left, const Order& right) {
            return left.created_at() < right.created_at();
        });
        const bool allowed = has_permission(shop_, employee_id_, Permission::AcceptOrders);
        accept_order_button_->setVisible(allowed);
        orders_table_->set_empty_text(order_filters_->checkedId() == static_cast<int>(OrderStatus::Accepted) && allowed
            ? QStringLiteral("Принятых заказов пока нет. Нажмите «Принять заказ».")
            : QStringLiteral("Для выбранного фильтра заказов нет."));
        orders_table_->reset_rows(static_cast<int>(shown.size()));
        const TimePoint now = qt_convert::now();
        int row = 0;
        for (const Order& order : shown) {
            const Client* client = shop_.clients().find_by_id(order.client_id());
            const QString client_name = client == nullptr ? QStringLiteral("?") : qt_convert::text(client->name());
            set_cell(orders_table_, row, OrderId, order.id());
            set_cell(orders_table_, row, OrderClient, client_name);
            QStringList services;
            for (Id item_id : order.item_ids()) {
                for (ServiceType type : shop_.items().get(item_id).services()) {
                    const QString name = qt_convert::text(display_name(type));
                    if (!services.contains(name)) services << name;
                }
            }
            set_cell(orders_table_, row, OrderService, services.join(QStringLiteral(", ")));
            set_cell(orders_table_, row, OrderUrgent, order.is_urgent() ? QStringLiteral("Да") : QStringLiteral("Нет"));
            set_cell(orders_table_, row, OrderStatusName, qt_convert::text(display_name(order.status())));
            set_cell(orders_table_, row, OrderItems, order.item_ids().size());
            set_sortable_cell(orders_table_, row, OrderCost, qt_convert::text(format_money(order.total_cost())),
                              order.total_cost(), true);
            set_time_cell(orders_table_, row, OrderDeadline, order.deadline());
            set_time_cell(orders_table_, row, OrderReady, order.planned_ready_at());
            fill_order_actions(row, order, now);
            ++row;
        }
        orders_table_->finish_rows();
        restore_selected_id(orders_table_, OrderId, id);
    });
}

void MainWindow::fill_order_actions(int row, const Order& order, TimePoint now) {
    const Id id = order.id();
    auto* actions = make_row_actions(orders_table_, row, id);
    if (order.status() == OrderStatus::InProgress &&
        has_permission(shop_, employee_id_, Permission::ChangeStatus)) {
        const auto completed_at = shop_.order_processing_completed_at(id);
        QString unavailable;
        if (!completed_at) {
            unavailable = QStringLiteral("Завершите все этапы изделий заказа на вкладке «Партии».");
        } else if (now < *completed_at) {
            unavailable = QStringLiteral("Отметить готовым можно после %1.").arg(qt_convert::format_time(*completed_at));
        }
        add_row_action(actions, QStringLiteral("Отметить готовым"), [this, id] { mark_order_ready(id); }, unavailable);
    } else if (order.status() == OrderStatus::Ready && has_permission(shop_, employee_id_, Permission::IssueOrders)) {
        const auto ready_at = order.actual_ready_at();
        const QString unavailable = ready_at && now < *ready_at
            ? QStringLiteral("Выдача доступна после %1.").arg(qt_convert::format_time(*ready_at)) : QString();
        add_row_action(actions, QStringLiteral("Выдать клиенту"), [this, id] { issue_order(id); }, unavailable);
    } else if (order.status() == OrderStatus::Accepted) {
        orders_table_->item(row, OrderStatusName)->setToolTip(
            QStringLiteral("Заказ перейдёт в обработку после подтверждения плана для его изделий."));
        if (has_permission(shop_, employee_id_, Permission::AcceptOrders) && shop_.is_order_editable(id)) {
            add_remove_action(actions, QStringLiteral("Удалить заказ"), [this, id] { remove_order(id); });
        }
    }
    auto* menu = add_more_actions(actions, QStringLiteral("Действия с заказом №%1").arg(id));
    menu->addAction(QStringLiteral("Изделия заказа"), this, [this, id] { show_order_items(id); });
    menu->addAction(QStringLiteral("История заказа"), this, [this, id] { show_order_history(id); });
}

void MainWindow::show_order_items(Id id) {
    OrderItemsDialog dialog(shop_, employee_id_, id, this);
    const Revision before = shop_.revision();
    dialog.exec();
    if (shop_.revision() != before) refresh();
}

void MainWindow::show_order_history(Id id) {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("История заказа №%1").arg(id));
    dialog.resize(580, 320);
    auto* layout = new QVBoxLayout(&dialog);
    auto* table = new TableWidget({QStringLiteral("Дата и время"), QStringLiteral("Статус"),
                                   QStringLiteral("Сотрудник")}, &dialog);
    const auto& history = shop_.orders().get(id).history();
    table->reset_rows(static_cast<int>(history.size()));
    int row = 0;
    for (const StatusRecord& record : history) {
        set_time_cell(table, row, 0, record.timestamp());
        set_cell(table, row, 1, qt_convert::text(display_name(record.status())));
        set_cell(table, row, 2, qt_convert::text(shop_.employees().get(record.employee_id()).name()));
        ++row;
    }
    layout->addWidget(table);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрыть"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void MainWindow::mark_order_ready(Id id) {
    if (run_guarded(this, [&] { shop_.change_order_status(id, OrderStatus::Ready, employee_id_, qt_convert::now()); })) {
        refresh();
        statusBar()->showMessage(QStringLiteral("Заказ №%1 готов.").arg(id), 5000);
    }
}

void MainWindow::issue_order(Id id) {
    if (run_guarded(this, [&] { shop_.issue_order(id, employee_id_, qt_convert::now()); })) {
        refresh();
        statusBar()->showMessage(QStringLiteral("Заказ №%1 выдан клиенту.").arg(id), 5000);
    }
}

void MainWindow::remove_order(Id id) {
    const QString question = QStringLiteral("Удалить заказ №%1 вместе с его изделиями?").arg(id);
    if (!confirm(this, QStringLiteral("Удаление заказа"), question)) {
        return;
    }
    if (run_guarded(this, [this, id] { shop_.remove_order(id, employee_id_); })) {
        refresh();
        statusBar()->showMessage(QStringLiteral("Заказ №%1 удалён.").arg(id), 5000);
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
        shop_,         QStringLiteral("Новый клиент"), QString(), QString(), kNoDiscount,
        [this](const QString& name, const QString& phone, double discount) {
            shop_.register_client(name.toStdString(), phone.toStdString(), discount, employee_id_);
        },
        this);
    if (dialog.exec() == QDialog::Accepted) {
        refresh();
    }
}

void MainWindow::edit_client(Id id) {
    if (!has_permission(shop_, employee_id_, Permission::ManageClients)) {
        return;
    }
    const Client current = shop_.clients().get(id);
    ClientDialog dialog(
        shop_,         QStringLiteral("Изменение клиента"), qt_convert::text(current.name()), qt_convert::text(format_phone(current.phone())),
        current.discount(),
        [this, id](const QString& name, const QString& phone, double discount) {
            Client updated = shop_.clients().get(id);
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

void MainWindow::remove_client(Id id) {
    const QString name = qt_convert::text(shop_.clients().get(id).name());
    if (!confirm(this, QStringLiteral("Удаление клиента"), QStringLiteral("Удалить клиента «%1»?").arg(name))) {
        return;
    }
    if (run_guarded(this, [this, id] { shop_.remove_client(id, employee_id_); })) {
        refresh();
    }
}

void MainWindow::update_status() {
    const Employee* employee = shop_.employees().find_by_id(employee_id_);
    const QString who = employee == nullptr ? QStringLiteral("?")
                                            : qt_convert::text(employee->name()) + QStringLiteral(" (") +
                                                  qt_convert::text(display_name(employee->role())) +
                                                  QStringLiteral(")");
    const QString revision = QStringLiteral("Ревизия: %1").arg(shop_.revision());
    const QString saved = shop_.is_dirty() ? QStringLiteral("есть несохранённые изменения")
                                           : QStringLiteral("всё сохранено");
    status_label_->setText(QStringList{who, revision, saved}.join(QString::fromUtf8(kStatusSeparator)));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!shop_.is_dirty()) {
        event->accept();
        return;
    }
    QMessageBox dialog(QMessageBox::Question, QStringLiteral("Несохранённые изменения"),
                       QStringLiteral("Сохранить изменения перед выходом?"),
                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    dialog.button(QMessageBox::Save)->setText(QStringLiteral("Сохранить"));
    dialog.button(QMessageBox::Discard)->setText(QStringLiteral("Не сохранять"));
    dialog.button(QMessageBox::Cancel)->setText(QStringLiteral("Отмена"));
    dialog.setDefaultButton(QMessageBox::Save);
    const auto answer = static_cast<QMessageBox::StandardButton>(dialog.exec());
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
