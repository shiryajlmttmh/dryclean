#include "gui/table_utils.h"

#include <QAction>
#include <QApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QToolButton>
#include <algorithm>

#include "gui/qt_convert.h"

namespace {

constexpr int kBaseRowRole = Qt::UserRole + 1;
constexpr int kSortValueRole = Qt::UserRole + 2;
constexpr int kMaxDataCellWidth = 180;
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
constexpr auto kActionReasonName = "actionReason";
constexpr auto kRecordIdProperty = "recordId";

class TableHeader : public QHeaderView {
public:
    explicit TableHeader(QWidget* parent) : QHeaderView(Qt::Horizontal, parent) {}

protected:
    QSize sectionSizeFromContents(int column) const override {
        ensurePolished();
        QStyleOptionHeader option;
        initStyleOption(&option);
        initStyleOptionForIndex(&option, column);
        return style()->sizeFromContents(QStyle::CT_HeaderSection, &option, {}, this);
    }
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

void add_action_reason(QHBoxLayout* layout, const QString& reason) {
    auto* panel = layout->parentWidget();
    auto* label = panel->findChild<QLabel*>(QString::fromLatin1(kActionReasonName), Qt::FindDirectChildrenOnly);
    if (!label) {
        label = new QLabel(reason, panel);
        label->setObjectName(QString::fromLatin1(kActionReasonName));
        label->setTextFormat(Qt::PlainText);
        auto size_policy = label->sizePolicy();
        size_policy.setRetainSizeWhenHidden(true);
        label->setSizePolicy(size_policy);
        layout->insertWidget(0, label);
    } else if (label->text() != reason) {
        label->setText(label->text() + QStringLiteral(" · ") + reason);
    }
}

QToolButton* make_action_button(QHBoxLayout* layout, const QString& name, const QString& unavailable) {
    auto* button = new QToolButton(layout->parentWidget());
    button->setFocusPolicy(Qt::StrongFocus);
    button->setAccessibleName(name);
    button->setAccessibleDescription(unavailable);
    button->setToolTip(unavailable.isEmpty() ? name : unavailable);
    button->setEnabled(unavailable.isEmpty());
    auto size_policy = button->sizePolicy();
    size_policy.setRetainSizeWhenHidden(true);
    button->setSizePolicy(size_policy);
    layout->addWidget(button);
    if (!unavailable.isEmpty()) {
        add_action_reason(layout, unavailable);
    }
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
    auto* header = new TableHeader(this);
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
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(header, &QHeaderView::sectionClicked, this, &TableWidget::cycle_sort);
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
    resizeColumnsToContents();
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
        return std::min(QTableWidget::sizeHintForColumn(column), kMaxDataCellWidth);
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
                         Qt::AlignCenter | Qt::TextWordWrap, empty_text_);
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
    layout->setAlignment(Qt::AlignRight);
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
    action->setText(QStringLiteral("%1 - %2").arg(action->text(), reason));
}

QMenu* add_more_actions(QHBoxLayout* layout, const QString& name) {
    auto* button = make_action_button(layout, name, {});
    button->setText(QStringLiteral("⋯"));
    auto* menu = new QMenu(button);
    button->setMenu(menu);
    button->setPopupMode(QToolButton::InstantPopup);
    return menu;
}
