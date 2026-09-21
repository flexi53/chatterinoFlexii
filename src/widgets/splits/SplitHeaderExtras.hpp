// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QDateTime>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;

/// A small picture in the split header - see Look -> Tabs: the channel's
/// own, round, or the cover of what it streams, as Twitch shows it
class HeaderPicture : public BaseWidget
{
public:
    enum class Shape {
        Round,
        Cover,
    };

    /// @a gap is the room after it, in unscaled pixels - only there while
    /// it shows
    HeaderPicture(Shape shape, int gap, QWidget *parent);

    /// Shows @a picture; a null one hides it
    void setPicture(const QPixmap &picture);

protected:
    void paintEvent(QPaintEvent *event) override;
    void scaleChangedEvent(float scale) override;

private:
    /// Where the picture goes, without the gap after it
    QRectF pictureRect() const;

    Shape shape_;
    int gap_;
    QPixmap picture_;
};

/// How lively the chat was, as a small curve in the split header - see
/// Look -> Tabs. It follows the stream from the moment it went live, with
/// a line of time under it and an upright line wherever the channel
/// changed what it streams. What happened before the channel was open here
/// nobody can know, so that stretch stays empty.
class ActivityGraph : public BaseWidget
{
public:
    /// How long one column of the count stands for
    static constexpr int SPAN_SECONDS = 30;
    /// As far back as the counts are ever kept - a day of half minutes
    static constexpr size_t MOST_SPANS = 2880;
    /// What the curve covers while it does not follow a stream
    static constexpr int WINDOW_SECONDS = 900;
    /// The shortest stretch the curve is drawn over, so a stream that just
    /// started does not give it a jumping scale
    static constexpr int SHORTEST_SECONDS = 300;
    /// How wide it would like to be and how tall it is, in unscaled pixels -
    /// the header is 28 high, and the curve gets as much of it as it can
    static constexpr int WIDE = 190;
    static constexpr int HEIGHT = 28;
    /// Room under the line of time for "10 min", "1h" and the like
    static constexpr int LABEL_ROOM = 9;
    /// Room before the curve, between it and the title
    static constexpr int LEFT_ROOM = 2;
    /// Room after it, before the chat mode
    static constexpr int RIGHT_ROOM = 6;
    /// How narrow the curve itself gets in a small split
    static constexpr int NARROWEST = 36;

    /// How a mark on the line of time is labelled: "10 min", "1h", "1h30"
    static QString timeLabel(qint64 seconds);

    explicit ActivityGraph(QWidget *parent);

    /// Counts the messages of @a channel from now on
    void setChannel(const ChannelPtr &channel);

    /// Adds the spans that have passed since the last look, empty. The
    /// curve does this by itself every half minute.
    void catchUp();

    /// Notes that the channel now streams @a category. Where that differs
    /// from what it streamed before, the curve gets an upright line.
    void noteCategory(const QString &category);

    /// Asks the channel what it streams and how long it has been live -
    /// a stream of its own starts the counting over
    void checkStream();

    /// Draws the curve over the stream @a id, which began at @a start. A
    /// stream other than the one it was following starts it over.
    void followStream(const QString &id, const QDateTime &start);

    /// Stops following a stream - the curve covers the last quarter of an
    /// hour again, as it does for a channel that is not live
    void unfollowStream();

    /// What the tooltip says: how lively the chat was, over what stretch,
    /// and where the channel changed what it streams
    QString description() const;

    /// Where the curve gets the time from. Tests hand it their own, so a
    /// stream of hours can pass in a moment.
    void setClock(std::function<QDateTime()> clock);

    /// How wide the header wants it, in pixels - 0 for its own width
    void setWantedWidth(int pixels);
    /// How wide it is when nobody asks for more or less
    int ownWidth() const;

    /// A made-up quarter hour of chat, for the preview in the settings
    void showSample();

    /// As wide as it would like to be
    QSize sizeHint() const override;
    /// As narrow as it may get when the split is small
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void scaleChangedEvent(float scale) override;
    /// The text is built when it is about to be shown, so it is never a
    /// half minute out of date
    bool event(QEvent *event) override;

private:
    QDateTime now() const;
    void updateTooltip();

    /// The span the messages of this moment are counted in, adding the
    /// spans that have passed since the last one
    size_t currentSpan();

    /// From when to when the curve is drawn
    std::pair<QDateTime, QDateTime> window() const;

    /// How far apart the ticks under the curve stand, in seconds, for a
    /// curve covering @a seconds
    static int tickSeconds(qint64 seconds);

    /// Messages per half minute, the first one starting at spansStart_
    std::vector<int> spans_;
    QDateTime spansStart_;

    /// What the channel changed to, and when
    std::vector<std::pair<QDateTime, QString>> changes_;
    /// What it streams now, to notice a change
    QString category_;
    /// When the stream went live, invalid while the channel is not live
    QDateTime streamStart_;
    /// Which stream that is, so a new one starts the counting over
    QString streamId_;

    std::function<QDateTime()> clock_;
    int wantedWidth_{};
    ChannelPtr channel_;
    pajlada::Signals::SignalHolder connections_;
    QTimer timer_;
};

}  // namespace chatterino
