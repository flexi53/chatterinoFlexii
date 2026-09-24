// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/FollowBrowserButton.hpp"

#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "widgets/helper/ActiveBorder.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace chatterino {

FollowBrowserButton::FollowBrowserButton(BaseWidget *parent)
    : Button(parent)
{
    this->refreshTooltip();

    QObject::connect(this, &Button::leftClicked, this, [] {
        auto &setting = getSettings()->tabFollowsBrowser;
        setting.setValue(!setting.getValue());
    });

    // It holds for the whole program, so every button of its kind follows
    // the same switch
    getSettings()->tabFollowsBrowser.connect(
        [this](const bool, auto) {
            this->refreshTooltip();
            this->update();
        },
        this->connections_, false);
}

bool FollowBrowserButton::following()
{
    return getSettings()->tabFollowsBrowser;
}

void FollowBrowserButton::refreshTooltip()
{
    this->setToolTip(
        following()
            ? QStringLiteral(
                  "Der Tab folgt dem Streamer, den du im Browser anschaust. "
                  "Klicken, um ihn stehen zu lassen. Gilt überall, nicht nur "
                  "hier.")
            : QStringLiteral(
                  "Der Tab bleibt, wo er ist. Klicken, damit er dem "
                  "Streamer folgt, den du im Browser anschaust. Gilt "
                  "überall, nicht nur hier."));
}

void FollowBrowserButton::paintContent(QPainter &painter)
{
    const bool on = following();

    // Quiet like the icons next to it while it is off; in the active colour
    // while the tab is being moved about, so it is clear who did it
    QColor color =
        getTheme()->isLightTheme() ? QColor("#333333") : QColor("#e6e6e6");
    if (on)
    {
        // Blue for "switched on" - the red of the bell says something is
        // being held back, which is not the case here
        color = QColor("#3f9dff");
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
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing);

    // As big as the icons next to it, inside the same padding
    const qreal side =
        std::min(this->width() - (8 * scale), this->height() - (4 * scale));
    QRectF box(0, 0, side, side);
    box.moveCenter(QRectF(this->rect()).center());

    paintChain(painter, box, !on);
}

void paintChain(QPainter &painter, const QRectF &box, bool broken)
{
    // Two links along the diagonal, each an outlined capsule: closed they
    // hook into one another, open a gap stands between them
    painter.save();
    painter.translate(box.center());
    painter.rotate(-45);

    const qreal linkWidth = box.width() * 0.52;
    const qreal linkHeight = box.height() * 0.60;
    const qreal radius = linkWidth / 2;
    // Closed they overlap by a third of a link, open they part
    const qreal shift =
        broken ? linkHeight * 0.58 : linkHeight * 0.34;

    const QRectF link(-linkWidth / 2, -linkHeight / 2, linkWidth, linkHeight);
    painter.drawRoundedRect(link.translated(0, -shift), radius, radius);
    painter.drawRoundedRect(link.translated(0, shift), radius, radius);

    painter.restore();
}

}  // namespace chatterino
