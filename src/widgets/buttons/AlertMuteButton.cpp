// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/AlertMuteButton.hpp"

#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace chatterino {

AlertMuteButton::AlertMuteButton(BaseWidget *parent)
    : Button(parent)
{
    this->refreshTooltip();

    QObject::connect(this, &Button::leftClicked, this, [this] {
        getSettings()->modAlertMuted.setValue(!getSettings()->modAlertMuted);
    });

    getSettings()->modAlertMuted.connect(
        [this](const bool, auto) {
            this->refreshTooltip();
            this->update();
        },
        this->connections_, false);
}

void AlertMuteButton::refreshTooltip()
{
    this->setToolTip(
        getSettings()->modAlertMuted
            ? QStringLiteral("Alarm-Fenster sind aus - es geht keines mehr "
                             "auf. Klicken, um sie wieder zu erlauben.")
            : QStringLiteral("Alarm-Fenster ausschalten - der Mod-Assistent "
                             "passt weiter auf, aber nichts ploppt mehr auf."));
}

void AlertMuteButton::paintContent(QPainter &painter)
{
    const bool muted = getSettings()->modAlertMuted;

    // The colour and weight of the icons next to it
    QColor color =
        getTheme()->isLightTheme() ? QColor("#333333") : QColor("#e6e6e6");
    if (!this->mouseOver())
    {
        color.setAlpha(200);
    }

    const auto scale = this->scale();
    QPen pen(color);
    pen.setWidthF(1.4 * scale);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing);

    // As big as the icons next to it, inside the same padding
    const qreal side =
        std::min(this->width() - (12 * scale), this->height() - (6 * scale));
    QRectF box(0, 0, side, side);
    box.moveCenter(QRectF(this->rect()).center());

    const auto x = [&](qreal part) {
        return box.left() + (box.width() * part);
    };
    const auto y = [&](qreal part) {
        return box.top() + (box.height() * part);
    };

    // The bell: a dome on a rim, with the clapper under it
    QPainterPath bell;
    bell.moveTo(x(0.18), y(0.7));
    bell.cubicTo(x(0.26), y(0.62), x(0.24), y(0.5), x(0.24), y(0.42));
    bell.cubicTo(x(0.24), y(0.2), x(0.38), y(0.12), x(0.5), y(0.12));
    bell.cubicTo(x(0.62), y(0.12), x(0.76), y(0.2), x(0.76), y(0.42));
    bell.cubicTo(x(0.76), y(0.5), x(0.74), y(0.62), x(0.82), y(0.7));
    bell.closeSubpath();
    painter.drawPath(bell);
    painter.drawLine(QPointF(x(0.42), y(0.82)), QPointF(x(0.58), y(0.82)));

    if (!muted)
    {
        return;
    }

    // Struck through, so it is clear at a glance that nothing will open -
    // short enough that the bell is still a bell
    painter.drawLine(QPointF(x(0.22), y(0.8)), QPointF(x(0.78), y(0.16)));
}

}  // namespace chatterino
