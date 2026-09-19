// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/FocusButton.hpp"

#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"

#include <QPainter>

#include <algorithm>

namespace chatterino {

FocusButton::FocusButton(BaseWidget *parent)
    : Button(parent)
{
    getSettings()->focusMode.connect(
        [this](const bool &, auto) {
            this->updateTooltip();
            this->update();
        },
        this->connections_);

    QObject::connect(this, &Button::leftClicked, this, [] {
        getSettings()->focusMode.setValue(!getSettings()->focusMode);
    });
}

void FocusButton::paintContent(QPainter &painter)
{
    // The colour and weight of the icons next to it
    QColor color =
        getTheme()->isLightTheme() ? QColor("#333333") : QColor("#e6e6e6");
    if (!this->mouseOver())
    {
        color.setAlpha(200);
    }

    QPen pen(color);
    pen.setWidthF(1.4 * this->scale());
    pen.setCapStyle(Qt::FlatCap);
    painter.setPen(pen);
    painter.setRenderHint(QPainter::Antialiasing);

    // As big as the icons next to it, inside the same padding
    const qreal side = std::min(this->width() - (12 * this->scale()),
                                this->height() - (6 * this->scale()));
    QRectF box(0, 0, side, side);
    box.moveCenter(QRectF(this->rect()).center());
    const qreal arm = side * 0.38;

    const auto corner = [&](QPointF at, qreal dx, qreal dy) {
        painter.drawLine(at, at + QPointF(dx, 0));
        painter.drawLine(at, at + QPointF(0, dy));
    };
    if (!getSettings()->focusMode)
    {
        corner(box.topLeft(), arm, arm);
        corner(box.topRight(), -arm, arm);
        corner(box.bottomLeft(), arm, -arm);
        corner(box.bottomRight(), -arm, -arm);
    }
    else
    {
        corner(box.topLeft() + QPointF(arm, arm), -arm, -arm);
        corner(box.topRight() + QPointF(-arm, arm), arm, -arm);
        corner(box.bottomLeft() + QPointF(arm, -arm), -arm, arm);
        corner(box.bottomRight() + QPointF(-arm, -arm), arm, arm);
    }
}

void FocusButton::updateTooltip()
{
    this->setToolTip(getSettings()->focusMode
                         ? QStringLiteral("Fokus beenden - alle Tabs, Knöpfe "
                                          "und Split-Köpfe zeigen")
                         : QStringLiteral("Fokus-Ansicht - nur die Chats und "
                                          "die Tab-Gruppen, die immer "
                                          "angezeigt werden"));
}

}  // namespace chatterino
