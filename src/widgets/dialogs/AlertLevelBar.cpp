// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/AlertLevelBar.hpp"

#include <QPainter>

#include <algorithm>

namespace chatterino {

namespace {

/// Where the mark sits, so what is past it has as much room again
constexpr double AT_THRESHOLD = 0.5;

}  // namespace

AlertLevelBar::AlertLevelBar(QWidget *parent)
    : QWidget(parent)
{
    this->setFixedHeight(4);
    this->hide();
}

void AlertLevelBar::show(int value, int threshold, const QColor &color)
{
    if (value <= 0 || threshold <= 0)
    {
        this->hide();
        return;
    }

    this->value_ = value;
    this->threshold_ = threshold;
    this->color_ = color;
    this->setToolTip(
        QStringLiteral("%1 von %2, ab denen der Alarm aufgeht")
            .arg(value)
            .arg(threshold));
    QWidget::show();
    this->update();
}

void AlertLevelBar::paintEvent(QPaintEvent * /*event*/)
{
    if (this->threshold_ <= 0)
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF all(this->rect());

    // The bed it runs on
    auto bed = this->color_;
    bed.setAlpha(45);
    painter.fillRect(all, bed);

    // Half the bar is what it takes to go off; anything past that is how
    // far over it went
    const auto share =
        std::min(1.0, (double(this->value_) / double(this->threshold_)) *
                          AT_THRESHOLD);
    QRectF filled = all;
    filled.setWidth(all.width() * share);
    auto fill = this->color_;
    fill.setAlpha(230);
    painter.fillRect(filled, fill);

    // Where the alert starts. It has to read on the fill as well as on the
    // bed, so it takes whichever of black and white stands out against the
    // colour it sits on.
    const auto mark = this->color_.lightness() > 140
                          ? QColor(0, 0, 0, 190)
                          : QColor(255, 255, 255, 220);
    painter.fillRect(
        QRectF(all.left() + (all.width() * AT_THRESHOLD) - 1, all.top(), 2,
               all.height()),
        mark);
}

}  // namespace chatterino
