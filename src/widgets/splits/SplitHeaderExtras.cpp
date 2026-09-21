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
#include <QFontMetricsF>
#include <QPainterPath>

#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>

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
    , clock_([] {
        return QDateTime::currentDateTimeUtc();
    })
{
    this->hide();
    this->scaleChangedEvent(this->scale());
    this->spansStart_ = this->now();
    this->spans_.push_back(0);

    this->timer_.setInterval(SPAN_SECONDS * 1000);
    QObject::connect(&this->timer_, &QTimer::timeout, this, [this] {
        this->catchUp();
    });
    this->timer_.start();
    this->updateTooltip();
}

void ActivityGraph::setClock(std::function<QDateTime()> clock)
{
    this->clock_ = std::move(clock);
    this->spansStart_ = this->now();
    this->spans_.assign(1, 0);
}

QDateTime ActivityGraph::now() const
{
    return this->clock_();
}

void ActivityGraph::setChannel(const ChannelPtr &channel)
{
    if (channel == this->channel_)
    {
        return;
    }
    this->channel_ = channel;
    this->spans_.assign(1, 0);
    this->spansStart_ = this->now();
    this->changes_.clear();
    this->category_.clear();
    this->streamStart_ = {};
    this->streamId_.clear();
    this->connections_.clear();

    if (channel != nullptr)
    {
        this->connections_.managedConnect(
            channel->messageAppended, [this](auto &message, auto) {
                // What people write, not what the app says
                if (!message->flags.has(MessageFlag::System))
                {
                    this->spans_.at(this->currentSpan())++;
                    this->update();
                }
            });

        if (auto *twitch = dynamic_cast<TwitchChannel *>(channel.get()))
        {
            this->connections_.managedConnect(twitch->streamStatusChanged,
                                              [this] {
                                                  this->checkStream();
                                              });
            this->checkStream();
        }
    }
    this->updateTooltip();
    this->update();
}

size_t ActivityGraph::currentSpan()
{
    const auto passed = this->spansStart_.secsTo(this->now());
    const auto wanted = size_t(std::max<qint64>(0, passed / SPAN_SECONDS)) + 1;

    if (wanted > this->spans_.size())
    {
        this->spans_.resize(wanted, 0);
    }
    if (this->spans_.size() > MOST_SPANS)
    {
        const auto tooMany = this->spans_.size() - MOST_SPANS;
        this->spans_.erase(this->spans_.begin(),
                           this->spans_.begin() + qsizetype(tooMany));
        this->spansStart_ =
            this->spansStart_.addSecs(qint64(tooMany) * SPAN_SECONDS);
    }
    return this->spans_.size() - 1;
}

void ActivityGraph::catchUp()
{
    this->currentSpan();
    this->update();
}

void ActivityGraph::checkStream()
{
    auto *twitch = dynamic_cast<TwitchChannel *>(this->channel_.get());
    if (twitch == nullptr)
    {
        return;
    }

    QString id;
    QString game;
    int uptime = 0;
    bool live = false;
    {
        auto status = twitch->accessStreamStatus();
        live = status->live;
        id = status->streamId;
        game = status->game;
        uptime = status->uptimeSeconds;
    }

    if (!live)
    {
        this->unfollowStream();
        return;
    }

    const auto started = id != this->streamId_;
    this->followStream(id, this->now().addSecs(-uptime));
    if (started)
    {
        // What it streams now is the starting point, not a change
        this->category_ = game;
        this->updateTooltip();
        return;
    }

    this->noteCategory(game);
}

void ActivityGraph::followStream(const QString &id, const QDateTime &start)
{
    if (id == this->streamId_)
    {
        return;
    }

    this->streamId_ = id;
    this->streamStart_ = start;
    this->category_.clear();

    // What was counted since the stream went live belongs to it - a tab
    // that was already open has been counting all along, so only what came
    // before the stream is let go of
    const auto before = this->spansStart_.secsTo(start) / SPAN_SECONDS;
    if (before >= qint64(this->spans_.size()))
    {
        // Nothing of this stream was seen yet
        this->spans_.assign(1, 0);
        this->spansStart_ = this->now();
    }
    else if (before > 0)
    {
        this->spans_.erase(this->spans_.begin(),
                           this->spans_.begin() + qsizetype(before));
        this->spansStart_ = this->spansStart_.addSecs(before * SPAN_SECONDS);
    }

    std::erase_if(this->changes_, [&start](const auto &change) {
        return change.first < start;
    });

    this->updateTooltip();
    this->update();
}

void ActivityGraph::unfollowStream()
{
    if (this->streamId_.isEmpty() && !this->streamStart_.isValid())
    {
        return;
    }
    this->streamStart_ = {};
    this->streamId_.clear();
    this->updateTooltip();
    this->update();
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

    this->changes_.emplace_back(this->now(), category);
    this->updateTooltip();
    this->update();
}

