#include "gui/helpers.h"

#include <QAbstractButton>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QListWidget>
#include <QMessageBox>
#include <QStringList>
#include <QTableWidget>
#include <QVariant>
#include <QVBoxLayout>

#include "exceptions.h"
#include "display.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kNoRow = -1;
constexpr const char* kUnknown = "?";
constexpr const char* kTagSeparator = ", ";
constexpr int kPositionRole = Qt::UserRole;

std::optional<std::size_t> stored_position(const QVariant& value, std::size_t count) {
    if (!value.isValid()) {
        return std::nullopt;
    }
    const std::size_t position = static_cast<std::size_t>(value.toULongLong());
    return position < count ? std::optional<std::size_t>(position) : std::nullopt;
}

}

bool run_guarded(QWidget* parent, const std::function<void()>& action) {
    try {
        action();
        return true;
    } catch (const ShopException& error) {
        QMessageBox::warning(parent, qt_convert::text(error.title()), qt_convert::text(error.what()));
    } catch (const std::exception& error) {
        QMessageBox::critical(parent, QStringLiteral("Непредвиденная ошибка"), qt_convert::text(error.what()));
    }
    return false;
}

bool confirm(QWidget* parent, const QString& title, const QString& text) {
    QMessageBox dialog(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    dialog.button(QMessageBox::Yes)->setText(QStringLiteral("Да"));
    dialog.button(QMessageBox::No)->setText(QStringLiteral("Нет"));
    return dialog.exec() == QMessageBox::Yes;
}

bool has_permission(const DryCleaningShop& shop, Id employee_id, Permission permission) {
    const Employee* employee = shop.employees().find_by_id(employee_id);
    return employee != nullptr && employee->has_permission(permission);
}

std::optional<Id> selected_id(const QTableWidget* table, int id_column) {
    const int row = table->currentRow();
    if (row == kNoRow || table->item(row, id_column) == nullptr) {
        return std::nullopt;
    }
    return static_cast<Id>(table->item(row, id_column)->text().toUInt());
}

void restore_selected_id(QTableWidget* table, int id_column, std::optional<Id> id) {
    table->setCurrentCell(kNoRow, kNoRow);
    table->clearSelection();
    if (!id) return;
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, id_column)->text().toUInt() == *id) {
            table->setCurrentCell(row, id_column);
            table->selectRow(row);
            return;
        }
    }
}

void add_positioned_item(QListWidget* list, const QString& text, std::size_t position) {
    auto* item = new QListWidgetItem(text, list);
    item->setData(kPositionRole, static_cast<qulonglong>(position));
}

std::optional<std::size_t> selected_position(const QListWidget* list, std::size_t count) {
    const QListWidgetItem* item = list->currentItem();
    return item == nullptr ? std::nullopt : stored_position(item->data(kPositionRole), count);
}

QString item_services_text(const Item& item) {
    QStringList names;
    for (ServiceType type : item.services()) names << qt_convert::text(display_name(type));
    return names.join(QStringLiteral(" → "));
}

QString item_tag(const DryCleaningShop& shop, Id item_id) {
    const Item* item = shop.items().find_by_id(item_id);
    return item == nullptr ? QString::fromUtf8(kUnknown) : qt_convert::text(item->tag());
}

QString item_tags(const DryCleaningShop& shop, const std::vector<Id>& item_ids) {
    QStringList tags;
    for (const Id id : item_ids) {
        tags << item_tag(shop, id);
    }
    return tags.join(QString::fromUtf8(kTagSeparator));
}

QString machine_name(const DryCleaningShop& shop, Id machine_id) {
    const Machine* machine = shop.machines().find_by_id(machine_id);
    return machine == nullptr ? QString::fromUtf8(kUnknown) : qt_convert::text(machine->name());
}

QWidget* make_titled_box(const QString& title, QWidget* content, QWidget* parent) {
    auto* box = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(content);
    return box;
}

QDoubleSpinBox* make_spin_box(QWidget* parent, double minimum, double maximum, int decimals, double step,
                              const QString& suffix, double value) {
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(minimum, maximum);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setSuffix(suffix);
    box->setValue(value);
    return box;
}
