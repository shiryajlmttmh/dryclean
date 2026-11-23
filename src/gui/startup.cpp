#include "gui/startup.h"

#include <QMessageBox>
#include <QPushButton>

#include "exceptions.h"
#include "gui/qt_convert.h"
#include "shop.h"

namespace {

QString path_text(const std::filesystem::path& path) {
    return QDir::toNativeSeparators(QString::fromStdWString(path.wstring()));
}

QString describe(const ShopException& error) {
    const auto reason = qt_convert::text(error.what());
    if (const auto* format = dynamic_cast<const FileFormatException*>(&error)) {
        return QStringLiteral("%1 · %2").arg(qt_convert::text(format->file()), reason);
    }
    return reason;
}

bool load_or_recover(DryCleaningShop& shop) {
    try {
        shop.load_all();
        return true;
    } catch (const DataCorruptionException& error) {
        const auto candidate = shop.find_recovery_snapshot();
        if (!candidate) {
            QMessageBox::critical(nullptr, QStringLiteral("Не удалось загрузить данные"),
                QStringLiteral("%1\nИсправных предыдущих снимков нет.\nХранилище: %2")
                    .arg(describe(error), path_text(shop.data_dir())));
            return false;
        }

        QMessageBox dialog(QMessageBox::Warning, QStringLiteral("Восстановление данных"),
            QStringLiteral("%1\nНайден исправный снимок от %2. Более поздние изменения могут быть потеряны.")
                .arg(describe(error), QDateTime::fromMSecsSinceEpoch(to_epoch_ms(candidate->saved_at))
                    .toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"))), QMessageBox::NoButton);
        dialog.setTextFormat(Qt::PlainText);
        auto* restore = dialog.addButton(QStringLiteral("Восстановить данные"), QMessageBox::AcceptRole);
        auto* exit = dialog.addButton(QStringLiteral("Выйти"), QMessageBox::RejectRole);
        dialog.setDefaultButton(exit);
        dialog.setEscapeButton(exit);
        dialog.exec();
        if (dialog.clickedButton() != restore) return false;

        const auto archive = shop.restore_snapshot(candidate->generation);
        QMessageBox::information(nullptr, QStringLiteral("Данные восстановлены"),
            QStringLiteral("Повреждённые файлы сохранены для диагностики.\n%1").arg(path_text(archive)));
        return true;
    }
}

bool retry_storage(const StorageException& error) {
    QMessageBox dialog(QMessageBox::Warning, QStringLiteral("Ошибка доступа к хранилищу"),
        QStringLiteral("%1\n%2").arg(qt_convert::text(error.what()), path_text(error.path())), QMessageBox::NoButton);
    dialog.setTextFormat(Qt::PlainText);
    auto* retry = dialog.addButton(QStringLiteral("Повторить"), QMessageBox::AcceptRole);
    auto* exit = dialog.addButton(QStringLiteral("Выйти"), QMessageBox::RejectRole);
    dialog.setDefaultButton(retry);
    dialog.setEscapeButton(exit);
    dialog.exec();
    return dialog.clickedButton() == retry;
}

}

bool load_shop(DryCleaningShop& shop) {
    for (;;) {
        try {
            return load_or_recover(shop);
        } catch (const StorageException& error) {
            if (!retry_storage(error)) return false;
        } catch (const ShopException& error) {
            QMessageBox::critical(nullptr, qt_convert::text(error.title()), describe(error));
            return false;
        }
    }
}
