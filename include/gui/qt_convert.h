#pragma once

#include <QDateTime>
#include <QDir>
#include <algorithm>
#include <cmath>
#include <QString>
#include <filesystem>
#include <optional>
#include <string>

#include "types.h"

namespace qt_convert {

inline QString text(const std::string& value) { return QString::fromStdString(value); }

inline std::filesystem::path path(const QString& value) {
    return std::filesystem::path(QDir::toNativeSeparators(value).toStdWString());
}

inline QString format_time(TimePoint value) {
    return QDateTime::fromMSecsSinceEpoch(to_epoch_ms(value)).toString(QStringLiteral("dd.MM.yyyy HH:mm"));
}

inline QString format_time(const std::optional<TimePoint>& value) {
    return value ? format_time(*value) : QStringLiteral("—");
}

inline QString format_time_remaining(TimePoint ready_at, TimePoint now) {
    const auto hours_left = static_cast<qint64>(std::ceil(std::max(0.0, hours_between(ready_at, now))));
    return QStringLiteral("Осталось %1 ч, %2").arg(hours_left).arg(format_time(ready_at));
}

inline TimePoint now() { return from_epoch_ms(QDateTime::currentMSecsSinceEpoch()); }

}
