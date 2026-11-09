#pragma once

#include <QDialog>
#include <functional>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;

class ClientDialog : public QDialog {
    Q_OBJECT

public:
    using Apply = std::function<void(const QString& name, const QString& phone, double discount)>;

    ClientDialog(const QString& title, const QString& name, const QString& phone, double discount, Apply apply,
                 QWidget* parent = nullptr);

private slots:
    void try_accept();

private:
    QLineEdit* name_edit_;
    QLineEdit* phone_edit_;
    QDoubleSpinBox* discount_edit_;
    QLabel* error_label_;
    Apply apply_;
};
