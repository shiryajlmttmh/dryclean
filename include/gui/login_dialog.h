#pragma once

#include <QDialog>

#include "shop.h"

class QLabel;
class QLineEdit;

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(DryCleaningShop& shop, QWidget* parent = nullptr);

    Id employee_id() const noexcept;

private slots:
    void try_login();

private:
    DryCleaningShop& shop_;
    QLineEdit* login_edit_;
    QLineEdit* password_edit_;
    QLineEdit* confirmation_edit_ = nullptr;
    QLabel* error_label_;
    Id employee_id_ = 0;
};
