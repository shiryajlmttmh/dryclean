#include "gui/machines_tab.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>

#include "display.h"
#include "gui/machine_dialog.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"
#include "gui/helpers.h"

namespace {

constexpr int kCapacityDecimals = 1;
constexpr int kMaintenanceDecimals = 1;
constexpr int kHistoryWindowWidth = 1000;
constexpr int kHistoryWindowHeight = 560;
constexpr int kDaysPerWeek = 7;
constexpr int kScaleHeight = 104;
constexpr qreal kSidePadding = 30;
constexpr qreal kTrackTop = 35;
constexpr qreal kTrackHeight = 46;
constexpr qreal kMinTrackWidth = 1;
constexpr qreal kLabelTop = 5;
constexpr qreal kMajorTickAboveTrack = 7;
constexpr qreal kMinorTickAboveTrack = 3;
constexpr qreal kTickBelowTrack = 3;
constexpr qreal kTickPenWidth = 1;
constexpr qreal kDayLabelGap = 6;
constexpr int kDateControlGap = 8;
constexpr auto kDayTickInterval = std::chrono::hours(1);
constexpr auto kFreeTimeColor = "#86cfa5";
constexpr auto kCompletedBatchColor = "#5d9bd3";
constexpr auto kRunningBatchColor = "#ab8be3";
constexpr auto kScheduledBatchColor = "#efad55";

enum HistoryColumn { HistoryBatch, HistoryStart, HistoryReady, HistoryStatus, HistoryCompleted, HistoryColumnCount };

enum MachineColumn { MachineId, MachineName, MachineType, MachineCapacity, MachineFreeAt, MachineMaintenance, MachineColumnCount };

constexpr std::array<const char*, MachineColumnCount> kMachineHeaders{"№", "Название", "Тип", "Объём, кг",
                                                                      "Доступность", "До ТО, ч"};
constexpr std::array<const char*, HistoryColumnCount> kHistoryHeaders{
    "Партия", "Начало", "Конец цикла", "Статус", "Подтверждение завершения"};

QDate first_day(QDate date, bool weekly) {
    return weekly ? date.addDays(1 - date.dayOfWeek()) : date;
}

TimePoint midnight(QDate date) {
    return from_epoch_ms(date.startOfDay().toMSecsSinceEpoch());
}

QString exact_time(TimePoint time) {
    return QDateTime::fromMSecsSinceEpoch(to_epoch_ms(time)).toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
}

QString projected_time(const QString& time, bool lower_bound) {
    return lower_bound ? QStringLiteral("не раньше %1").arg(time) : time;
}

QString batch_tooltip(const Batch& batch, const BatchForecast& projection) {
    const auto start_label = batch.started_at() ? QStringLiteral("Фактический запуск") : QStringLiteral("Прогноз начала");
    const auto end_label = batch.completed_at() ? QStringLiteral("Фактическое подтверждение завершения")
                                               : QStringLiteral("Расчётный конец цикла");
    return QStringLiteral("Партия №%1 · %2\n%3: %4\n%5: %6\nПлан: %7 - %8")
        .arg(batch.id())
        .arg(qt_convert::text(display_name(projection.status)), start_label,
             projected_time(exact_time(projection.start), projection.lower_bound), end_label,
             projected_time(exact_time(projection.end), projection.lower_bound),
             exact_time(batch.planned_start_at()), exact_time(batch.planned_ready_at()));
}

class MachineTimeline final : public QGraphicsView {
public:
    MachineTimeline(const DryCleaningShop& shop, const ShopForecast& forecast, Id machine, QWidget* parent)
        : QGraphicsView(parent), shop_(shop), forecast_(forecast), machine_(machine) {
        setScene(new QGraphicsScene(this));
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFixedHeight(kScaleHeight);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(QStringLiteral("Календарная шкала занятости машины"));
    }

