#include "gui/planning_tab.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <utility>

#include "display.h"
#include "gui/helpers.h"
#include "gui/qt_convert.h"
#include "gui/table_utils.h"

namespace {

constexpr int kCardBodyIndent = 12;
constexpr int kCardBodyTopMargin = 2;
constexpr int kDetailsMaxHeight = 220;

enum ConflictColumn { ConflictFirst, ConflictSecond, ConflictReasons };
constexpr std::array kConflictHeaders{"Первое изделие", "Второе изделие", "Причины"};

enum ForecastColumn { ForecastOrder, ForecastDeadline, ForecastReady, ForecastVerdict };
constexpr std::array kForecastHeaders{"Заказ №", "Срок", "Прогноз готовности", "Оценка"};

QLabel* add_text(QVBoxLayout* layout, const QString& text) {
    auto* label = new QLabel(text, layout->parentWidget());
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    layout->addWidget(label);
    return label;
}

QScrollArea* make_card_panel(QVBoxLayout*& cards, QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* contents = new QWidget(scroll);
    cards = new QVBoxLayout(contents);
    cards->setContentsMargins(0, 0, 0, 0);
    cards->addStretch();
    scroll->setWidget(contents);
    return scroll;
}

void clear_cards(QVBoxLayout* panel) {
    while (panel->count() > 1) {
        auto* entry = panel->takeAt(0);
        delete entry->widget();
        delete entry;
    }
}

void show_empty(QVBoxLayout* panel, const QString& text) {
    auto* label = new QLabel(text, panel->parentWidget());
    label->setWordWrap(true);
    panel->insertWidget(0, label);
}

void connect_disclosure(QToolButton* toggle, QWidget* contents) {
    toggle->setCheckable(true);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setArrowType(Qt::RightArrow);
    toggle->setFocusPolicy(Qt::StrongFocus);
    contents->hide();
    QObject::connect(toggle, &QToolButton::toggled, contents, [toggle, contents](bool shown) {
        toggle->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
        contents->setVisible(shown);
    });
}

QVBoxLayout* add_card(QVBoxLayout* panel, const QString& title, const QString& summary, bool expanded = false) {
    auto* frame = new QFrame(panel->parentWidget());
    frame->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(frame);
    auto* toggle = new QToolButton(frame);
    toggle->setText(title);
    auto font = toggle->font();
    font.setBold(true);
    toggle->setFont(font);
    layout->addWidget(toggle, 0, Qt::AlignLeft);
    add_text(layout, summary);
    auto* contents = new QWidget(frame);
    auto* body = new QVBoxLayout(contents);
    body->setContentsMargins(kCardBodyIndent, kCardBodyTopMargin, 0, 0);
    layout->addWidget(contents);
    connect_disclosure(toggle, contents);
    toggle->setChecked(expanded);
    panel->insertWidget(panel->count() - 1, frame);
    return body;
}

QString item_caption(const Item& item) {
    return QStringLiteral("%1 · %2").arg(qt_convert::text(item.kind()), qt_convert::text(item.tag()));
}

QString short_time(TimePoint time) {
    return QDateTime::fromMSecsSinceEpoch(to_epoch_ms(time)).toString(QStringLiteral("dd.MM HH:mm"));
}

}

PlanningTab::PlanningTab(DryCleaningShop& shop, Id employee_id, QWidget* parent)
    : QWidget(parent), shop_(shop), employee_id_(employee_id) {
    build_button_ = new QPushButton(QStringLiteral("Построить план"), this);
    confirm_button_ = new QPushButton(QStringLiteral("Подтвердить план"), this);
    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(build_button_);
    toolbar->addWidget(confirm_button_);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    action_hint_ = add_text(layout, {});
    summary_label_ = add_text(layout, {});

    auto* panels = new QSplitter(Qt::Horizontal, this);
    panels->setChildrenCollapsible(false);
    auto* queue = make_card_panel(queue_cards_, this);
    auto* batches = make_card_panel(batch_cards_, this);
    panels->addWidget(make_titled_box(QStringLiteral("Ожидают планирования"), queue, panels));
    panels->addWidget(make_titled_box(QStringLiteral("Партии плана"), batches, panels));
    panels->setStretchFactor(0, kStretch);
    panels->setStretchFactor(1, kStretch);
    layout->addWidget(panels, kStretch);

    details_button_ = new QToolButton(this);
    details_button_->setText(QStringLiteral("Подробности плана"));
    details_button_->setAccessibleDescription(QStringLiteral("Конфликты изделий и прогнозы по заказам"));
    auto* details = new QTabWidget(this);
    details_ = details;
    details->setMaximumHeight(kDetailsMaxHeight);
    conflicts_table_ = new TableWidget(make_headers(kConflictHeaders), details);
    conflicts_table_->set_empty_text(QStringLiteral("Конфликтов между изделиями нет."));
    forecasts_table_ = new TableWidget(make_headers(kForecastHeaders), details);
    forecasts_table_->set_empty_text(QStringLiteral("Нет заказов для расчёта прогноза."));
    details->addTab(conflicts_table_, QStringLiteral("Конфликты изделий"));
    details->addTab(forecasts_table_, QStringLiteral("Прогнозы по заказам"));
    layout->addWidget(details_button_, 0, Qt::AlignLeft);
    layout->addWidget(details_);
    connect_disclosure(details_button_, details_);

    connect(build_button_, &QPushButton::clicked, this, &PlanningTab::build);
    connect(confirm_button_, &QPushButton::clicked, this, &PlanningTab::confirm);
    refresh();
}

