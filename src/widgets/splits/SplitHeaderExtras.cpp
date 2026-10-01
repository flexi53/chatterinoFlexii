// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitHeaderExtras.hpp"

#include "Application.hpp"

#include "common/Channel.hpp"
#include "controllers/activity/ActivityMarks.hpp"
#include "controllers/twitch/ChannelNumbers.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/Helpers.hpp"
#include "util/RoundPixmap.hpp"
#include "util/UiStyle.hpp"

#include <QApplication>
#include <QEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegion>
#include <QTextLayout>
#include <QFontMetricsF>
#include <QPainterPath>

#include <algorithm>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

namespace chatterino {

void HeaderTitle::setRuns(std::vector<headerparts::Run> runs)
{
    if (runs == this->runs_)
    {
        return;
    }
    this->runs_ = std::move(runs);
    this->update();
}

void HeaderTitle::paintEvent(QPaintEvent *event)
{
    if (this->runs_.empty())
    {
        Label::paintEvent(event);
        return;
    }

    QPainter painter(this);
    const auto font =
        getApp()->getFonts()->getFont(this->getFontStyle(), this->scale());
    painter.setFont(font);

    // What is really on screen: a narrow header cuts the title short, and
    // what was cut away carries no colour any more
    const auto text = this->shouldElide_ ? this->elidedText_ : this->text_;
    const auto rect = this->textRect();

    // One line, laid out once, with a colour hung on the stretches that
    // have one. Measuring the pieces and painting them apart moved the
    // letters about - on a screen that draws two pixels for every one, by
    // a whole word.
    QTextLayout layout(text, font, painter.device());
    layout.setCacheEnabled(true);

    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(option);

    QList<QTextLayout::FormatRange> formats;
    for (const auto &run : this->runs_)
    {
        const auto from = std::clamp(run.from, 0, int(text.size()));
        const auto to = std::clamp(run.from + run.length, from,
                                   int(text.size()));
        if (from == to || !run.color.isValid())
        {
            continue;
        }
        QTextCharFormat format;
        format.setForeground(run.color);
        formats.append({.start = from, .length = to - from, .format = format});
    }
    layout.setFormats(formats);

    layout.beginLayout();
    auto line = layout.createLine();
    if (!line.isValid())
    {
        layout.endLayout();
        return;
    }
    line.setLineWidth(std::max(qreal(1), rect.width()));
    line.setPosition({0, 0});
    layout.endLayout();

    // From the left, or in the middle where the whole line fits, as Label
    // lays it out
    auto left = rect.left();
    if (this->centered_ && line.naturalTextWidth() <= rect.width())
    {
        left += (rect.width() - line.naturalTextWidth()) / 2;
    }
    const auto top = rect.top() + ((rect.height() - line.height()) / 2);

    painter.setPen(this->palette().windowText().color());
    layout.draw(&painter, QPointF(left, top));
}

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

void HeaderPicture::setExtraWidth(int pixels)
{
    if (pixels == this->extra_)
    {
        return;
    }
    this->extra_ = pixels;
    this->scaleChangedEvent(this->scale());
    this->update();
}

void HeaderPicture::setTallness(int pixels)
{
    if (pixels == this->tall_)
    {
        return;
    }
    this->tall_ = pixels;
    this->scaleChangedEvent(this->scale());
    this->update();
}

void HeaderPicture::scaleChangedEvent(float scale)
{
    // Twitch's covers are 52 by 72
    const auto height = int((this->tall_ > 0 ? this->tall_ : 16) * scale);
    auto width =
        this->shape_ == Shape::Cover ? int(height * 52 / 72.0) : height;
    width = std::max(width + int(this->extra_ * scale), int(6 * scale));
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

    // Buttons -> Kurve: drawn anew when any of it is switched
    const auto again = [this](auto, auto) {
        this->update();
    };
    getSettings()->curveLabels.connect(again, this->connections_, false);
    getSettings()->curveMarkMentions.connect(again, this->connections_, false);
    getSettings()->curveMarkAlerts.connect(again, this->connections_, false);
    getSettings()->curveMarkActions.connect(again, this->connections_, false);
    getSettings()->curveClick.connect(
        [this](auto on, auto) {
            this->setCursor(this->jump_ && on ? Qt::PointingHandCursor
                                              : Qt::ArrowCursor);
        },
        this->connections_, false);
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
        this->firstCategory_ = category;
        this->firstCategoryAt_ = this->now();
        this->update();
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
        const auto *help = static_cast<QHelpEvent *>(event);
        this->updateTooltip(help->pos().x());
    }
    return BaseWidget::event(event);
}

