// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/FocusButton.hpp"

#include "singletons/Theme.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/Window.hpp"

#include <QPainter>

#include <algorithm>

namespace chatterino {

namespace {

SplitNotebook *notebookOf(const QWidget *widget)
{
    auto *window = dynamic_cast<Window *>(widget->window());
    return window == nullptr ? nullptr : &window->getNotebook();
}

}  // namespace

FocusButton::FocusButton(BaseWidget *parent)
    : Button(parent)
{
    this->refresh();

    QObject::connect(this, &Button::leftClicked, this, [this] {
        if (auto *notebook = notebookOf(this))
        {
            notebook->toggleFocusMode();
        }
    });
}

bool FocusButton::active() const
{
    const auto *notebook = notebookOf(this);
    return notebook != nullptr && notebook->isFocusMode();
}

void FocusButton::refresh()
{
    this->setToolTip(this->active()
                         ? QStringLiteral("Fokus beenden - alle Tabs, Knöpfe "
                                          "und Split-Köpfe in diesem Fenster "
                                          "zeigen")
                         : QStringLiteral("Fokus-Ansicht für dieses Fenster - "
                                          "nur die Chats und die Tab-Gruppen, "
                                          "die immer angezeigt werden"));
    this->update();
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
    if (!this->active())
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

}  // namespace chatterino
