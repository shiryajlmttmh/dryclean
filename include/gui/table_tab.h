#pragma once

#include <QStringList>
#include <QWidget>
#include <optional>

#include "shop.h"

class QDialog;
class QHBoxLayout;
class QPushButton;
class QTableWidget;

class TableTab : public QWidget {
    Q_OBJECT

public:
    static constexpr int kIdColumn = 0;

    TableTab(DryCleaningShop& shop, Id employee_id, Permission permission, const QStringList& headers,
             QWidget* parent);

public slots:
    void refresh();

signals:
    void changed();

protected:
    virtual void fill(QTableWidget* table) = 0;
    virtual void add_record() = 0;
    virtual void edit_record(Id id) = 0;
    virtual void update_buttons();

    void show_dialog(QDialog& dialog);
    std::optional<Id> selected() const;
    bool is_allowed() const;

    DryCleaningShop& shop_;
    Id employee_id_;
    QHBoxLayout* toolbar_;
    QTableWidget* table_;

private slots:
    void edit_selected();

private:
    Permission permission_;
    QPushButton* add_button_;
    QPushButton* edit_button_;
};
