#pragma once

#include <QStringList>

#include "suggestion.h"

class DryCleaningShop;
class QLineEdit;

QStringList record_suggestions(const DryCleaningShop& shop, SuggestionKind kind);
QStringList input_suggestions(const DryCleaningShop& shop, SuggestionKind kind);
void set_suggestions(QLineEdit* edit, const QStringList& values);
