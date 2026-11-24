#include "gui/table_utils.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHeaderView>
#include <QHelpEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <algorithm>

#include "gui/helpers.h"
#include "gui/qt_convert.h"

namespace {

constexpr int kBaseRowRole = Qt::UserRole + 1;
constexpr int kSortValueRole = Qt::UserRole + 2;
constexpr int kCompactMinCharacters = 4;
constexpr int kCompactMaxCharacters = 27;
constexpr int kTextMinCharacters = 13;
constexpr int kTextMaxCharacters = 34;
constexpr int kWideTextMinCharacters = 17;
constexpr int kWideTextMaxCharacters = 48;
constexpr int kMeasureAllRows = -1;
constexpr int kEditIconSize = 16;
constexpr qreal kEditIconStrokeWidth = 1.3;
constexpr QPointF kPencilTip{2.5, 13.5};
constexpr QPointF kPencilLowerLeft{3.4, 9.3};
constexpr QPointF kPencilUpperLeft{10.7, 2.0};
constexpr QPointF kPencilUpperRight{14.0, 5.3};
constexpr QPointF kPencilLowerRight{6.7, 12.6};
constexpr QPointF kPencilCapLeft{9.4, 3.3};
constexpr QPointF kPencilCapRight{12.7, 6.6};
constexpr int kGridLineWidth = 1;
constexpr int kEmptyTextHorizontalInset = 12;
constexpr int kEmptyTextVerticalInset = 4;
constexpr int kActionHorizontalMargin = 3;
constexpr int kActionVerticalMargin = 1;
constexpr int kActionSpacing = 4;
constexpr auto kRecordIdProperty = "recordId";

struct ColumnLimits {
    int minimum;
    int maximum;
};

ColumnLimits character_limits(ColumnKind kind) {
    switch (kind) {
        case ColumnKind::Compact: return {kCompactMinCharacters, kCompactMaxCharacters};
        case ColumnKind::WideText: return {kWideTextMinCharacters, kWideTextMaxCharacters};
        default: return {kTextMinCharacters, kTextMaxCharacters};
    }
}

class TableHeader : public QHeaderView {
public:
    TableHeader(QWidget* parent, int actions_column)
        : QHeaderView(Qt::Horizontal, parent), actions_column_(actions_column) {}

protected:
    QSize sectionSizeFromContents(int column) const override {
        ensurePolished();
        QStyleOptionHeader option;
        initStyleOption(&option);
        initStyleOptionForIndex(&option, column);
        if (column != actions_column_) option.sortIndicator = QStyleOptionHeader::SortDown;
        return style()->sizeFromContents(QStyle::CT_HeaderSection, &option, {}, this);
    }

private:
    int actions_column_;
};

class TableItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;

    bool operator<(const QTableWidgetItem& other) const override {
        if (!tableWidget()->horizontalHeader()->isSortIndicatorShown()) {
            return data(kBaseRowRole).toInt() < other.data(kBaseRowRole).toInt();
        }
        const QVariant left = data(kSortValueRole);
        const QVariant right = other.data(kSortValueRole);
        if (!left.isValid() || !right.isValid()) {
            return !left.isValid() && right.isValid();
        }
        if (left.metaType().id() == QMetaType::QString) {
            return QString::localeAwareCompare(left.toString(), right.toString()) < 0;
        }
        return QVariant::compare(left, right) == QPartialOrdering::Less;
    }
};

QIcon edit_icon(const QWidget* widget) {
    const qreal scale = widget->devicePixelRatioF();
    QPixmap pixmap(qRound(kEditIconSize * scale), qRound(kEditIconSize * scale));
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(widget->palette().color(QPalette::ButtonText), kEditIconStrokeWidth, Qt::SolidLine, Qt::RoundCap,
                        Qt::RoundJoin));
    painter.drawPolygon(QPolygonF{kPencilTip, kPencilLowerLeft, kPencilUpperLeft, kPencilUpperRight, kPencilLowerRight});
    painter.drawLine(kPencilCapLeft, kPencilCapRight);
    return QIcon(pixmap);
}

