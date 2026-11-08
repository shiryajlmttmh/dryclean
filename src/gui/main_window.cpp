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
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "display.h"
#include "exceptions.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kDefaultWidth = 1100;
constexpr int kDefaultHeight = 650;
constexpr int kAllStatuses = -1;

void configure_table(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(static_cast<int>(headers.size()));
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
}

void set_cell(QTableWidget* table, int row, int column, const QString& text, bool right_aligned = false) {
    auto* item = new QTableWidgetItem(text);
    if (right_aligned) {
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    table->setItem(row, column, item);
}

}

MainWindow::MainWindow(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QMainWindow(parent), shop_(shop), employee_id_(employee_id) {
    setWindowTitle(QStringLiteral("Химчистка"));
    resize(kDefaultWidth, kDefaultHeight);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(build_clients_tab(), QStringLiteral("Клиенты"));
    tabs->addTab(build_orders_tab(), QStringLiteral("Заказы"));
    setCentralWidget(tabs);

    status_label_ = new QLabel(this);
    statusBar()->addPermanentWidget(status_label_);

    build_menu();
    refresh();
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

    client_search_ = new QLineEdit(page);
    client_search_->setPlaceholderText(QStringLiteral("Поиск по имени или телефону"));
    client_search_->setClearButtonEnabled(true);
    layout->addWidget(client_search_);

    clients_table_ = new QTableWidget(page);
    configure_table(clients_table_, {QStringLiteral("№"), QStringLiteral("Имя"), QStringLiteral("Телефон"),
                                     QStringLiteral("Скидка"), QStringLiteral("Заказов")});
    layout->addWidget(clients_table_);

    connect(client_search_, &QLineEdit::textChanged, this, &MainWindow::refresh_clients);
    return page;
}

QWidget* MainWindow::build_orders_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    status_filter_ = new QComboBox(page);
    status_filter_->addItem(QStringLiteral("Все статусы"), kAllStatuses);
    for (const OrderStatus status : {OrderStatus::Accepted, OrderStatus::InProgress, OrderStatus::Ready,
                                     OrderStatus::Issued}) {
        status_filter_->addItem(qt_convert::text(display_name(status)), static_cast<int>(status));
    }
    layout->addWidget(status_filter_);

    orders_table_ = new QTableWidget(page);
    configure_table(orders_table_, {QStringLiteral("№"), QStringLiteral("Клиент"), QStringLiteral("Услуга"),
                                    QStringLiteral("Срочный"), QStringLiteral("Статус"),
                                    QStringLiteral("Изделий"), QStringLiteral("Стоимость"),
                                    QStringLiteral("Срок"), QStringLiteral("Прогноз готовности")});
    layout->addWidget(orders_table_);

    connect(status_filter_, &QComboBox::currentIndexChanged, this, &MainWindow::refresh_orders);
    return page;
}

bool MainWindow::run(const std::function<void()>& action) {
    try {
        action();
        return true;
    } catch (const ShopException& error) {
        QMessageBox::warning(this, qt_convert::text(error.title()), qt_convert::text(error.what()));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("Непредвиденная ошибка"), qt_convert::text(error.what()));
    }
    return false;
}

void MainWindow::save() {
    run([this] { shop_.save_all(); });
    update_status();
}

void MainWindow::refresh() {
    refresh_clients();
    refresh_orders();
    update_status();
}

void MainWindow::refresh_clients() {
    run([this] {
        const std::vector<Client> found = shop_.search_clients(client_search_->text().toStdString());
        clients_table_->setRowCount(static_cast<int>(found.size()));
        int row = 0;
        for (const Client& client : found) {
            set_cell(clients_table_, row, 0, QString::number(client.id()), true);
            set_cell(clients_table_, row, 1, qt_convert::text(client.name()));
            set_cell(clients_table_, row, 2, qt_convert::text(client.phone()));
            set_cell(clients_table_, row, 3, qt_convert::text(format_percent(client.discount())), true);
            set_cell(clients_table_, row, 4, QString::number(client.orders_count()), true);
            ++row;
        }
    });
}

void MainWindow::refresh_orders() {
    run([this] {
        const int filter = status_filter_->currentData().toInt();
        std::vector<const Order*> shown;
        for (const Order& order : shop_.orders()) {
            if (filter == kAllStatuses || static_cast<int>(order.status()) == filter) {
                shown.push_back(&order);
            }
        }
        orders_table_->setRowCount(static_cast<int>(shown.size()));
        int row = 0;
        for (const Order* order : shown) {
            const Client* client = shop_.clients().find_by_id(order->client_id());
            const QString client_name = client == nullptr ? QStringLiteral("?") : qt_convert::text(client->name());
            set_cell(orders_table_, row, 0, QString::number(order->id()), true);
            set_cell(orders_table_, row, 1, client_name);
            set_cell(orders_table_, row, 2, qt_convert::text(display_name(order->service_type())));
            set_cell(orders_table_, row, 3, order->is_urgent() ? QStringLiteral("Да") : QStringLiteral("Нет"));
            set_cell(orders_table_, row, 4, qt_convert::text(display_name(order->status())));
            set_cell(orders_table_, row, 5, QString::number(order->item_ids().size()), true);
            set_cell(orders_table_, row, 6, qt_convert::text(format_money(order->total_cost())), true);
            set_cell(orders_table_, row, 7, qt_convert::format_time(order->deadline()));
            set_cell(orders_table_, row, 8, qt_convert::format_time(order->planned_ready_at()));
            ++row;
        }
    });
}

void MainWindow::update_status() {
    const Employee* employee = shop_.employees().find_by_id(employee_id_);
    const QString who = employee == nullptr ? QStringLiteral("?")
                                            : qt_convert::text(employee->name()) + QStringLiteral(" (") +
                                                  qt_convert::text(display_name(employee->role())) +
                                                  QStringLiteral(")");
    const QString saved = shop_.is_dirty() ? QStringLiteral("есть несохранённые изменения")
                                           : QStringLiteral("всё сохранено");
    status_label_->setText(QStringLiteral("%1   |   изменений: %2   |   %3")
                               .arg(who)
                               .arg(shop_.revision())
                               .arg(saved));
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
    if (answer == QMessageBox::Save && !run([this] { shop_.save_all(); })) {
        event->ignore();
        return;
    }
    event->accept();
}
