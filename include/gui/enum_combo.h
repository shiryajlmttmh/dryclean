#pragma once

#include <QComboBox>
#include <QString>
#include <initializer_list>

#include "display.h"

template <typename Enum>
void fill_enum_combo(QComboBox* box, std::initializer_list<Enum> values) {
    for (const Enum value : values) {
        box->addItem(QString::fromStdString(display_name(value)), static_cast<int>(value));
    }
}

template <typename Enum>
Enum enum_value(const QComboBox* box) {
    return static_cast<Enum>(box->currentData().toInt());
}

template <typename Enum>
void select_enum(QComboBox* box, Enum value) {
    box->setCurrentIndex(box->findData(static_cast<int>(value)));
}
