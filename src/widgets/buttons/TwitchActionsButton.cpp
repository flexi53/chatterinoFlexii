// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/TwitchActionsButton.hpp"

#include "singletons/Theme.hpp"

#include <QPainter>

#include <algorithm>

namespace chatterino {

TwitchActionsButton::TwitchActionsButton(BaseWidget *parent)
    : Button(parent)
{
    this->setToolTip(QStringLiteral(
        "Kanalpunkte dieses Kanals einlösen - und als Mod Umfragen und "
        "Vorhersagen starten. Mitmachen geht im Banner über dem Chat."));
}

void TwitchActionsButton::paintContent(QPainter &painter)
{
    QColor color = getTheme()->messages.textColors.regular;
    color.setAlpha(this->mouseOver() ? 255 : 200);

    const auto scale = this->scale();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    // As big as the icons next to it, inside the same padding
    const qreal side =
        std::min(this->width() - (12 * scale), this->height() - (6 * scale));
    QRectF box(0, 0, side, side);
    box.moveCenter(QRectF(this->rect()).center());

    // Three bars of different length, lying down like the ones on twitch.tv
    const auto bar = box.height() * 0.2;
    const auto gap = (box.height() - (3 * bar)) / 2;
    const auto radius = bar / 2;
    int i = 0;
    for (const auto length : {0.95, 0.6, 0.8})
    {
        painter.drawRoundedRect(
            QRectF(box.left(), box.top() + (i * (bar + gap)),
                   box.width() * length, bar),
            radius, radius);
        i++;
    }
}

}  // namespace chatterino
