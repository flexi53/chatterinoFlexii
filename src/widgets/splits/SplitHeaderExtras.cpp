// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitHeaderExtras.hpp"

#include "common/Channel.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Theme.hpp"
#include "util/RoundPixmap.hpp"

#include <QEvent>
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
    this->timer_.setInterval(SPAN_SECONDS * 1000);
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
    this->changes_.fill({});
    this->category_.clear();
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

        if (auto *twitch = dynamic_cast<TwitchChannel *>(channel.get()))
        {
            // What it streams now is the starting point, not a change
            this->category_ = twitch->accessStreamStatus()->game;
            this->connections_.managedConnect(twitch->streamStatusChanged,
                                              [this] {
                                                  this->checkCategory();
                                              });
        }
    }
    this->updateTooltip();
    this->update();
}

void ActivityGraph::shift()
{
    std::rotate(this->counts_.begin(), this->counts_.begin() + 1,
                this->counts_.end());
    this->counts_.back() = 0;
    std::rotate(this->changes_.begin(), this->changes_.begin() + 1,
                this->changes_.end());
    this->changes_.back().clear();
    this->updateTooltip();
    this->update();
}

void ActivityGraph::checkCategory()
{
    auto *twitch = dynamic_cast<TwitchChannel *>(this->channel_.get());
    if (twitch != nullptr)
    {
        this->noteCategory(twitch->accessStreamStatus()->game);
    }
}

void ActivityGraph::noteCategory(const QString &category)
{
    if (category.isEmpty() || category == this->category_)
    {
        return;
    }

    // Nothing to mark while the channel had no category to begin with -
    // that is a stream starting, not a change
    const auto had = !this->category_.isEmpty();
    this->category_ = category;
    if (!had)
    {
        return;
    }

    this->changes_.back() = category;
    this->updateTooltip();
    this->update();
}

bool ActivityGraph::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        this->updateTooltip();
    }
    return BaseWidget::event(event);
}

void ActivityGraph::updateTooltip()
{
    this->setToolTip(this->description());
}

QString ActivityGraph::description() const
{
    // The last two spans make the last minute
    const auto lastMinute =
        this->counts_.at(this->counts_.size() - 2) + this->counts_.back();
    const auto total =
        std::accumulate(this->counts_.begin(), this->counts_.end(), 0);
    const auto minutes = int(SPANS) * SPAN_SECONDS / 60;

    auto text = QStringLiteral("Chat-Aktivität der letzten %1 Minuten\n"
                               "%2 Nachrichten, zuletzt etwa %3 pro Minute\n"
                               "Ein Strich unten je Minute")
                    .arg(minutes)
                    .arg(total)
                    .arg(lastMinute);

    // What it streams, and where that changed
    for (size_t i = 0; i < SPANS; i++)
    {
        if (this->changes_.at(i).isEmpty())
        {
            continue;
        }
        const auto ago = int(SPANS - i) * SPAN_SECONDS / 60;
        text += QStringLiteral("\nvor %1 Min.: %2")
                    .arg(ago)
                    .arg(this->changes_.at(i));
    }

    return text;
}

void ActivityGraph::scaleChangedEvent(float scale)
{
    // Room before the curve and some more after it. It would like to be
    // wide enough that a quarter of an hour is worth looking at, but gives
    // way to the title when the split is narrow.
    this->setFixedHeight(int(20 * scale));
    this->updateGeometry();
}

QSize ActivityGraph::sizeHint() const
{
    return {int((3 + 104 + 6) * this->scale()), int(20 * this->scale())};
}

QSize ActivityGraph::minimumSizeHint() const
{
    return {int((3 + 36 + 6) * this->scale()), int(20 * this->scale())};
}

void ActivityGraph::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto highest = std::max(
        1, *std::max_element(this->counts_.begin(), this->counts_.end()));
    // Room at the bottom for the line of time under the curve
    const QRectF area = QRectF(this->rect())
                            .adjusted(3 * this->scale(), 2, -6 * this->scale(),
                                      -6 * this->scale());
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

    // The line of time: one tick per minute, a taller one every five, so
    // it is clear how far back the curve reaches
    const auto axisY = area.bottom() + (1.5 * this->scale());
    auto muted = this->palette().color(QPalette::WindowText);
    muted.setAlpha(60);
    QPen axisPen(muted);
    axisPen.setWidthF(1);
    painter.setPen(axisPen);
    painter.drawLine(QPointF(area.left(), axisY), QPointF(area.right(), axisY));

    const auto perMinute = size_t(60 / SPAN_SECONDS);
    for (size_t k = 0; k * perMinute < SPANS; k++)
    {
        const auto i = SPANS - 1 - (k * perMinute);
        const auto x = area.left() + (step * qreal(i));
        const auto every5 = k % 5 == 0;
        auto tick = muted;
        tick.setAlpha(every5 ? 130 : 60);
        painter.setPen(QPen(tick, 1));
        painter.drawLine(
            QPointF(x, axisY),
            QPointF(x, axisY + ((every5 ? 3.5 : 2) * this->scale())));
        if (i == 0)
        {
            break;
        }
    }

    // Where the channel changed what it streams
    auto changeColor = this->palette().color(QPalette::WindowText);
    changeColor.setAlpha(150);
    QPen changePen(changeColor);
    changePen.setWidthF(1);
    changePen.setStyle(Qt::DotLine);
    painter.setPen(changePen);
    for (size_t i = 0; i < SPANS; i++)
    {
        if (this->changes_.at(i).isEmpty())
        {
            continue;
        }
        const auto x = area.left() + (step * qreal(i));
        painter.drawLine(QPointF(x, area.top()), QPointF(x, axisY));
    }
}

}  // namespace chatterino