void ActivityGraph::updateTooltip(std::optional<int> at)
{
    this->setToolTip(this->description(at));
}

namespace {

/// The colour a mark under the curve is drawn in - told apart at a glance,
/// whatever the theme does with its own colours
QColor markColor(activitymarks::Kind kind, Theme *theme)
{
    switch (kind)
    {
        case activitymarks::Kind::Mention:
            return {0x4d, 0xa6, 0xff};
        case activitymarks::Kind::Alert:
            return {0xff, 0x8c, 0x1a};
        case activitymarks::Kind::ModAction:
            return {0x8c, 0xd8, 0x6b};
    }
    return theme->messages.textColors.regular;
}

}  // namespace

QString ActivityGraph::lengthLabel(qint64 seconds)
{
    const auto minutes = std::max<qint64>(0, seconds) / 60;
    if (minutes < 60)
    {
        return QStringLiteral("%1 Min.").arg(minutes);
    }
    return QStringLiteral("%1 Std. %2 Min.").arg(minutes / 60).arg(minutes % 60);
}

std::vector<ActivityGraph::Stretch> ActivityGraph::stretches() const
{
    const auto [from, to] = this->window();

    // Every point at which something else began, the first one included -
    // that one gets no line, but its stretch still has a name
    std::vector<std::pair<QDateTime, QString>> points;
    if (!this->firstCategory_.isEmpty())
    {
        points.emplace_back(this->firstCategoryAt_, this->firstCategory_);
    }
    for (const auto &change : this->changes_)
    {
        points.push_back(change);
    }
    if (points.empty())
    {
        return {};
    }

    // What ran when the curve begins - the last thing begun before it.
    // Before that nobody was watching, so that stretch stays nameless.
    QString running;
    auto start = from;
    size_t next = 0;
    for (; next < points.size() && points.at(next).first <= from; next++)
    {
        running = points.at(next).second;
    }

    std::vector<Stretch> stretches;
    const auto close = [&](const QDateTime &until, const QString &what) {
        if (what.isEmpty())
        {
            return;
        }
        stretches.push_back({.from = start, .to = until, .what = what});
    };

    for (; next < points.size(); next++)
    {
        const auto &[when, what] = points.at(next);
        if (when > to)
        {
            break;
        }
        close(when, running);
        start = when;
        running = what;
    }
    close(to, running);

    // A change this very moment leaves a stretch of no length behind it.
    // Only the last one stays - that is what runs now.
    std::vector<Stretch> kept;
    for (size_t i = 0; i < stretches.size(); i++)
    {
        if (stretches.at(i).from == stretches.at(i).to &&
            i + 1 < stretches.size())
        {
            continue;
        }
        kept.push_back(stretches.at(i));
    }
    return kept;
}

int ActivityGraph::messagesPerMinute() const
{
    if (this->spans_.empty())
    {
        return 0;
    }

    // The half minute running now and the one before it. The one running is
    // only part way through, so what they hold is counted against the time
    // they really cover - otherwise the number would halve every time a
    // half minute rolls over, and a steady chat would look as if it fell
    // silent.
    const auto count = qint64(this->spans_.size()) > 1
                           ? this->spans_.back() +
                                 this->spans_.at(this->spans_.size() - 2)
                           : this->spans_.back();

    const auto elapsed = std::clamp<qint64>(
        this->spansStart_.secsTo(this->now()) -
            (qint64(this->spans_.size()) - 1) * SPAN_SECONDS,
        0, SPAN_SECONDS);
    const auto covered =
        double((this->spans_.size() > 1 ? SPAN_SECONDS : 0) + elapsed);

    // A window barely open says nothing sensible
    return int(std::lround(double(count) * 60.0 / std::max(15.0, covered)));
}

