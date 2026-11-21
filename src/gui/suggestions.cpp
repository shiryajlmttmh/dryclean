#include "gui/suggestions.h"

#include <QCompleter>
#include <QLineEdit>
#include <QSet>
#include <QStringListModel>
#include <algorithm>

#include "gui/qt_convert.h"
#include "shop.h"

namespace {

QStringList seeds(SuggestionKind kind) {
    switch (kind) {
        case SuggestionKind::ItemName:
            return {QStringLiteral("пальто"), QStringLiteral("куртка"), QStringLiteral("пиджак"),
                    QStringLiteral("брюки"), QStringLiteral("платье"), QStringLiteral("шарф"),
                    QStringLiteral("рубашка"), QStringLiteral("юбка"), QStringLiteral("плед")};
        case SuggestionKind::DefectName:
            return {QStringLiteral("жирное пятно"), QStringLiteral("кофе"), QStringLiteral("чернила"),
                    QStringLiteral("грязь"), QStringLiteral("потёртость"), QStringLiteral("разрыв")};
        case SuggestionKind::MaterialName:
            return {QStringLiteral("порошок"), QStringLiteral("пятновыводитель"), QStringLiteral("ополаскиватель")};
        case SuggestionKind::Unit:
            return {QStringLiteral("кг"), QStringLiteral("л"), QStringLiteral("г"), QStringLiteral("мл")};
        case SuggestionKind::MachineName:
            return {QStringLiteral("Машина сухой чистки"), QStringLiteral("Машина влажной чистки")};
        case SuggestionKind::ClientName:
        case SuggestionKind::EmployeeName:
            return {};
    }
    return {};
}

QStringList unique_values(const QStringList& values) {
    QStringList result;
    QSet<QString> seen;
    for (const QString& value : values) {
        const auto key = value.toCaseFolded();
        if (!value.isEmpty() && !seen.contains(key)) {
            seen.insert(key);
            result.append(value);
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const QString& left, const QString& right) {
        return QString::localeAwareCompare(left, right) < 0;
    });
    return result;
}

template <class Records>
void append_names(QStringList& values, const Records& records) {
    for (const auto& record : records) values.append(qt_convert::text(record.name()));
}

}

QStringList record_suggestions(const DryCleaningShop& shop, SuggestionKind kind) {
    QStringList values;
    switch (kind) {
        case SuggestionKind::ItemName:
        case SuggestionKind::DefectName:
            for (const Item& item : shop.items()) {
                if (kind == SuggestionKind::ItemName) {
                    values.append(qt_convert::text(item.kind()));
                } else {
                    for (const auto& defect : item.defects()) values.append(qt_convert::text(defect.name));
                }
            }
            break;
        case SuggestionKind::MaterialName: append_names(values, shop.materials()); break;
        case SuggestionKind::Unit:
            for (const Material& material : shop.materials()) values.append(qt_convert::text(material.unit()));
            break;
        case SuggestionKind::MachineName: append_names(values, shop.machines()); break;
        case SuggestionKind::ClientName: append_names(values, shop.clients()); break;
        case SuggestionKind::EmployeeName: append_names(values, shop.employees()); break;
    }
    return unique_values(values);
}

QStringList input_suggestions(const DryCleaningShop& shop, SuggestionKind kind) {
    QStringList values = record_suggestions(shop, kind);
    values.append(seeds(kind));
    for (const Suggestion& suggestion : shop.suggestions()) {
        if (suggestion.kind() == kind) values.append(qt_convert::text(suggestion.value()));
    }
    return unique_values(values);
}

void set_suggestions(QLineEdit* edit, const QStringList& values) {
    if (auto* completer = edit->completer()) {
        if (auto* model = qobject_cast<QStringListModel*>(completer->model())) {
            if (model->stringList() != values) model->setStringList(values);
            return;
        }
    }
    auto* completer = new QCompleter(values, edit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchStartsWith);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    edit->setCompleter(completer);
}
