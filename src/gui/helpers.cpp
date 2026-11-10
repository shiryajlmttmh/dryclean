#include "gui/helpers.h"

#include <QGroupBox>
#include <QMessageBox>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include "exceptions.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kNoRow = -1;
constexpr const char* kUnknown = "?";
constexpr const char* kTagSeparator = ", ";

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

QString item_tags(const DryCleaningShop& shop, const std::vector<Id>& item_ids) {
    QStringList tags;
    for (const Id id : item_ids) {
        const Item* item = shop.items().find_by_id(id);
        tags << (item == nullptr ? QString::fromUtf8(kUnknown) : qt_convert::text(item->tag()));
    }
    return tags.join(QString::fromUtf8(kTagSeparator));
}

QString machine_name(const DryCleaningShop& shop, Id machine_id) {
    const Machine* machine = shop.machines().find_by_id(machine_id);
    return machine == nullptr ? QString::fromUtf8(kUnknown) : qt_convert::text(machine->name());
}

QGroupBox* make_titled_box(const QString& title, QWidget* content, QWidget* parent) {
    auto* box = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(content);
    return box;
}
