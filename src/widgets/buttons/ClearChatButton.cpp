// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/ClearChatButton.hpp"

#include "singletons/Theme.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace chatterino {

ClearChatButton::ClearChatButton(BaseWidget *parent)
    : Button(parent)
{
    this->setToolTip(QStringLiteral(
        "Chat leeren - nur hier bei dir, wie „Clear messages“. Für alle "
        "anderen bleibt alles, wie es ist."));
}

void ClearChatButton::paintContent(QPainter &painter)
{
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

    // The handle and the lid
    painter.drawLine(QPointF(x(0.38), y(0.08)), QPointF(x(0.62), y(0.08)));
    painter.drawLine(QPointF(x(0.1), y(0.22)), QPointF(x(0.9), y(0.22)));

    // The bin, narrowing towards the bottom
    QPainterPath bin;
    bin.moveTo(x(0.2), y(0.22));
    bin.lineTo(x(0.28), y(0.95));
    bin.lineTo(x(0.72), y(0.95));
    bin.lineTo(x(0.8), y(0.22));
    painter.drawPath(bin);

    // Its ribs
    painter.drawLine(QPointF(x(0.42), y(0.38)), QPointF(x(0.43), y(0.8)));
    painter.drawLine(QPointF(x(0.58), y(0.38)), QPointF(x(0.57), y(0.8)));
}

}  // namespace chatterino
