#pragma once

#include <QDialog>

#include "shop.h"

class QLabel;
class QLineEdit;

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(const DryCleaningShop& shop, QWidget* parent = nullptr);

    Id employee_id() const noexcept;

private slots:
    void try_login();

private:
    const DryCleaningShop& shop_;
    QLineEdit* login_edit_;
    QLabel* error_label_;
    Id employee_id_ = 0;
};
