#include <QApplication>
#include <cstdlib>
#include <QStandardPaths>

#include "gui/login_dialog.h"
#include "gui/main_window.h"
#include "gui/qt_convert.h"
#include "gui/startup.h"
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
    if (!load_shop(shop)) return EXIT_FAILURE;

    LoginDialog login(shop);
    if (login.exec() != QDialog::Accepted) {
        return EXIT_SUCCESS;
    }

    MainWindow window(shop, login.employee_id());
    window.show();
    return app.exec();
}
