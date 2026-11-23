#include "gui/table_tab.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "gui/helpers.h"
#include "gui/table_utils.h"

TableTab::TableTab(DryCleaningShop& shop, Id employee_id, Permission permission, const QStringList& headers,
                   QWidget* parent, bool has_view_actions)
    : QWidget(parent), shop_(shop), employee_id_(employee_id), permission_(permission),
      has_view_actions_(has_view_actions) {
    add_button_ = new QPushButton(QStringLiteral("Добавить"), this);
    read_only_hint_ = new QLabel(QStringLiteral("Только просмотр. У вашей роли нет прав на изменения."), this);
    read_only_hint_->setWordWrap(true);
    table_ = new TableWidget(headers, this, true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(add_button_, 0, Qt::AlignLeft);
    layout->addWidget(read_only_hint_);
    layout->addWidget(table_);

    connect(add_button_, &QPushButton::clicked, this, [this] {
        if (check_action_permission(this, shop_, employee_id_, permission_)) add_record();
    });
}

void TableTab::refresh() {
    const bool allowed = is_allowed();
    const bool show_actions = allowed || has_view_actions_;
    add_button_->setVisible(allowed);
    read_only_hint_->setVisible(!allowed);
    table_->setColumnHidden(table_->actions_column(), !show_actions);
    const auto selected = selected_id(table_, 0);
    const QSignalBlocker blocker(table_);
    table_->set_empty_text(allowed ? QStringLiteral("Нет записей. Нажмите «Добавить» для создания записи.")
                                   : QStringLiteral("Нет записей."));
    fill(table_);
    for (int row = 0; show_actions && row < table_->rowCount(); ++row) {
        const Id id = table_->item(row, 0)->text().toUInt();
        auto* actions = make_row_actions(table_, row, id);
        if (allowed) {
            add_edit_action(actions, QStringLiteral("Изменить запись №%1").arg(id),
                            [this, id] { if (check_action_permission(this, shop_, employee_id_, permission_)) edit_record(id); });
            add_remove_action(actions, QStringLiteral("Удалить запись №%1").arg(id),
                              [this, id] { confirm_removal(id); }, removal_block(id));
        }
        add_extra_actions(actions, id);
    }
    table_->finish_rows();
    restore_selected_id(table_, 0, selected);
}

bool TableTab::is_allowed() const { return has_permission(shop_, employee_id_, permission_); }

void TableTab::add_extra_actions(QHBoxLayout*, Id) {}

QString TableTab::removal_block(Id) const { return {}; }

void TableTab::confirm_removal(Id id) {
    if (!check_action_permission(this, shop_, employee_id_, permission_)) return;
    const auto blocked = removal_block(id);
    if (!blocked.isEmpty()) {
        show_action_refusal(this, blocked);
        refresh();
        return;
    }
    if (!confirm(this, QStringLiteral("Удаление"), QStringLiteral("Удалить запись №%1?").arg(id))) {
        return;
    }
    if (run_guarded(this, [this, id] { remove_record(id); })) {
        emit changed();
    }
}

void TableTab::show_dialog(QDialog& dialog) {
    if (dialog.exec() == QDialog::Accepted) {
        emit changed();
    }
}
