#pragma once

#include <QStringList>
#include <QTableWidget>
#include <QVariant>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>

#include "types.h"

class QHBoxLayout;
class QMenu;
class QToolButton;

template <std::size_t N>
QStringList make_headers(const std::array<const char*, N>& names) {
    QStringList headers;
    for (const char* name : names) {
        headers << QString::fromUtf8(name);
    }
    return headers;
}

class TableWidget : public QTableWidget {
public:
    TableWidget(const QStringList& headers, QWidget* parent, int stretch_column = -1, bool has_actions = false);

    void reset_rows(int count);
    void finish_rows();
    void set_empty_text(const QString& text);
    void add_stretch_column(int column);
    int actions_column() const noexcept;

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    int sizeHintForColumn(int column) const override;

private:
    void cycle_sort(int column);

    struct FocusedAction {
        Id id;
        QString name;
    };
    std::optional<FocusedAction> focused_action_;
    int sort_column_ = -1;
    Qt::SortOrder sort_order_ = Qt::DescendingOrder;
    int actions_column_ = -1;
    QString empty_text_;
};

void set_cell(TableWidget* table, int row, int column, const QString& text, bool right_aligned = false);
void set_cell(TableWidget* table, int row, int column, qulonglong value);
void set_sortable_cell(TableWidget* table, int row, int column, const QString& text,
                       const QVariant& sort_value, bool right_aligned = false);
void set_time_cell(TableWidget* table, int row, int column, std::optional<TimePoint> value);
QVariant time_sort_value(std::optional<TimePoint> value);

QHBoxLayout* make_row_actions(TableWidget* table, int row, Id id);
QToolButton* add_row_action(QHBoxLayout* layout, const QString& text, const std::function<void()>& action,
                            const QString& unavailable = {});
void add_edit_action(QHBoxLayout* layout, const QString& name, const std::function<void()>& action,
                     const QString& unavailable = {});
void add_remove_action(QHBoxLayout* layout, const QString& name, const std::function<void()>& action,
                       const QString& unavailable = {});
QMenu* add_more_actions(QHBoxLayout* layout, const QString& name);