std::vector<activitymarks::Mark> ActivityGraph::marksToShow(
    const QDateTime &from, const QDateTime &to) const
{
    if (this->channel_ == nullptr)
    {
        return {};
    }

    const auto *settings = getSettings();
    std::vector<activitymarks::Mark> shown;
    for (auto &mark : activitymarks::marks(this->channel_->getName(), from, to))
    {
        const bool wanted = [&] {
            switch (mark.kind)
            {
                case activitymarks::Kind::Mention:
                    return settings->curveMarkMentions.getValue();
                case activitymarks::Kind::Alert:
                    return settings->curveMarkAlerts.getValue();
                case activitymarks::Kind::ModAction:
                    return settings->curveMarkActions.getValue();
            }
            return false;
        }();
        if (wanted)
        {
            shown.push_back(mark);
        }
    }
    return shown;
}

std::optional<double> ActivityGraph::rateTrend() const
{
    if (this->spans_.size() < 2)
    {
        return {};
    }

    const auto now = this->now();
    const auto from = now.addSecs(-TREND_OVER);
    const auto until = now.addSecs(-TREND_LEAVES_OUT);

    // What came in over that stretch, and how much of it was counted at all
    int messages = 0;
    int spans = 0;
    for (size_t i = 0; i < this->spans_.size(); i++)
    {
        const auto when = this->spansStart_.addSecs(qint64(i) * SPAN_SECONDS);
        if (when < from || when > until)
        {
            continue;
        }
        messages += this->spans_.at(i);
        spans++;
    }

    // Too short a stretch says nothing - a channel just opened, or one that
    // has only been quiet
    if (spans * SPAN_SECONDS < TREND_NEEDS || messages == 0)
    {
        return {};
    }

    const auto before =
        double(messages) / (double(spans * SPAN_SECONDS) / 60.0);
    return (double(this->messagesPerMinute()) - before) / before;
}

