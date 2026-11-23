#pragma once

#include <QStringList>
#include <QWidget>

#include "shop.h"

class QDialog;
class QHBoxLayout;
class QLabel;
class QPushButton;
class TableWidget;

class TableTab : public QWidget {
    Q_OBJECT

public:
    TableTab(DryCleaningShop& shop, Id employee_id, Permission permission, const QStringList& headers,
             QWidget* parent, bool has_view_actions = false);

public slots:
    void refresh();

signals:
    void changed();

protected:
    virtual void fill(TableWidget* table) = 0;
    virtual void add_record() = 0;
    virtual void edit_record(Id id) = 0;
    virtual void remove_record(Id id) = 0;
    virtual void add_extra_actions(QHBoxLayout* actions, Id id);
    virtual QString removal_block(Id id) const;

    void show_dialog(QDialog& dialog);
    bool is_allowed() const;

    DryCleaningShop& shop_;
    Id employee_id_;

private:
    void confirm_removal(Id id);

    TableWidget* table_;
    Permission permission_;
    bool has_view_actions_;
    QPushButton* add_button_;
    QLabel* read_only_hint_;
};
