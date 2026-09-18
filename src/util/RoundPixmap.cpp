// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/RoundPixmap.hpp"

#include <QPainter>
#include <QRectF>

namespace chatterino {

void paintRound(QPainter &painter, const QRectF &rect, const QPixmap &pixmap)
{
    const auto ratio =
        painter.device() != nullptr ? painter.device()->devicePixelRatioF() : 1.0;
    auto scaled =
        pixmap.scaled((rect.size() * ratio).toSize(), Qt::IgnoreAspectRatio,
                      Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(ratio);

    QBrush brush(scaled);
    brush.setTransform(QTransform::fromTranslate(rect.x(), rect.y()));

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(brush);
    painter.drawEllipse(rect);
    painter.restore();
}

QPixmap roundPixmap(const QPixmap &pixmap, int side)
{
    QPixmap round(side, side);
    round.fill(Qt::transparent);
    QPainter painter(&round);
    paintRound(painter, QRectF(0, 0, side, side), pixmap);
    return round;
}

}  // namespace chatterino
