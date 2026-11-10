#pragma once

#include <QString>
#include <QStringList>
#include <array>
#include <cstddef>

class QTableWidget;

template <std::size_t N>
QStringList make_headers(const std::array<const char*, N>& names) {
    QStringList headers;
    for (const char* name : names) {
        headers << QString::fromUtf8(name);
    }
    return headers;
}

void configure_table(QTableWidget* table, const QStringList& headers);
void set_cell(QTableWidget* table, int row, int column, const QString& text, bool right_aligned = false);
