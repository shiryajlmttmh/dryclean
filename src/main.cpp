#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>

#include "exceptions.h"
#include "gui/login_dialog.h"
#include "gui/main_window.h"
#include "gui/qt_convert.h"
#include "shop.h"

namespace {

constexpr const char* kApplicationName = "dryclean";
constexpr const char* kOrganizationName = "BSUIR";

}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(kApplicationName);
    QApplication::setOrganizationName(kOrganizationName);

    const QString data_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    DryCleaningShop shop(qt_convert::path(data_directory));
    try {
        shop.load_all();
        if (shop.is_dirty()) {
            shop.save_all();
        }
    } catch (const ShopException& error) {
        QMessageBox::critical(nullptr, qt_convert::text(error.title()), qt_convert::text(error.what()));
        return 1;
    }

    LoginDialog login(shop);
    if (login.exec() != QDialog::Accepted) {
        return 0;
    }

    MainWindow window(shop, login.employee_id());
    window.show();
    return app.exec();
}
