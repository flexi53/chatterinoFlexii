// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SendWaitBar.hpp"

#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace chatterino {

namespace {

/// How often the bar is redrawn while it runs
constexpr int TICK_MS = 40;

/// How tall the bar is, in unscaled pixels
constexpr int HEIGHT = 3;

}  // namespace

SendWaitBar::SendWaitBar(QWidget *parent)
    : BaseWidget(parent)
{
    this->hide();
    this->scaleChangedEvent(this->scale());

    this->tick_.setInterval(TICK_MS);
    this->tick_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->tick_, &QTimer::timeout, this, [this] {
        if (this->share() <= 0)
        {
            this->stop();
            return;
        }
        this->update();
    });

    getSettings()->slowModeBar.connect(
        [this](const bool on, auto) {
            if (!on)
            {
                this->stop();
            }
        },
        this->signalHolder_);
}

void SendWaitBar::run(std::chrono::milliseconds remaining,
                      std::chrono::milliseconds total)
{
    if (!getSettings()->slowModeBar || remaining <= std::chrono::seconds(0) ||
        total <= std::chrono::seconds(0))
    {
        this->stop();
        return;
    }

    this->end_ = std::chrono::steady_clock::now() + remaining;
    this->total_ = std::max(total, remaining);
    this->show();
    this->tick_.start();
    this->update();
}

void SendWaitBar::stop()
{
    this->tick_.stop();
    this->total_ = std::chrono::milliseconds(0);
    this->hide();
}

double SendWaitBar::share() const
{
    if (this->total_ <= std::chrono::milliseconds(0))
    {
        return 0;
    }

    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        this->end_ - std::chrono::steady_clock::now());
    if (left <= std::chrono::milliseconds(0))
    {
        return 0;
    }
    return std::min(1.0, double(left.count()) / double(this->total_.count()));
}

void SendWaitBar::paintEvent(QPaintEvent * /*event*/)
{
    const auto share = this->share();
    if (share <= 0)
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF all(this->rect());

    // The part still to wait, on a faint bed so the bar keeps its place
    auto bed = this->theme->accent;
    bed.setAlpha(40);
    painter.fillRect(all, bed);

    QRectF left = all;
    left.setWidth(all.width() * share);
    auto color = this->theme->accent;
    color.setAlpha(220);
    painter.fillRect(left, color);
}

void SendWaitBar::scaleChangedEvent(float scale)
{
    this->setFixedHeight(std::max(2, int(HEIGHT * scale)));
}

}  // namespace chatterino