std::pair<QDateTime, QDateTime> ActivityGraph::window() const
{
    const auto to = this->clock_();
    if (this->streamStart_.isValid())
    {
        auto from = this->streamStart_;
        if (from.secsTo(to) < SHORTEST_SECONDS)
        {
            from = to.addSecs(-SHORTEST_SECONDS);
        }
        return {from, to};
    }
    return {to.addSecs(-WINDOW_SECONDS), to};
}

int ActivityGraph::tickSeconds(qint64 seconds)
{
    // A tick every so often, so there are never more than about ten
    for (const auto step : {60, 300, 900, 1800, 3600, 7200, 14400, 21600})
    {
        if (seconds / step <= 10)
        {
            return step;
        }
    }
    return 43200;
}

QString ActivityGraph::timeLabel(qint64 seconds)
{
    if (seconds < 3600)
    {
        return QStringLiteral("%1 min").arg(seconds / 60);
    }
    const auto hours = seconds / 3600;
    const auto minutes = (seconds % 3600) / 60;
    if (minutes == 0)
    {
        return QStringLiteral("%1h").arg(hours);
    }
    return QStringLiteral("%1h%2").arg(hours).arg(minutes, 2, 10, QChar('0'));
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
    const auto [from, to] = this->window();
    const auto seconds = std::max<qint64>(1, from.secsTo(to));
    // Only what falls inside the stretch the curve shows
    int total = 0;
    for (size_t i = 0; i < this->spans_.size(); i++)
    {
        const auto when = this->spansStart_.addSecs(qint64(i) * SPAN_SECONDS);
        if (when >= from && when <= to)
        {
            total += this->spans_.at(i);
        }
    }

    // The last two spans make the last minute
    auto lastMinute = this->spans_.back();
    if (this->spans_.size() > 1)
    {
        lastMinute += this->spans_.at(this->spans_.size() - 2);
    }

    QString text;
    if (this->streamStart_.isValid())
    {
        const auto hours = seconds / 3600;
        const auto minutes = (seconds % 3600) / 60;
        text = QStringLiteral("Chat-Aktivität seit Streamstart (vor %1)")
                   .arg(hours > 0 ? QStringLiteral("%1 Std. %2 Min.")
                                        .arg(hours)
                                        .arg(minutes)
                                  : QStringLiteral("%1 Min.").arg(minutes));
    }
    else
    {
        text = QStringLiteral("Chat-Aktivität der letzten %1 Minuten")
                   .arg(WINDOW_SECONDS / 60);
    }

    text += QStringLiteral("\n%1 Nachrichten, zuletzt etwa %2 pro Minute")
                .arg(total)
                .arg(lastMinute);

    const auto tick = tickSeconds(seconds);
    text += QStringLiteral("\nEin Strich unten je %1")
                .arg(tick < 3600 ? QStringLiteral("%1 Min.").arg(tick / 60)
                                 : QStringLiteral("%1 Std.").arg(tick / 3600));

    // Where the counting could not reach back
    if (this->spansStart_ > from.addSecs(SPAN_SECONDS))
    {
        text += QStringLiteral("\nVor dem Öffnen des Kanals zählt niemand "
                               "mit - der Anfang bleibt leer");
    }

    for (const auto &[when, what] : this->changes_)
    {
        if (when < from)
        {
            continue;
        }
        text += QStringLiteral("\n%1 Uhr: %2")
                    .arg(when.toLocalTime().toString("HH:mm"), what);
    }

    return text;
}

void ActivityGraph::scaleChangedEvent(float scale)
{
    // Room before the curve and some more after it. It would like to be
    // wide enough that a whole stream is worth looking at, but gives way
    // to the title when the split is narrow.
    this->setFixedHeight(int(HEIGHT * scale));
    this->updateGeometry();
}

QSize ActivityGraph::sizeHint() const
{
    return {int((3 + WIDE + 6) * this->scale()), int(HEIGHT * this->scale())};
}

QSize ActivityGraph::minimumSizeHint() const
{
    return {int((3 + 36 + 6) * this->scale()), int(HEIGHT * this->scale())};
}

