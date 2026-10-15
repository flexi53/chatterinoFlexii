// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/AlertMuteButton.hpp"

#include "controllers/moderation/AlertMute.hpp"
#include "singletons/Settings.hpp"
#include "widgets/helper/ActiveBorder.hpp"
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
        alertmute::setMuted(this->channel_, !this->muted());
    });

    getSettings()->modAlertMutedChannels.connect(
        [this](const QString &, auto) {
            this->refreshTooltip();
            this->update();
        },
        this->connections_, false);
}

void AlertMuteButton::setChannel(const QString &channel)
{
    if (channel == this->channel_)
    {
        return;
    }
    this->channel_ = channel;
    this->refreshTooltip();
    this->update();
}

bool AlertMuteButton::muted() const
{
    return alertmute::isMuted(this->channel_);
}

void AlertMuteButton::refreshTooltip()
{
    const auto where = this->channel_.isEmpty()
                           ? QStringLiteral("diesem Kanal")
                           : QStringLiteral("#%1").arg(this->channel_);
    this->setToolTip(
        this->muted()
            ? QStringLiteral("Alarm-Fenster für %1 sind aus - es geht keines "
                             "mehr auf. Klicken, um sie wieder zu erlauben.")
                  .arg(where)
            : QStringLiteral("Alarm-Fenster für %1 ausschalten - der "
                             "Mod-Assistent passt weiter auf, aber nichts "
                             "ploppt mehr auf. Andere Kanäle bleiben, wie "
                             "sie sind.")
                  .arg(where));
}

void AlertMuteButton::paintContent(QPainter &painter)
{
    const bool muted = this->muted();

    // Green while the alerts may come, red while they are held back - the
    // same two colours as the speech bubble beside it
    QColor color = muted ? activeborder::fallback() : QColor("#3ecf8e");
    if (!this->mouseOver())
    {
        color.setAlpha(235);
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

    paintBell(painter, box, muted);
}

void paintBell(QPainter &painter, const QRectF &box, bool struck)
{
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

    if (!struck)
    {
        return;
    }

    // Struck through, so it is clear at a glance that nothing will open -
    // short enough that the bell is still a bell
    painter.drawLine(QPointF(x(0.22), y(0.8)), QPointF(x(0.78), y(0.16)));
}

}  // namespace chatterino