QString ActivityGraph::description(std::optional<int> at) const
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
        text = QStringLiteral("Chat-Aktivität seit Streamstart (vor %1)")
                   .arg(lengthLabel(seconds));
    }
    else
    {
        text = QStringLiteral("Chat-Aktivität der letzten %1 Minuten")
                   .arg(WINDOW_SECONDS / 60);
    }

    text += QStringLiteral("\n%1 Nachrichten, zuletzt %2/min")
                .arg(total)
                .arg(this->messagesPerMinute());

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

    // What was streamed over the stretch, and how long each lasted. The one
    // the mouse stands on comes first, in full - in a narrow split there is
    // no room to write them into the curve.
    const auto stretches = this->stretches();
    const auto area = this->curveArea();
    std::optional<size_t> under;
    if (at && area.width() > 1)
    {
        const auto share = (double(*at) - area.left()) / area.width();
        if (share >= 0 && share <= 1)
        {
            const auto when = from.addSecs(qint64(share * double(seconds)));
            for (size_t i = 0; i < stretches.size(); i++)
            {
                if (when >= stretches.at(i).from && when <= stretches.at(i).to)
                {
                    under = i;
                    break;
                }
            }
        }
    }

    const auto line = [](const Stretch &stretch) {
        return QStringLiteral("%1 - %2 Uhr: %3 (%4)")
            .arg(stretch.from.toLocalTime().toString("HH:mm"),
                 stretch.to.toLocalTime().toString("HH:mm"), stretch.what,
                 lengthLabel(stretch.from.secsTo(stretch.to)));
    };

    if (under)
    {
        const auto &stretch = stretches.at(*under);
        text += QStringLiteral("\n\nHier: %1").arg(line(stretch));

        // What the chat did over that stretch
        int messages = 0;
        int busiest = 0;
        int counted = 0;
        for (size_t i = 0; i < this->spans_.size(); i++)
        {
            const auto when =
                this->spansStart_.addSecs(qint64(i) * SPAN_SECONDS);
            if (when < stretch.from || when > stretch.to)
            {
                continue;
            }
            messages += this->spans_.at(i);
            busiest = std::max(busiest, this->spans_.at(i));
            counted++;
        }
        if (counted > 0)
        {
            const auto perMinute =
                double(messages) * 60.0 / double(counted * SPAN_SECONDS);
            text += QStringLiteral("\n%1 Nachrichten, ⌀ %2/min, Spitze %3/min")
                        .arg(messages)
                        .arg(int(std::lround(perMinute)))
                        .arg(busiest * 2);
        }

        // and how many watched, as far as that was counted
        if (auto *twitch = dynamic_cast<TwitchChannel *>(this->channel_.get()))
        {
            const auto watched = channelnumbers::viewersBetween(
                twitch->roomId(), stretch.from, stretch.to);
            if (watched)
            {
                text += QStringLiteral("\nZuschauer ⌀ %1, Spitze %2")
                            .arg(localizeNumbers(watched->average),
                                 localizeNumbers(watched->most));
            }
        }

        // and what happened in it
        const auto inStretch =
            this->marksToShow(stretch.from, stretch.to);
        if (!inStretch.empty())
        {
            std::map<activitymarks::Kind, int> counts;
            for (const auto &mark : inStretch)
            {
                counts[mark.kind]++;
            }
            QStringList said;
            for (const auto &[kind, count] : counts)
            {
                said.append(QStringLiteral("%1× %2").arg(count).arg(
                    activitymarks::nameOf(kind)));
            }
            text += QStringLiteral("\n%1").arg(said.join(", "));
        }
    }
    if (stretches.size() > 1 || (!stretches.empty() && !under))
    {
        for (size_t i = 0; i < stretches.size(); i++)
        {
            if (under && i == *under)
            {
                continue;
            }
            text += QStringLiteral("\n%1").arg(line(stretches.at(i)));
        }
    }

    // What happened lately, as far as those marks are switched on - last of
    // all, so the stretch under the mouse stays at the top
    const auto marks = this->marksToShow(from, to);
    if (!marks.empty())
    {
        text += QStringLiteral("\n");
        const size_t most = 5;
        size_t shown = 0;
        for (auto it = marks.rbegin(); it != marks.rend() && shown < most;
             ++it, shown++)
        {
            text += QStringLiteral("\n%1 Uhr: %2 (%3)")
                        .arg(it->when.toLocalTime().toString("HH:mm"),
                             activitymarks::nameOf(it->kind), it->who);
        }
        if (marks.size() > most)
        {
            text += QStringLiteral("\n… und %1 weitere")
                        .arg(marks.size() - most);
        }
    }

    return text;
}

void ActivityGraph::whenClicked(std::function<void(const QDateTime &)> jump)
{
    this->jump_ = std::move(jump);
    this->setCursor(this->jump_ && getSettings()->curveClick
                        ? Qt::PointingHandCursor
                        : Qt::ArrowCursor);
}

QDateTime ActivityGraph::timeAt(int x) const
{
    const auto area = this->curveArea();
    if (area.width() <= 1)
    {
        return {};
    }

    const auto share = (double(x) - area.left()) / area.width();
    if (share < 0 || share > 1)
    {
        return {};
    }

    const auto [from, to] = this->window();
    return from.addSecs(qint64(share * double(from.secsTo(to))));
}

