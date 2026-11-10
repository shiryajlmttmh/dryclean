#include "gui/table_tab.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "gui/helpers.h"
#include "gui/table_utils.h"

TableTab::TableTab(DryCleaningShop& shop, Id employee_id, Permission permission, const QStringList& headers,
                   QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id), permission_(permission) {
    toolbar_ = new QHBoxLayout;
    toolbar_->addStretch(1);
    add_button_ = new QPushButton(QStringLiteral("Добавить"), this);
    edit_button_ = new QPushButton(QStringLiteral("Изменить"), this);
    toolbar_->addWidget(add_button_);
    toolbar_->addWidget(edit_button_);

    table_ = new QTableWidget(this);
    configure_table(table_, headers);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar_);
    layout->addWidget(table_);

    connect(add_button_, &QPushButton::clicked, this, &TableTab::add_record);
    connect(edit_button_, &QPushButton::clicked, this, &TableTab::edit_selected);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &TableTab::edit_selected);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &TableTab::update_buttons);
}

void TableTab::refresh() {
    fill(table_);
    update_buttons();
}

bool TableTab::is_allowed() const { return has_permission(shop_, employee_id_, permission_); }

std::optional<Id> TableTab::selected() const { return selected_id(table_, kIdColumn); }

void TableTab::update_buttons() {
    add_button_->setEnabled(is_allowed());
    edit_button_->setEnabled(is_allowed() && selected().has_value());
}

void TableTab::edit_selected() {
    const std::optional<Id> id = selected();
    if (id && is_allowed()) {
        edit_record(*id);
    }
}

void TableTab::show_dialog(QDialog& dialog) {
    if (dialog.exec() == QDialog::Accepted) {
        emit changed();
    }
}