class RowActionMenu final : public QMenu {
public:
    using QMenu::QMenu;

protected:
    bool event(QEvent* event) override {
        if (event->type() == QEvent::ToolTip) {
            const auto* help = static_cast<QHelpEvent*>(event);
            auto* action = actionAt(help->pos());
            if (action && !action->isEnabled()) {
                QToolTip::showText(help->globalPos(), action->toolTip(), this, actionGeometry(action));
                return true;
            }
        }
        return QMenu::event(event);
    }
};

QToolButton* make_action_button(QHBoxLayout* layout, const QString& name, const QString& unavailable) {
    auto* button = new QToolButton(layout->parentWidget());
    button->setFocusPolicy(Qt::StrongFocus);
    button->setAccessibleName(name);
    set_action_availability(button, unavailable, name);
    auto size_policy = button->sizePolicy();
    size_policy.setRetainSizeWhenHidden(true);
    button->setSizePolicy(size_policy);
    layout->addWidget(button);
    return button;
}

}

TableWidget::TableWidget(const QStringList& headers, QWidget* parent, bool has_actions)
    : QTableWidget(parent), empty_text_(QStringLiteral("Нет записей.")) {
    QStringList labels = headers;
    if (has_actions) {
        actions_column_ = static_cast<int>(labels.size());
        labels << QStringLiteral("Действия");
    }
    columns_.resize(static_cast<std::size_t>(labels.size()));
    auto* header = new TableHeader(this, actions_column_);
    setHorizontalHeader(header);
    setColumnCount(static_cast<int>(labels.size()));
    setHorizontalHeaderLabels(labels);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAlternatingRowColors(true);
    setTabKeyNavigation(false);
    setWordWrap(false);
    setTextElideMode(Qt::ElideRight);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    verticalHeader()->hide();
    header->setSectionsClickable(true);
    header->setStretchLastSection(false);
    header->setResizeContentsPrecision(kMeasureAllRows);
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(header, &QHeaderView::sectionClicked, this, &TableWidget::cycle_sort);
    measure_columns();
}

void TableWidget::reset_rows(int count) {
    focused_action_.reset();
    auto* button = qobject_cast<QToolButton*>(QApplication::focusWidget());
    if (button && viewport()->isAncestorOf(button)) {
        const auto id = button->parentWidget()->property(kRecordIdProperty);
        if (id.isValid()) {
            focused_action_ = FocusedAction{id.toUInt(), button->accessibleName()};
        }
    }
    setRowCount(0);
    setRowCount(count);
    setAccessibleDescription(count == 0 ? empty_text_ : QString());
}

void TableWidget::finish_rows() {
    setAccessibleDescription(rowCount() == 0 ? empty_text_ : QString());
    if (sort_column_ >= 0) {
        sortItems(sort_column_, sort_order_);
    }
    measure_columns();
    if (!focused_action_) return;
    const FocusedAction focus = *focused_action_;
    focused_action_.reset();
    for (int row = 0; row < rowCount(); ++row) {
        auto* panel = cellWidget(row, actions_column_);
        if (!panel || panel->property(kRecordIdProperty).toUInt() != focus.id) continue;
        for (auto* button : panel->findChildren<QToolButton*>()) {
            if (button->isEnabled() && button->accessibleName() == focus.name) {
                button->setFocus(Qt::OtherFocusReason);
                return;
            }
        }
        break;
    }
    setFocus(Qt::OtherFocusReason);
}

void TableWidget::set_empty_text(const QString& text) {
    empty_text_ = text;
    setAccessibleDescription(rowCount() == 0 ? text : QString());
    viewport()->update();
}

void TableWidget::set_text_columns(std::initializer_list<int> columns, ColumnKind kind) {
    for (int column : columns) columns_.at(column).kind = kind;
    measure_columns();
}

void TableWidget::reserve_column_text(int column, const QStringList& samples) {
    columns_.at(column).samples = samples;
    measure_columns();
}