void ActivityGraph::mousePressEvent(QMouseEvent *event)
{
    if (!this->jump_ || !getSettings()->curveClick ||
        event->button() != Qt::LeftButton)
    {
        BaseWidget::mousePressEvent(event);
        return;
    }
    this->pressedAt_ = event->pos();
    event->accept();
}

void ActivityGraph::mouseReleaseEvent(QMouseEvent *event)
{
    if (!this->jump_ || !getSettings()->curveClick ||
        event->button() != Qt::LeftButton)
    {
        BaseWidget::mouseReleaseEvent(event);
        return;
    }

    // A click, not the end of a drag
    if ((event->pos() - this->pressedAt_).manhattanLength() >
        QApplication::startDragDistance())
    {
        event->accept();
        return;
    }

    const auto when = this->timeAt(event->pos().x());
    if (when.isValid())
    {
        this->jump_(when);
    }
    event->accept();
}

void ActivityGraph::scaleChangedEvent(float scale)
{
    // Room before the curve and some more after it. It would like to be
    // wide enough that a whole stream is worth looking at, but gives way
    // to the title when the split is narrow.
    this->setFixedHeight(int(this->tallness() * scale));
    this->updateGeometry();
}

int ActivityGraph::tallness() const
{
    return this->tallness_ > 0 ? this->tallness_ : uistyle::headerHeight();
}

void ActivityGraph::setTallness(int pixels)
{
    if (pixels == this->tallness_)
    {
        return;
    }
    this->tallness_ = pixels;
    this->scaleChangedEvent(this->scale());
    this->update();
}

void ActivityGraph::setWantedWidth(int pixels)
{
    pixels = std::max(pixels, 0);
    if (pixels == this->wantedWidth_)
    {
        return;
    }
    this->wantedWidth_ = pixels;
    this->updateGeometry();
}

int ActivityGraph::ownWidth() const
{
    return int((LEFT_ROOM + WIDE + RIGHT_ROOM) * this->scale());
}

void ActivityGraph::showSample()
{
    // Quiet, a burst, calmer, and busy again towards now - the whole
    // quarter hour, so no stretch is left unknown
    const std::vector<int> sample{
        2, 3, 2, 4, 5, 4, 6, 9, 14, 11, 7, 5, 4, 5, 6, 5,
        4, 3, 4, 6, 8, 7, 6, 8, 10, 12, 11, 9, 10, 12, 11,
    };
    this->spans_.assign(sample.begin(), sample.end());
    this->spansStart_ = this->now().addSecs(-WINDOW_SECONDS);
    this->update();
}

QSize ActivityGraph::sizeHint() const
{
    return {this->wantedWidth_ > 0 ? this->wantedWidth_ : this->ownWidth(),
            int(this->tallness() * this->scale())};
}

QSize ActivityGraph::minimumSizeHint() const
{
    return {int((LEFT_ROOM + NARROWEST + RIGHT_ROOM) * this->scale()),
            int(this->tallness() * this->scale())};
}

QRectF ActivityGraph::curveArea() const
{
    // Room at the bottom for the line of time under the curve, and under
    // that for what its marks stand for - Compact's lower header has no
    // room for those, the tooltip says them
    const bool labels = !uistyle::compact();
    return QRectF(this->rect())
        .adjusted(LEFT_ROOM * this->scale(), 2, -RIGHT_ROOM * this->scale(),
                  -(6 + (labels ? LABEL_ROOM : 0)) * this->scale());
}

