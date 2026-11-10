#pragma once

#include <QDialog>

class QFormLayout;
class QLabel;

class ApplyDialog : public QDialog {
    Q_OBJECT

public:
    ApplyDialog(const QString& title, QWidget* parent);

protected:
    QFormLayout* form() const noexcept;
    virtual void apply() = 0;

private slots:
    void try_accept();

private:
    QFormLayout* form_;
    QLabel* error_label_;
};