void PlanningTab::refresh() {
    const auto now = qt_convert::now();
    const QString stale = plan_ ? qt_convert::text(shop_.plan_freshness_block(*plan_, now)) : QString();
    const Plan* preview = plan_ && stale.isEmpty() ? &*plan_ : nullptr;
    if (rendered_revision_ != shop_.revision() || rendered_preview_ != (preview != nullptr)) {
        const auto candidates = shop_.collect_plan_items(now);
        candidate_count_ = candidates.size();
        show_queue(candidates, preview);
        show_batches(preview);
        show_details(preview);
        rendered_revision_ = shop_.revision();
        rendered_preview_ = preview != nullptr;
    }
    update_controls(preview, stale);
}

void PlanningTab::update_controls(const Plan* preview, const QString& stale) {
    const bool may_build = has_permission(shop_, employee_id_, Permission::BuildPlan);
    const bool may_confirm = has_permission(shop_, employee_id_, Permission::ConfirmPlan);
    const QString build_hint = may_build ? (candidate_count_ ? QString() : QStringLiteral("Нет изделий для планирования."))
                                         : QStringLiteral("Для построения плана нужны права оператора.");
    build_button_->setEnabled(build_hint.isEmpty());
    build_button_->setToolTip(build_hint);
    build_button_->setAccessibleDescription(build_hint);
    confirm_button_->setEnabled(preview && !preview->batches.empty() && may_confirm);
    QString confirm_hint;
    if (!may_confirm) confirm_hint = QStringLiteral("Для подтверждения плана нужны права оператора.");
    else if (!stale.isEmpty()) confirm_hint = stale;
    else if (!preview) confirm_hint = QStringLiteral("Сначала постройте план.");
    else if (preview->batches.empty()) confirm_hint = QStringLiteral("В плане нет партий для подтверждения.");
    confirm_button_->setToolTip(confirm_hint);
    confirm_button_->setAccessibleDescription(confirm_hint);
    QStringList hints;
    if (!build_hint.isEmpty()) hints << build_hint;
    if (!confirm_hint.isEmpty() && (stale.isEmpty() || confirm_hint != stale)) hints << confirm_hint;
    action_hint_->setText(hints.join(QStringLiteral(" ")));
    action_hint_->setVisible(!hints.isEmpty());

    if (!stale.isEmpty()) {
        summary_label_->setText(stale);
    } else if (preview) {
        summary_label_->setText(QStringLiteral("Предварительный план · Распределено %1 из %2 · Партий: %3 · Не размещено: %4")
            .arg(candidate_count_ - preview->unplaced.size()).arg(candidate_count_)
            .arg(preview->batches.size()).arg(preview->unplaced.size()));
    } else if (confirmed_revision_ == shop_.revision()) {
        summary_label_->setText(QStringLiteral("План подтверждён · Создано партий: %1 · Ожидают планирования: %2")
            .arg(confirmed_batches_).arg(candidate_count_));
    } else {
        summary_label_->setText(QStringLiteral("Ожидают планирования: %1").arg(candidate_count_));
    }
}

void PlanningTab::show_queue(const std::vector<PlanItem>& candidates, const Plan* preview) {
    clear_cards(queue_cards_);
    std::map<Id, std::string> unplaced;
    if (preview) unplaced.insert(preview->unplaced.begin(), preview->unplaced.end());
    else if (confirmed_revision_ == shop_.revision()) unplaced = confirmed_unplaced_;

    std::map<std::pair<TimePoint, Id>, std::vector<const PlanItem*>> orders;
    for (const PlanItem& item : candidates) orders[{item.deadline, item.order_id}].push_back(&item);
    for (const auto& [key, members] : orders) {
        const auto& [deadline, order_id] = key;
        const auto waiting = preview ? static_cast<std::size_t>(std::count_if(members.begin(), members.end(),
            [&](const PlanItem* item) { return unplaced.contains(item->item_id); })) : members.size();
        if (waiting == 0) continue;

        const auto count = preview ? QStringLiteral("Размещено %1 из %2").arg(members.size() - waiting).arg(members.size())
                                   : QStringLiteral("Ожидают: %1").arg(waiting);
        auto* body = add_card(queue_cards_, QStringLiteral("Заказ №%1").arg(order_id),
                             QStringLiteral("Срок %1 · %2").arg(qt_convert::format_time(deadline), count),
                             preview || !unplaced.empty());
        for (const PlanItem* candidate : members) {
            const auto reason = unplaced.find(candidate->item_id);
            if (preview && reason == unplaced.end()) continue;
            const Item& item = shop_.items().get(candidate->item_id);
            QString text = QStringLiteral("%1\nЭтап %2 из %3 · %4")
                .arg(item_caption(item)).arg(item.completed_batches().size() + 1).arg(item.services().size())
                .arg(qt_convert::text(display_name(candidate->service_type)));
            if (reason != unplaced.end()) {
                text += preview ? QStringLiteral("\nПричина: ") : QStringLiteral("\nПо последнему плану: ");
                text += qt_convert::text(reason->second);
            }
            add_text(body, text)->setToolTip(text);
        }
    }
    if (queue_cards_->count() == 1) {
        show_empty(queue_cards_, preview ? QStringLiteral("Все изделия распределены. Подтвердите план.")
                                         : QStringLiteral("Нет изделий, ожидающих планирования."));
    }
}

