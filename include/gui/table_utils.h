#pragma once

#include <QStringList>
#include <QTableWidget>
#include <QVariant>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <vector>

#include "types.h"

class QEvent;
class QHBoxLayout;
class QIcon;
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

enum class ColumnKind { Compact, Text, WideText };

class TableWidget : public QTableWidget {
public:
    TableWidget(const QStringList& headers, QWidget* parent, bool has_actions = false);

    void reset_rows(int count);
    void finish_rows();
    void reset_sort();
    void set_empty_text(const QString& text);
    void set_text_columns(std::initializer_list<int> columns, ColumnKind kind = ColumnKind::Text);
    void reserve_column_text(int column, const QStringList& samples);
    int actions_column() const noexcept;

protected:
    bool viewportEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    int sizeHintForColumn(int column) const override;

private:
    void cycle_sort(int column);
    void measure_columns();
    void layout_columns();

    struct ColumnMetrics {
        ColumnKind kind = ColumnKind::Compact;
        QStringList samples;
        int minimum = 0;
        int preferred = 0;
        int maximum = 0;
    };

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
    std::vector<ColumnMetrics> columns_;
    bool layout_pending_ = false;
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
QToolButton* add_row_action(QHBoxLayout* layout, const QString& text, const QString& name,
                            const std::function<void()>& action, const QString& unavailable = {});
void add_row_icon_action(QHBoxLayout* layout, const QIcon& icon, const QString& name,
                         const std::function<void()>& action, const QString& unavailable = {});
void add_edit_action(QHBoxLayout* layout, const QString& name, const std::function<void()>& action,
                     const QString& unavailable = {});
void add_remove_action(QHBoxLayout* layout, const QString& name, const std::function<void()>& action,
                       const QString& unavailable = {});
QMenu* add_more_actions(QHBoxLayout* layout, const QString& name);
