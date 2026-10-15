// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/AutoMessageButton.hpp"

#include "controllers/automessages/AutoMessages.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace chatterino {

AutoMessageButton::AutoMessageButton(BaseWidget *parent)
    : Button(parent)
{
    this->refreshTooltip();

    QObject::connect(this, &Button::leftClicked, this, [this] {
        AutoMessages::setOn(this->channel_, !AutoMessages::isOn(this->channel_));
    });

    getSettings()->autoMessagesChannels.connect(
        [this](const QString &, auto) {
            this->refreshTooltip();
            this->update();
        },
        this->connections_, false);
}

void AutoMessageButton::setChannel(const QString &channel)
{
    if (channel == this->channel_)
    {
        return;
    }
    this->channel_ = channel;
    this->refreshTooltip();
    this->update();
}

void AutoMessageButton::refreshTooltip()
{
    const auto where = this->channel_.isEmpty()
                           ? QStringLiteral("diesem Kanal")
                           : QStringLiteral("#%1").arg(this->channel_);
    this->setToolTip(
        AutoMessages::isOn(this->channel_)
            ? QStringLiteral("Autonachrichten in %1 sind an - dein Account "
                             "antwortet nach den Regeln unter Erweitert → "
                             "Autonachrichten. Klicken zum Ausschalten.")
                  .arg(where)
            : QStringLiteral("Autonachrichten in %1 sind aus. Klicken zum "
                             "Einschalten - die Regeln stehen unter "
                             "Erweitert → Autonachrichten.")
                  .arg(where));
}

void AutoMessageButton::paintContent(QPainter &painter)
{
    const bool on = AutoMessages::isOn(this->channel_);

    // Quiet like the icons next to it while off; green while it answers
    // here, so a channel left on does not go unnoticed
    QColor color =
        getTheme()->isLightTheme() ? QColor("#333333") : QColor("#e6e6e6");
    if (on)
    {
        color = QColor("#3ecf8e");
    }
    if (!this->mouseOver())
    {
        color.setAlpha(on ? 235 : 200);
    }

    const auto scale = this->scale();
    QPen pen(color);
    pen.setWidthF(1.4 * scale);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
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

    // A speech bubble with its tail at the bottom left
    QPainterPath bubble;
    const QRectF body(x(0.1), y(0.14), box.width() * 0.8, box.height() * 0.56);
    bubble.addRoundedRect(body, box.width() * 0.16, box.width() * 0.16);
    QPainterPath tail;
    tail.moveTo(x(0.28), y(0.68));
    tail.lineTo(x(0.22), y(0.88));
    tail.lineTo(x(0.46), y(0.68));
    painter.setBrush(on ? QBrush(color) : Qt::NoBrush);
    painter.drawPath(bubble.united(tail));

    // Three dots, the typing kind - cut out of the filled bubble when on
    painter.setBrush(on ? QBrush(getTheme()->isLightTheme() ? Qt::white
                                                             : QColor("#1a1a1a"))
                        : QBrush(color));
    painter.setPen(Qt::NoPen);
    const auto dot = box.width() * 0.06;
    for (const auto part : {0.33, 0.5, 0.67})
    {
        painter.drawEllipse(QPointF(x(part), y(0.42)), dot, dot);
    }

    if (!on)
    {
        // Struck through, like the bell beside it
        painter.setPen(pen);
        painter.drawLine(QPointF(x(0.16), y(0.86)), QPointF(x(0.84), y(0.1)));
    }
}

}  // namespace chatterino