void PlanningTab::show_batches(const Plan* preview) {
    clear_cards(batch_cards_);
    if (!preview) {
        const auto hint = confirmed_revision_ == shop_.revision()
            ? QStringLiteral("Подтверждённые партии доступны на вкладке «Партии».")
            : candidate_count_ == 0 ? QStringLiteral("Нет изделий для нового плана.")
                                    : QStringLiteral("Постройте план, чтобы увидеть партии.");
        show_empty(batch_cards_, hint);
        return;
    }
    if (preview->batches.empty()) {
        show_empty(batch_cards_, QStringLiteral("Партии не созданы. Причины указаны у изделий слева."));
        return;
    }
    int number = 0;
    for (const PlannedBatch& batch : preview->batches) {
        const auto service = qt_convert::text(display_name(batch.service_type));
        const auto summary = QStringLiteral("%1 · %2\n%3 - %4 · Загрузка %5")
            .arg(machine_name(shop_, batch.machine_id), service,
                 short_time(batch.planned_start_at), short_time(batch.planned_ready_at),
                 qt_convert::text(format_percent(batch.fill_ratio)));
        auto* body = add_card(batch_cards_, QStringLiteral("Партия %1").arg(++number), summary);
        for (Id id : batch.item_ids) {
            const Item& item = shop_.items().get(id);
            const auto text = QStringLiteral("%1 · заказ №%2").arg(item_caption(item)).arg(item.order_id());
            add_text(body, text)->setToolTip(text);
        }
    }
}

void PlanningTab::show_details(const Plan* preview) {
    details_button_->setVisible(preview != nullptr);
    if (!preview) details_button_->setChecked(false);
    conflicts_table_->reset_rows(0);
    forecasts_table_->reset_rows(preview ? static_cast<int>(preview->order_ready_at.size()) : 0);
    if (!preview) return;
    int row = 0;
    for (const auto& [first, neighbours] : preview->conflicts) {
        for (const auto& [second, reasons] : neighbours) {
            if (first >= second) continue;
            QStringList texts;
            for (const std::string& reason : reasons) texts << qt_convert::text(reason);
            conflicts_table_->insertRow(row);
            set_cell(conflicts_table_, row, ConflictFirst, item_tag(shop_, first));
            set_cell(conflicts_table_, row, ConflictSecond, item_tag(shop_, second));
            set_cell(conflicts_table_, row, ConflictReasons, texts.join(QStringLiteral(" · ")));
            ++row;
        }
    }
    conflicts_table_->finish_rows();
    row = 0;
    for (const auto& [order_id, ready_at] : preview->order_ready_at) {
        const auto deadline = shop_.orders().get(order_id).deadline();
        const auto verdict = !ready_at ? QStringLiteral("Нет прогноза")
            : *ready_at > deadline ? QStringLiteral("Опоздание") : QStringLiteral("В срок");
        set_cell(forecasts_table_, row, ForecastOrder, order_id);
        set_time_cell(forecasts_table_, row, ForecastDeadline, deadline);
        set_time_cell(forecasts_table_, row, ForecastReady, ready_at);
        set_cell(forecasts_table_, row, ForecastVerdict, verdict);
        ++row;
    }
    forecasts_table_->finish_rows();
}

void PlanningTab::build() {
    if (run_guarded(this, [&] { plan_ = shop_.build_plan(qt_convert::now(), employee_id_); })) {
        rendered_revision_.reset();
    }
    refresh();
}

void PlanningTab::confirm() {
    if (!plan_) {
        show_action_refusal(this, QStringLiteral("Сначала постройте план."));
        return;
    }
    std::map<Id, std::string> unplaced(plan_->unplaced.begin(), plan_->unplaced.end());
    std::vector<Id> created;
    if (!run_guarded(this, [&] { created = shop_.confirm_plan(*plan_, employee_id_, qt_convert::now()); })) {
        refresh();
        return;
    }
    plan_.reset();
    confirmed_revision_ = shop_.revision();
    confirmed_batches_ = created.size();
    confirmed_unplaced_ = std::move(unplaced);
    refresh();
    emit changed();
    QMessageBox::information(this, QStringLiteral("План подтверждён"),
                             QStringLiteral("Создано партий: %1. Они доступны на вкладке «Партии».").arg(created.size()));
}
