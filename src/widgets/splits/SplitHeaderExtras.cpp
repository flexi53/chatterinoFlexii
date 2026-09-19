// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitHeaderExtras.hpp"

#include "common/Channel.hpp"
#include "messages/Message.hpp"
#include "singletons/Theme.hpp"
#include "util/RoundPixmap.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <numeric>

namespace chatterino {

HeaderPicture::HeaderPicture(Shape shape, int gap, QWidget *parent)
    : BaseWidget(parent)
    , shape_(shape)
    , gap_(gap)
{
    this->hide();
    this->scaleChangedEvent(this->scale());
}

void HeaderPicture::setPicture(const QPixmap &picture)
{
    this->picture_ = picture;
    this->setVisible(!picture.isNull());
    this->update();
}

void HeaderPicture::scaleChangedEvent(float scale)
{
    // Twitch's covers are 52 by 72
    const auto height = int(16 * scale);
    const auto width =
        this->shape_ == Shape::Cover ? int(height * 52 / 72.0) : height;
    this->setFixedSize(width + int(this->gap_ * scale), height);
}

QRectF HeaderPicture::pictureRect() const
{
    return {0, 0, qreal(this->width() - int(this->gap_ * this->scale())),
            qreal(this->height())};
}

void HeaderPicture::paintEvent(QPaintEvent * /*event*/)
{
    if (this->picture_.isNull())
    {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF area = this->pictureRect();
    if (this->shape_ == Shape::Round)
    {
        paintRound(painter, area, this->picture_);
        return;
    }
    QPainterPath corners;
    corners.addRoundedRect(area, 2 * this->scale(), 2 * this->scale());
    painter.setClipPath(corners);
    painter.drawPixmap(area, this->picture_, this->picture_.rect());
}

ActivityGraph::ActivityGraph(QWidget *parent)
    : BaseWidget(parent)
{
    this->hide();
    this->scaleChangedEvent(this->scale());
    this->timer_.setInterval(30000);
    QObject::connect(&this->timer_, &QTimer::timeout, this, [this] {
        this->shift();
    });
    this->timer_.start();
    this->updateTooltip();
}

void ActivityGraph::setChannel(const ChannelPtr &channel)
{
    if (channel == this->channel_)
    {
        return;
    }
    this->channel_ = channel;
    this->counts_.fill(0);
    this->connections_.clear();
    if (channel != nullptr)
    {
        this->connections_.managedConnect(
            channel->messageAppended, [this](auto &message, auto) {
                // What people write, not what the app says
                if (!message->flags.has(MessageFlag::System))
                {
                    this->counts_.back()++;
                }
            });
    }
    this->updateTooltip();
    this->update();
}

void ActivityGraph::shift()
{
    std::rotate(this->counts_.begin(), this->counts_.begin() + 1,
                this->counts_.end());
    this->counts_.back() = 0;
    this->updateTooltip();
    this->update();
}

void ActivityGraph::updateTooltip()
{
    // The last two spans make the last minute
    const auto lastMinute =
        this->counts_.at(this->counts_.size() - 2) + this->counts_.back();
    const auto total =
        std::accumulate(this->counts_.begin(), this->counts_.end(), 0);
    this->setToolTip(QStringLiteral("Chat-Aktivität der letzten 10 Minuten\n"
                                    "%1 Nachrichten, zuletzt etwa %2 pro "
                                    "Minute")
                         .arg(total)
                         .arg(lastMinute));
}

void ActivityGraph::scaleChangedEvent(float scale)
{
    // The curve and a little room on either side of it
    this->setFixedSize(int((40 + (2 * 6)) * scale), int(16 * scale));
}

void ActivityGraph::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto highest = std::max(
        1, *std::max_element(this->counts_.begin(), this->counts_.end()));
    const auto side = 6 * this->scale();
    const QRectF area = QRectF(this->rect()).adjusted(side, 2, -side, -2);
    const auto step = area.width() / qreal(this->counts_.size() - 1);

    QPainterPath line;
    for (size_t i = 0; i < this->counts_.size(); i++)
    {
        const QPointF point(
            area.left() + (step * qreal(i)),
            area.bottom() - (area.height() * this->counts_.at(i) / highest));
        if (i == 0)
        {
            line.moveTo(point);
        }
        else
        {
            line.lineTo(point);
        }
    }

    // Red, as live is shown
    QColor color = getTheme()->tabs.liveIndicator;
    // A soft fill under the curve, and the curve itself
    QPainterPath fill = line;
    fill.lineTo(area.bottomRight());
    fill.lineTo(area.bottomLeft());
    fill.closeSubpath();
    QColor soft = color;
    soft.setAlpha(55);
    painter.fillPath(fill, soft);

    color.setAlpha(230);
    QPen pen(color);
    pen.setWidthF(1.2 * this->scale());
    painter.setPen(pen);
    painter.drawPath(line);
}

}  // namespace chatterino
