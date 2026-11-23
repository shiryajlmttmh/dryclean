#pragma once

#include <QSet>
#include <QStringList>
#include <QTableWidget>
#include <QVariant>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>

#include "types.h"

class QAction;
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
    TableWidget(const QStringList& headers, QWidget* parent, bool has_actions = false);

    void reset_rows(int count);
    void finish_rows();
    void reset_sort();
    void set_empty_text(const QString& text);
    void fit_full_column_contents(int column);
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
    static constexpr int kNoColumn = -1;
    int sort_column_ = kNoColumn;
    Qt::SortOrder sort_order_ = Qt::DescendingOrder;
    int actions_column_ = kNoColumn;
    QString empty_text_;
    QSet<int> full_content_columns_;
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
void set_menu_action_unavailable(QAction* action, const QString& reason);
QMenu* add_more_actions(QHBoxLayout* layout, const QString& name);