void ActivityGraph::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool labels = !uistyle::compact();
    const QRectF area = this->curveArea();
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

    // A wide curve has more columns than there are half minutes. Such a
    // column holds the half minute it falls in, rather than dropping to
    // nothing between the starts - which drew a comb instead of a line.
    for (size_t c = 0; c < columns; c++)
    {
        if (counted.at(c) > 0)
        {
            continue;
        }
        const auto since = double(this->spansStart_.secsTo(from)) +
                           (double(c) * perColumn);
        if (since < 0)
        {
            continue;
        }
        const auto span = size_t(since / SPAN_SECONDS);
        if (span < this->spans_.size())
        {
            values.at(c) = double(this->spans_.at(span));
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
    for (qint64 back = labelStep; labels && labelStep > 0 && back <= seconds;
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

    // What happened while you watched: your name, an alert, a timeout you
    // gave - a small tick each, right above the line of time
    if (this->channel_ != nullptr)
    {
        const auto marks = this->marksToShow(from, to);
        const auto tick = 4.0 * this->scale();
        double lastX = -1000;
        for (const auto &mark : marks)
        {
            const auto offset = double(from.secsTo(mark.when));
            if (offset < 0 || offset > double(seconds))
            {
                continue;
            }
            const auto x =
                area.left() + (area.width() * offset / double(seconds));
            // Two marks on the same spot would only smudge each other
            if (std::abs(x - lastX) < tick)
            {
                continue;
            }
            lastX = x;

            const auto color = markColor(mark.kind, getTheme());

            // A short stem up into the curve, so it is seen at a glance
            auto stem = color;
            stem.setAlpha(120);
            painter.setPen(QPen(stem, std::max(1.0, double(this->scale()))));
            painter.drawLine(QPointF(x, axisY - (tick * 1.8)),
                             QPointF(x, axisY));

            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            QPainterPath pointer;
            pointer.moveTo(x, axisY - tick);
            pointer.lineTo(x - (tick * 0.75), axisY + (tick * 0.5));
            pointer.lineTo(x + (tick * 0.75), axisY + (tick * 0.5));
            pointer.closeSubpath();
            painter.drawPath(pointer);
        }
        painter.setBrush(Qt::NoBrush);
    }

    // What ran between those lines, written over the curve where the
    // stretch is wide enough to read it - the tooltip has them all anyway
    auto over = muted;
    over.setAlpha(165);
    painter.setPen(over);
    for (const auto &stretch :
         getSettings()->curveLabels ? this->stretches() : std::vector<Stretch>{})
    {
        const auto left =
            area.left() +
            (area.width() * double(from.secsTo(stretch.from)) / double(seconds));
        const auto right =
            area.left() +
            (area.width() * double(from.secsTo(stretch.to)) / double(seconds));
        const QRectF room(left + (2 * this->scale()), area.top(),
                          right - left - (4 * this->scale()),
                          metrics.height());
        // A stump of two letters says nothing and only covers the curve
        if (room.width() < LABEL_NEEDS * this->scale())
        {
            continue;
        }

        // The length joins the name where both fit whole, said short:
        // "42 min", "1 h 42", "2 h"
        const auto minutes = stretch.from.secsTo(stretch.to) / 60;
        const auto length =
            minutes < 60 ? QStringLiteral("%1 min").arg(minutes)
            : minutes % 60 == 0
                ? QStringLiteral("%1 h").arg(minutes / 60)
                : QStringLiteral("%1 h %2").arg(minutes / 60).arg(minutes % 60);
        const auto both = QStringLiteral("%1 · %2").arg(stretch.what, length);
        const auto text =
            metrics.horizontalAdvance(both) <= room.width() ? both
                                                            : stretch.what;
        const auto shown =
            metrics.elidedText(text, Qt::ElideRight, room.width());

        // On a plate of the header's own colour - the curve runs behind it,
        // and red under grey letters reads badly
        QRectF plate(0, room.top(), metrics.horizontalAdvance(shown) + (6 * this->scale()),
                     metrics.height());
        plate.moveCenter(QPointF(room.center().x(), plate.center().y()));
        auto back = getTheme()->splits.header.background;
        back.setAlpha(210);
        painter.setPen(Qt::NoPen);
        painter.setBrush(back);
        painter.drawRoundedRect(plate, 3 * this->scale(), 3 * this->scale());
        painter.setBrush(Qt::NoBrush);
        painter.setPen(over);
        painter.drawText(plate, Qt::AlignCenter, shown);
    }
}

}  // namespace chatterino