void TableWidget::measure_columns() {
    auto* header = horizontalHeader();
    const auto metrics = fontMetrics();
    const int char_width = metrics.averageCharWidth();
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.features = QStyleOptionViewItem::HasDisplay;
    const auto cell_width = [this, &option](const QString& sample) {
        option.text = sample;
        return style()->sizeFromContents(QStyle::CT_ItemViewItem, &option, {}, this).width() +
               (showGrid() ? kGridLineWidth : 0);
    };

    for (int column = 0; column < columnCount(); ++column) {
        if (isColumnHidden(column)) continue;
        auto& widths = columns_[column];
        const int heading = header->sectionSizeHint(column);
        const int contents = sizeHintForColumn(column);
        if (column == actions_column_) {
            widths.minimum = widths.preferred = widths.maximum = std::max(heading, contents);
            continue;
        }

        const auto limits = character_limits(widths.kind);
        int minimum = std::max(heading, limits.minimum * char_width);
        for (const auto& sample : widths.samples) minimum = std::max(minimum, cell_width(sample));
        const int maximum = std::max(minimum, limits.maximum * char_width);
        const int preferred = std::clamp(contents, minimum, maximum);
        widths.preferred = preferred;
        widths.minimum = widths.kind == ColumnKind::Compact ? preferred : minimum;
        widths.maximum = widths.kind == ColumnKind::Compact ? preferred : maximum;
    }
    layout_columns();
}

void TableWidget::layout_columns() {
    auto* header = horizontalHeader();
    std::vector<int> widths(columns_.size());
    int room = viewport()->width();
    for (int column = 0; column < columnCount(); ++column) {
        if (isColumnHidden(column)) continue;
        widths[column] = columns_[column].preferred;
        room -= widths[column];
    }

    const bool grow = room >= 0;
    int remaining = grow ? room : -room;
    const auto capacity = [&](int column) {
        if (isColumnHidden(column)) return 0;
        return grow ? columns_[column].maximum - widths[column] : widths[column] - columns_[column].minimum;
    };
    while (remaining > 0) {
        int adjustable = 0;
        for (int column = 0; column < columnCount(); ++column) {
            if (capacity(column) > 0) ++adjustable;
        }
        if (adjustable == 0) break;

        const int share = std::max(1, remaining / adjustable);
        for (int column = 0; column < columnCount() && remaining > 0; ++column) {
            const int amount = std::min({share, remaining, capacity(column)});
            widths[column] += grow ? amount : -amount;
            remaining -= amount;
        }
    }

    for (int column = 0; column < columnCount(); ++column) {
        if (widths[column] > 0 && widths[column] != columnWidth(column)) {
            header->resizeSection(column, widths[column]);
        }
    }
}

bool TableWidget::viewportEvent(QEvent* event) {
    const bool handled = QTableWidget::viewportEvent(event);
    if (event->type() == QEvent::Resize && !layout_pending_) {
        layout_pending_ = true;
        QTimer::singleShot(0, this, [this] {
            layout_pending_ = false;
            layout_columns();
        });
    }
    return handled;
}

void TableWidget::changeEvent(QEvent* event) {
    QTableWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange ||
        event->type() == QEvent::StyleChange) {
        measure_columns();
    }
}

int TableWidget::actions_column() const noexcept { return actions_column_; }

void TableWidget::reset_sort() {
    sort_column_ = kNoColumn;
    horizontalHeader()->setSortIndicatorShown(false);
    sortItems(0, Qt::AscendingOrder);
}

void TableWidget::cycle_sort(int column) {
    auto* header = horizontalHeader();
    if (column == actions_column_) {
        header->setSortIndicator(sort_column_, sort_order_);
        return;
    }
    if (column == sort_column_ && sort_order_ == Qt::AscendingOrder) {
        reset_sort();
        return;
    }

    sort_order_ = column == sort_column_ ? Qt::AscendingOrder : Qt::DescendingOrder;
    sort_column_ = column;
    header->setSortIndicatorShown(true);
    sortItems(sort_column_, sort_order_);
}

int TableWidget::sizeHintForColumn(int column) const {
    if (column != actions_column_) {
        return QTableWidget::sizeHintForColumn(column);
    }

    int width = 0;
    for (int row = 0; row < rowCount(); ++row) {
        if (const auto* actions = cellWidget(row, column)) {
            width = std::max(width, actions->sizeHint().width());
        }
    }
    return width + (showGrid() ? kGridLineWidth : 0);
}

