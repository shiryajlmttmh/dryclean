#include "gui/control_style.h"

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleOption>
#include <utility>

namespace {

constexpr int kDarkWindowLightness = 128;
constexpr qreal kDarkSurfaceShade = 0.38;
constexpr qreal kLightSurfaceShade = 0.23;
constexpr qreal kDisabledBorderStrength = 0.10;
constexpr qreal kDisabledIconShadowStrength = 0.48;
constexpr qreal kMaxChannel = 255.0;
constexpr qreal kDisabledTextStrength = 0.64;
constexpr qreal kFrameInset = 0.5;
constexpr qreal kFrameRadius = 4;

QColor blend(const QColor& background, const QColor& foreground, qreal strength) {
    return QColor::fromRgbF(
        background.redF() + (foreground.redF() - background.redF()) * strength,
        background.greenF() + (foreground.greenF() - background.greenF()) * strength,
        background.blueF() + (foreground.blueF() - background.blueF()) * strength);
}

QColor disabled_surface(const QPalette& palette, QPalette::ColorRole role) {
    const bool dark = palette.color(QPalette::Active, QPalette::Window).lightness() < kDarkWindowLightness;
    return blend(palette.color(QPalette::Active, role), dark ? Qt::black : Qt::gray,
                 dark ? kDarkSurfaceShade : kLightSurfaceShade);
}

QColor disabled_text(const QPalette& palette, QPalette::ColorRole role, const QColor& surface) {
    return blend(surface, palette.color(QPalette::Active, role), kDisabledTextStrength);
}

void apply_disabled_palette(QPalette& palette) {
    const auto button = disabled_surface(palette, QPalette::Button);
    const auto field = disabled_surface(palette, QPalette::Base);
    palette.setColor(QPalette::Disabled, QPalette::Button, button);
    palette.setColor(QPalette::Disabled, QPalette::Base, field);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled_text(palette, QPalette::ButtonText, button));
    palette.setColor(QPalette::Disabled, QPalette::Text, disabled_text(palette, QPalette::Text, field));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled_text(palette, QPalette::WindowText, field));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, palette.color(QPalette::Disabled, QPalette::Text));
}

void paint_frame(QPainter* painter, const QRect& rect, const QPalette& palette, QPalette::ColorRole role) {
    const auto surface = disabled_surface(palette, role);
    const auto border = blend(surface, palette.color(QPalette::Active, QPalette::WindowText), kDisabledBorderStrength);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(border);
    painter->setBrush(surface);
    painter->drawRoundedRect(QRectF(rect).adjusted(kFrameInset, kFrameInset, -kFrameInset, -kFrameInset),
                             kFrameRadius, kFrameRadius);
    painter->restore();
}

class ControlStyle final : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    void polish(QPalette& palette) override {
        QProxyStyle::polish(palette);
        apply_disabled_palette(palette);
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget) const override {
        if (option->state.testFlag(State_Enabled)) {
            QProxyStyle::drawControl(element, option, painter, widget);
        } else if (element == CE_PushButton) {
            auto label = *qstyleoption_cast<const QStyleOptionButton*>(option);
            apply_disabled_palette(label.palette);
            paint_frame(painter, label.rect, label.palette, QPalette::Button);
            label.rect = subElementRect(SE_PushButtonContents, &label, widget);
            QProxyStyle::drawControl(CE_PushButtonLabel, &label, painter, widget);
        } else if (element == CE_ComboBoxLabel) {
            auto label = *qstyleoption_cast<const QStyleOptionComboBox*>(option);
            apply_disabled_palette(label.palette);
            QProxyStyle::drawControl(element, &label, painter, widget);
        } else {
            QProxyStyle::drawControl(element, option, painter, widget);
        }
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget) const override {
        if (element == PE_PanelLineEdit && !option->state.testFlag(State_Enabled)) {
            const auto* field = qstyleoption_cast<const QStyleOptionFrame*>(option);
            if (field && field->lineWidth > 0) {
                paint_frame(painter, field->rect, field->palette, QPalette::Base);
                return;
            }
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter,
                            const QWidget* widget) const override {
        if (option->state.testFlag(State_Enabled)) {
            QProxyStyle::drawComplexControl(control, option, painter, widget);
            return;
        }

        switch (control) {
        case CC_ToolButton: {
            auto label = *qstyleoption_cast<const QStyleOptionToolButton*>(option);
            apply_disabled_palette(label.palette);
            paint_frame(painter, label.rect, label.palette, QPalette::Button);
            label.rect = subControlRect(control, option, SC_ToolButton, widget);
            QProxyStyle::drawControl(CE_ToolButtonLabel, &label, painter, widget);
            break;
        }
        case CC_ComboBox:
            paint_frame(painter, option->rect, option->palette, QPalette::Base);
            draw_arrow(PE_IndicatorArrowDown, control, SC_ComboBoxArrow, option, painter, widget);
            break;
        case CC_SpinBox:
            paint_frame(painter, option->rect, option->palette, QPalette::Base);
            draw_arrow(PE_IndicatorArrowUp, control, SC_SpinBoxUp, option, painter, widget);
            draw_arrow(PE_IndicatorArrowDown, control, SC_SpinBoxDown, option, painter, widget);
            break;
        default:
            QProxyStyle::drawComplexControl(control, option, painter, widget);
        }
    }

    QPixmap generatedIconPixmap(QIcon::Mode mode, const QPixmap& pixmap,
                                const QStyleOption* option) const override {
        if (mode != QIcon::Disabled || !option) return QProxyStyle::generatedIconPixmap(mode, pixmap, option);

        auto image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
        const auto surface = disabled_surface(option->palette, QPalette::Button);
        const auto ink = disabled_text(option->palette, QPalette::ButtonText, surface);
        const auto shadow = blend(surface, ink, kDisabledIconShadowStrength);
        const bool dark = surface.lightness() < kDarkWindowLightness;
        for (int row = 0; row < image.height(); ++row) {
            auto* pixels = reinterpret_cast<QRgb*>(image.scanLine(row));
            for (int column = 0; column < image.width(); ++column) {
                const auto pixel = pixels[column];
                const auto brightness = qGray(pixel) / kMaxChannel;
                const auto tone = blend(shadow, ink, dark ? brightness : 1.0 - brightness);
                pixels[column] = qRgba(tone.red(), tone.green(), tone.blue(), qAlpha(pixel));
            }
        }
        return QPixmap::fromImage(std::move(image));
    }

private:
    void draw_arrow(PrimitiveElement arrow, ComplexControl control, SubControl part, const QStyleOptionComplex* option,
                    QPainter* painter, const QWidget* widget) const {
        if (!option->subControls.testFlag(part)) return;
        QStyleOption glyph;
        glyph.rect = subControlRect(control, option, part, widget);
        glyph.state = option->state;
        glyph.palette = option->palette;
        apply_disabled_palette(glyph.palette);
        QProxyStyle::drawPrimitive(arrow, &glyph, painter, widget);
    }
};

}

void install_control_style(QApplication& app) {
    app.setStyle(new ControlStyle(app.style()));
}