    void set_period(QDate date, bool weekly) {
        date_ = date;
        weekly_ = weekly;
        setAccessibleDescription(weekly_ ? QStringLiteral("Стрелки выбирают день, Enter открывает его.") : QString());
        redraw();
    }

    void on_day_selected(std::function<void(QDate)> action) { select_day_ = std::move(action); }
    void refresh() { redraw(); }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QGraphicsView::resizeEvent(event);
        if (date_.isValid()) redraw();
    }

    void mousePressEvent(QMouseEvent* event) override {
        const QPointF point = mapToScene(event->position().toPoint());
        if (weekly_ && event->button() == Qt::LeftButton && point.y() >= 0 && point.y() <= kTrackTop + kTrackHeight &&
            point.x() >= track().left() && point.x() < track().right()) {
            const QDate week = first_day(date_, true);
            for (int day = 0; day < kDaysPerWeek; ++day) {
                if (point.x() < x_at(midnight(week.addDays(day + 1)))) {
                    select_day_(week.addDays(day));
                    event->accept();
                    return;
                }
            }
        }
        QGraphicsView::mousePressEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (weekly_ && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
            select_day_(date_);
            event->accept();
            return;
        }
        if (weekly_ && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
            const auto day = date_.addDays(event->key() == Qt::Key_Left ? -1 : 1);
            if (first_day(day, true) == first_day(date_, true)) date_ = day;
            redraw();
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

private:
    QRectF track() const {
        return {kSidePadding, kTrackTop, std::max(kMinTrackWidth, viewport()->width() - 2 * kSidePadding), kTrackHeight};
    }

    qreal x_at(TimePoint time) const {
        const QRectF area = track();
        return area.left() + area.width() * hours_between(time, start_) / hours_between(end_, start_);
    }

    void add_label(const QString& text, qreal center, bool selected = false) {
        auto label_font = font();
        label_font.setBold(selected);
        auto* label = scene()->addSimpleText(text, label_font);
        label->setBrush(palette().text());
        const qreal width = label->boundingRect().width();
        label->setPos(std::max(qreal(0), std::min(center - width / 2, viewport()->width() - width)), kLabelTop);
    }

    void add_tick(qreal x, bool major) {
        scene()->addLine(x, kTrackTop - (major ? kMajorTickAboveTrack : kMinorTickAboveTrack),
                         x, track().bottom() + kTickBelowTrack, QPen(palette().mid(), kTickPenWidth));
    }

    void draw_week(QDate monday) {
        constexpr std::array weekdays{"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
        for (int i = 0; i < kDaysPerWeek; ++i) {
            const auto date = monday.addDays(i);
            const qreal left = x_at(midnight(date));
            const qreal right = x_at(midnight(date.addDays(1)));
            add_tick(left, true);
            add_label(QStringLiteral("%1 %2").arg(QString::fromUtf8(weekdays[i]), date.toString(QStringLiteral("dd.MM"))),
                      (left + right) / 2, date == date_);
        }
        add_tick(track().right(), true);
    }

    void draw_day() {
        const auto tick_label = [](TimePoint time) {
            return QDateTime::fromMSecsSinceEpoch(to_epoch_ms(time)).toString(QStringLiteral("HH:mm"));
        };
        const qreal spacing = track().width() / hours_between(end_, start_);
        const qreal label_width = fontMetrics().horizontalAdvance(QStringLiteral("00:00")) + kDayLabelGap;
        const int label_every = std::max(1, static_cast<int>(std::ceil(label_width / spacing)));
        int index = 0;
        for (auto tick = start_; tick < end_; tick += kDayTickInterval, ++index) {
            const bool label = index % label_every == 0 && (tick == start_ || x_at(end_) - x_at(tick) >= label_width);
            add_tick(x_at(tick), label);
            if (label) add_label(tick_label(tick), x_at(tick));
        }
        add_tick(x_at(end_), true);
        add_label(tick_label(end_), x_at(end_));
    }

    void redraw() {
        scene()->clear();
        scene()->setSceneRect(0, 0, viewport()->width(), viewport()->height());
        const QDate first = first_day(date_, weekly_);
        start_ = midnight(first);
        end_ = midnight(first.addDays(weekly_ ? kDaysPerWeek : 1));
        if (end_ <= start_) return;

        scene()->addRect(track(), QPen(Qt::NoPen), QBrush(QColor(kFreeTimeColor)));
        for (const Batch& batch : shop_.batches()) {
            if (batch.machine_id() != machine_) continue;
            const auto& projection = forecast_.batches.at(batch.id());
            const TimePoint start = std::max(start_, projection.start);
            const TimePoint end = std::min(end_, projection.release);
            if (end <= start) continue;
            const auto color = batch.completed_at() ? kCompletedBatchColor
                : batch.started_at() ? kRunningBatchColor : kScheduledBatchColor;
            auto* block = scene()->addRect(x_at(start), kTrackTop, x_at(end) - x_at(start), kTrackHeight,
                                          QPen(Qt::NoPen), QBrush(QColor(color)));
            block->setToolTip(batch_tooltip(batch, projection));
        }
        if (weekly_) draw_week(first);
        else draw_day();
    }

    const DryCleaningShop& shop_;
    const ShopForecast& forecast_;
    Id machine_;
    QDate date_;
    bool weekly_ = true;
    TimePoint start_{};
    TimePoint end_{};
    std::function<void(QDate)> select_day_;
};

}

MachinesTab::MachinesTab(DryCleaningShop& shop, Id employee_id, const ShopForecast& forecast, QWidget* parent)
    : TableTab(shop, employee_id, Permission::ManageStaff, make_headers(kMachineHeaders),
               parent, true), forecast_(forecast) {}

void MachinesTab::refresh_history() { emit forecast_changed(); }

void MachinesTab::add_extra_actions(QHBoxLayout* actions, Id id) {
    add_row_action(actions, QStringLiteral("Занятость"), [this, id] { show_history(id); });
    if (is_allowed()) {
        auto* menu = add_more_actions(actions, QStringLiteral("Обслуживание машины №%1").arg(id));
        auto* service = menu->addAction(QStringLiteral("Выполнить ТО"), this, [this, id] { service_machine(id); });
        if (!shop_.can_service_machine(id, forecast_.now)) {
            set_menu_action_unavailable(service, QStringLiteral("сначала завершите все партии машины"));
        }
    }
}

QString MachinesTab::removal_block(Id id) const {
    return shop_.can_remove_machine(id) ? QString() : QStringLiteral("У машины есть партии.");
}

void MachinesTab::service_machine(Id id) {
    if (!check_action_permission(this, shop_, employee_id_, Permission::ManageStaff)) return;
    if (!shop_.can_service_machine(id, qt_convert::now())) {
        show_action_refusal(this, QStringLiteral("Сначала завершите все партии машины."));
        return;
    }
    if (!confirm(this, QStringLiteral("Техобслуживание"),
                 QStringLiteral("Подтвердить выполненное ТО и обнулить счётчик часов?"))) return;
    if (run_guarded(this, [this, id] { shop_.service_machine(id, employee_id_, qt_convert::now()); })) {
        emit changed();
    }
}

void MachinesTab::show_history(Id id) {
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(kHistoryWindowWidth, kHistoryWindowHeight);
    auto* layout = new QVBoxLayout(dialog);
    auto* modes = new QButtonGroup(dialog);
    auto* day_button = new QToolButton(dialog);
    day_button->setText(QStringLiteral("День"));
    day_button->setCheckable(true);
    modes->addButton(day_button);
    auto* week_button = new QToolButton(dialog);
    week_button->setText(QStringLiteral("Неделя"));
    week_button->setCheckable(true);
    modes->addButton(week_button);
    week_button->setChecked(true);

    auto* previous = new QToolButton(dialog);
    previous->setArrowType(Qt::LeftArrow);
    auto* next = new QToolButton(dialog);
    next->setArrowType(Qt::RightArrow);
    auto* today = new QToolButton(dialog);
    today->setText(QStringLiteral("Сегодня"));
    auto* period = new QLabel(dialog);
    auto* day_hint = new QLabel(QStringLiteral("Нажмите на день, чтобы открыть его шкалу."), dialog);
    day_hint->setWordWrap(true);
    auto* date_controls = new QHBoxLayout;
    date_controls->addWidget(day_button);
    date_controls->addWidget(week_button);
    date_controls->addSpacing(kDateControlGap);
    date_controls->addWidget(previous);
    date_controls->addWidget(next);
    date_controls->addWidget(today);
    date_controls->addWidget(period);
    date_controls->addStretch();
    layout->addLayout(date_controls);
    layout->addWidget(day_hint);
    auto* view = new MachineTimeline(shop_, forecast_, id, dialog);
    layout->addWidget(view);
    auto* legend = new QLabel(QStringLiteral(
        "Зелёный - без партий · синий - завершена · фиолетовый - запущена · оранжевый - ожидает запуска"), dialog);
    legend->setWordWrap(true);
    layout->addWidget(legend);

    const auto date = std::make_shared<QDate>(QDate::currentDate());
    const auto update_period = [=] {
        const bool weekly = week_button->isChecked();
        const auto label = QStringLiteral("dd.MM.yyyy");
        const QDate first = first_day(*date, weekly);
        period->setText(weekly
            ? QStringLiteral("%1 - %2").arg(first.toString(label), first.addDays(kDaysPerWeek - 1).toString(label))
            : date->toString(label));
        const auto previous_name = weekly ? QStringLiteral("Предыдущая неделя") : QStringLiteral("Предыдущий день");
        const auto next_name = weekly ? QStringLiteral("Следующая неделя") : QStringLiteral("Следующий день");
        previous->setAccessibleName(previous_name);
        previous->setToolTip(previous_name);
        next->setAccessibleName(next_name);
        next->setToolTip(next_name);
        day_hint->setVisible(weekly);
        view->set_period(*date, weekly);
    };
    const auto navigate = [week_button, date, update_period](int direction) {
        *date = date->addDays(direction * (week_button->isChecked() ? kDaysPerWeek : 1));
        update_period();
    };
    connect(modes, &QButtonGroup::buttonClicked, dialog, update_period);
    connect(previous, &QToolButton::clicked, dialog, [navigate] { navigate(-1); });
    connect(next, &QToolButton::clicked, dialog, [navigate] { navigate(1); });
    connect(today, &QToolButton::clicked, dialog, [date, update_period] {
        *date = QDate::currentDate();
        update_period();
    });
    view->on_day_selected([day_button, date, update_period](QDate selected) {
        *date = selected;
        day_button->setChecked(true);
        update_period();
    });
    update_period();
    auto* table = new TableWidget(make_headers(kHistoryHeaders), dialog);
    table->set_empty_text(QStringLiteral("У этой машины пока нет партий."));
    const auto refresh_history = [this, id, dialog, view, table] {
        const auto* machine = shop_.machines().find_by_id(id);
        if (!machine) {
            dialog->close();
            return;
        }
        dialog->setWindowTitle(QStringLiteral("Занятость - ") + qt_convert::text(machine->name()));
        view->refresh();
        const auto selected = selected_id(table, HistoryBatch);
        std::vector<const Batch*> history;
        for (const Batch& batch : shop_.batches()) {
            if (batch.machine_id() == id) history.push_back(&batch);
        }
        std::stable_sort(history.begin(), history.end(), [this](const Batch* left, const Batch* right) {
            return forecast_.batches.at(left->id()).start < forecast_.batches.at(right->id()).start;
        });
        table->reset_rows(static_cast<int>(history.size()));
        int row = 0;
        for (const Batch* batch : history) {
            const auto& projection = forecast_.batches.at(batch->id());
            set_cell(table, row, HistoryBatch, batch->id());
            set_sortable_cell(table, row, HistoryStart, projected_time(qt_convert::format_time(projection.start),
                             projection.lower_bound), time_sort_value(projection.start));
            const auto end = batch->completed_at() ? batch->expected_ready_at() : projection.end;
            set_sortable_cell(table, row, HistoryReady, projected_time(qt_convert::format_time(end),
                             projection.lower_bound), time_sort_value(end));
            set_cell(table, row, HistoryStatus, qt_convert::text(display_name(projection.status)));
            set_time_cell(table, row, HistoryCompleted, batch->completed_at());
            table->item(row, HistoryStart)->setToolTip(batch_tooltip(*batch, projection));
            ++row;
        }
        table->finish_rows();
        restore_selected_id(table, HistoryBatch, selected);
    };
    connect(this, &MachinesTab::forecast_changed, dialog, refresh_history);
    refresh_history();
    layout->addWidget(table);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрыть"));
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog->show();
}

void MachinesTab::fill(TableWidget* table) {
    table->reset_rows(static_cast<int>(shop_.machines().size()));
    int row = 0;
    std::vector<Machine> machines(shop_.machines().begin(), shop_.machines().end());
    std::stable_sort(machines.begin(), machines.end(), [this](const Machine& left, const Machine& right) {
        const auto left_available = forecast_.machines.at(left.id()).available_at;
        const auto right_available = forecast_.machines.at(right.id()).available_at;
        if (left_available != right_available) return left_available < right_available;
        return QString::localeAwareCompare(qt_convert::text(left.name()), qt_convert::text(right.name())) < 0;
    });
    for (const Machine& machine : machines) {
        set_cell(table, row, MachineId, machine.id());
        set_cell(table, row, MachineName, qt_convert::text(machine.name()));
        set_cell(table, row, MachineType, qt_convert::text(display_name(machine.type())));
        set_sortable_cell(table, row, MachineCapacity, QString::number(machine.capacity_kg(), 'f', kCapacityDecimals),
                          machine.capacity_kg(), true);
        const auto& projection = forecast_.machines.at(machine.id());
        const QString availability = projection.awaiting_confirmation ? QStringLiteral("Ожидает подтверждения")
            : projection.reserved ? projected_time(qt_convert::format_time(projection.available_at), true)
                                  : QStringLiteral("Свободна");
        set_sortable_cell(table, row, MachineFreeAt, availability,
                          time_sort_value(projection.reserved ? std::optional(projection.available_at) : std::nullopt));
        if (projection.awaiting_confirmation) {
            table->item(row, MachineFreeAt)->setToolTip(QStringLiteral("Занята до подтверждения завершения партии."));
        }
        set_sortable_cell(table, row, MachineMaintenance,
                          QString::number(machine.maintenance_remaining(), 'f', kMaintenanceDecimals),
                          machine.maintenance_remaining());
        table->item(row, MachineMaintenance)->setToolTip(QStringLiteral("%1 ч с учётом плана")
                                                           .arg(table->item(row, MachineMaintenance)->text()));
        ++row;
    }
}

void MachinesTab::add_record() {
    MachineDialog dialog(
        shop_,         QStringLiteral("Новая машина"), Machine(),
        [this](const Machine& machine) { shop_.add_machine(machine, employee_id_); }, this);
    show_dialog(dialog);
}

void MachinesTab::edit_record(Id id) {
    MachineDialog dialog(
        shop_,         QStringLiteral("Изменение машины"), shop_.machines().get(id),
        [this, id](const Machine& machine) {
            Machine updated = machine;
            updated.set_id(id);
            shop_.update_machine(updated, employee_id_);
        },
        this);
    show_dialog(dialog);
}

void MachinesTab::remove_record(Id id) { shop_.remove_machine(id, employee_id_); }