void TableWidget::paintEvent(QPaintEvent* event) {
    QTableWidget::paintEvent(event);
    if (rowCount() == 0) {
        QPainter painter(viewport());
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(viewport()->rect().adjusted(kEmptyTextHorizontalInset, kEmptyTextVerticalInset,
                                                     -kEmptyTextHorizontalInset, -kEmptyTextVerticalInset),
                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, empty_text_);
    }
}

void TableWidget::keyPressEvent(QKeyEvent* event) {
    const int key = event->key();
    if (actions_column_ >= 0 && (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space)) {
        if (auto* actions = cellWidget(currentRow(), actions_column_)) {
            for (auto* button : actions->findChildren<QToolButton*>()) {
                if (button->isEnabled()) {
                    button->setFocus(Qt::TabFocusReason);
                    event->accept();
                    return;
                }
            }
        }
    }
    QTableWidget::keyPressEvent(event);
}

void set_cell(TableWidget* table, int row, int column, const QString& text, bool right_aligned) {
    set_sortable_cell(table, row, column, text, text, right_aligned);
}

void set_cell(TableWidget* table, int row, int column, qulonglong value) {
    set_sortable_cell(table, row, column, QString::number(value), value, true);
}

void set_sortable_cell(TableWidget* table, int row, int column, const QString& text,
                       const QVariant& sort_value, bool right_aligned) {
    auto* item = new TableItem(text);
    item->setToolTip(text);
    item->setData(kBaseRowRole, row);
    item->setData(kSortValueRole, sort_value);
    if (right_aligned) {
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    table->setItem(row, column, item);
}

QVariant time_sort_value(std::optional<TimePoint> value) {
    return value ? QVariant::fromValue<qlonglong>(to_epoch_ms(*value)) : QVariant();
}

void set_time_cell(TableWidget* table, int row, int column, std::optional<TimePoint> value) {
    set_sortable_cell(table, row, column, qt_convert::format_time(value), time_sort_value(value));
}

QHBoxLayout* make_row_actions(TableWidget* table, int row, Id id) {
    auto* panel = new QWidget(table);
    panel->setProperty(kRecordIdProperty, id);
    auto* layout = new QHBoxLayout(panel);
    layout->setContentsMargins(kActionHorizontalMargin, kActionVerticalMargin,
                               kActionHorizontalMargin, kActionVerticalMargin);
    layout->setSpacing(kActionSpacing);
    layout->setAlignment(Qt::AlignLeft);
    table->setCellWidget(row, table->actions_column(), panel);
    return layout;
}

QToolButton* add_row_action(QHBoxLayout* layout, const QString& text, const std::function<void()>& action,
                            const QString& unavailable) {
    auto* button = make_action_button(layout, text, unavailable);
    button->setText(text);
    QObject::connect(button, &QToolButton::clicked, button, action);
    return button;
}

void add_edit_action(QHBoxLayout* layout, const QString& name, const std::function<void()>& action,
                     const QString& unavailable) {
    auto* button = make_action_button(layout, name, unavailable);
    button->setIcon(edit_icon(button));
    QObject::connect(button, &QToolButton::clicked, button, action);
}

void add_remove_action(QHBoxLayout* layout, const QString& name, const std::function<void()>& action,
                       const QString& unavailable) {
    auto* button = make_action_button(layout, name, unavailable);
    button->setIcon(button->style()->standardIcon(QStyle::SP_TrashIcon));
    QObject::connect(button, &QToolButton::clicked, button, action);
}

void set_menu_action_unavailable(QAction* action, const QString& reason) {
    action->setEnabled(false);
    action->setToolTip(reason);
}

QMenu* add_more_actions(QHBoxLayout* layout, const QString& name) {
    auto* button = make_action_button(layout, name, {});
    button->setText(QStringLiteral("⋯"));
    auto* menu = new RowActionMenu(button);
    button->setMenu(menu);
    button->setPopupMode(QToolButton::InstantPopup);
    return menu;
}