void ActivityGraph::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Room at the bottom for the line of time under the curve, and under
    // that for what its marks stand for
    const QRectF area =
        QRectF(this->rect())
            .adjusted(3 * this->scale(), 2, -6 * this->scale(),
                      -(6 + LABEL_ROOM) * this->scale());
    if (area.width() <= 1)
    {
        return;
    }

    const auto [from, to] = this->window();
    const auto seconds = std::max<qint64>(1, from.secsTo(to));
    const auto columns = size_t(std::max(2.0, area.width()));
    const auto perColumn = double(seconds) / double(columns);

    // What was written, gathered into one value per column of pixels. A
    // column stands for what came in per half minute on average, so a
    // single burst does not tower over a whole busy hour.
    std::vector<double> values(columns, 0.0);
    std::vector<int> counted(columns, 0);
    for (size_t i = 0; i < this->spans_.size(); i++)
    {
        const auto when = this->spansStart_.addSecs(qint64(i) * SPAN_SECONDS);
        const auto offset = double(from.secsTo(when));
        if (offset < 0 || offset > double(seconds))
        {
            continue;
        }
        const auto column =
            size_t(std::min(double(columns - 1), offset / perColumn));
        values.at(column) += double(this->spans_.at(i));
        counted.at(column)++;
    }
    for (size_t c = 0; c < columns; c++)
    {
        if (counted.at(c) > 1)
        {
            values.at(c) /= double(counted.at(c));
        }
    }

    const auto highest =
        std::max(1.0, *std::max_element(values.begin(), values.end()));

    QPainterPath line;
    for (size_t c = 0; c < columns; c++)
    {
        const QPointF point(
            area.left() + (area.width() * double(c) / double(columns - 1)),
            area.bottom() - (area.height() * values.at(c) / highest));
        if (c == 0)
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

    // From the theme rather than the palette: the header's palette is not
    // repainted when the theme changes, which left the line of time dark on
    // a light theme
    auto muted = getTheme()->messages.textColors.regular;

    // The stretch before the channel was open here - nobody counted then
    const auto unknown = double(from.secsTo(this->spansStart_));
    if (unknown > perColumn)
    {
        auto dark = muted;
        dark.setAlpha(30);
        QRectF band = area;
        band.setWidth(area.width() * std::min(1.0, unknown / double(seconds)));
        painter.fillRect(band, dark);
    }

    // The line of time: a tick every so often, so it is clear how far back
    // the curve reaches
    const auto axisY = area.bottom() + (1.5 * this->scale());
    auto axis = muted;
    axis.setAlpha(60);
    painter.setPen(QPen(axis, 1));
    painter.drawLine(QPointF(area.left(), axisY), QPointF(area.right(), axisY));

    const auto tick = tickSeconds(seconds);
    for (qint64 back = 0; back <= seconds; back += tick)
    {
        const auto x =
            area.right() - (area.width() * double(back) / double(seconds));
        const auto tall = (back / tick) % 5 == 0;
        auto mark = muted;
        mark.setAlpha(tall ? 130 : 60);
        painter.setPen(QPen(mark, 1));
        painter.drawLine(
            QPointF(x, axisY),
            QPointF(x, axisY + ((tall ? 3.5 : 2) * this->scale())));
    }

    // What the marks stand for - "10 min", "30 min", "1h" - under as many
    // of them as fit without crowding, counted back from now on the right
    QFont small = this->font();
    small.setPixelSize(std::max(7, int(8 * this->scale())));
    painter.setFont(small);
    const QFontMetricsF metrics(small);
    auto label = muted;
    label.setAlpha(150);
    painter.setPen(label);

    // Round steps read best - 30 min, 1h, 2h rather than 45 min, 1h15 -
    // so the labels go on the roundest step of the marks that leaves room
    // enough between them
    const auto widest = metrics.horizontalAdvance(QStringLiteral("30 min"));
    const auto perSecond = area.width() / double(seconds);
    qint64 labelStep = 0;
    for (const qint64 step : {300, 600, 900, 1800, 3600, 7200, 10800, 14400,
                              21600, 43200, 86400})
    {
        if (step < tick || step % tick != 0)
        {
            continue;
        }
        if (double(step) * perSecond >= widest + (8 * this->scale()))
        {
            labelStep = step;
            break;
        }
    }

    const auto labelTop = axisY + (3.5 * this->scale());
    for (qint64 back = labelStep; labelStep > 0 && back <= seconds;
         back += labelStep)
    {
        const auto x = area.right() - (double(back) * perSecond);
        const auto text = timeLabel(back);
        const auto width = metrics.horizontalAdvance(text);
        const auto left = x - (width / 2);
        // Only whole labels - one that would run off the left is left out
        if (left < 0)
        {
            break;
        }
        painter.drawText(QRectF(left, labelTop, width + 1,
                                this->height() - labelTop),
                         Qt::AlignHCenter | Qt::AlignTop, text);
    }

    // Where the channel changed what it streams
    // Bright enough to stand out against the curve behind it
    auto changeColor = muted;
    changeColor.setAlpha(220);
    QPen changePen(changeColor);
    changePen.setWidthF(std::max(1.0, 1.0 * this->scale()));
    changePen.setStyle(Qt::DashLine);
    painter.setPen(changePen);
    for (const auto &[when, what] : this->changes_)
    {
        const auto offset = double(from.secsTo(when));
        if (offset < 0 || offset > double(seconds))
        {
            continue;
        }
        const auto x = area.left() + (area.width() * offset / double(seconds));
        painter.drawLine(QPointF(x, area.top()), QPointF(x, axisY));
    }
}

}  // namespace chatterino
