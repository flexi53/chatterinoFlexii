// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QTimer>

#include <array>
#include <memory>

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

/// How lively the chat was over the last quarter of an hour, as a small
/// curve in the split header - see Look -> Tabs. Under the curve a line
/// with a tick per minute says how far back it reaches, and where the
/// channel changed what it streams there is an upright line.
class ActivityGraph : public BaseWidget
{
public:
    /// How long one column stands for
    static constexpr int SPAN_SECONDS = 30;
    /// How many columns there are - 30 half minutes make a quarter hour
    static constexpr size_t SPANS = 30;

    explicit ActivityGraph(QWidget *parent);

    /// Counts the messages of @a channel from now on
    void setChannel(const ChannelPtr &channel);

    /// Moves the curve on by one span, so the newest one starts empty.
    /// Happens by itself every half minute.
    void shift();

    /// Notes that the channel now streams @a category. Where that differs
    /// from what it streamed before, the curve gets an upright line.
    void noteCategory(const QString &category);

    /// What the tooltip says: how lively the chat was and where the
    /// channel changed what it streams
    QString description() const;

    /// As wide as it would like to be - the whole quarter hour
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
    void updateTooltip();
    /// Asks the channel what it streams now and notes a change
    void checkCategory();

    /// Messages per half minute, oldest first
    std::array<int, SPANS> counts_{};
    /// What the channel changed to in that span, empty where it did not
    std::array<QString, SPANS> changes_;
    /// What it streams now, to notice a change
    QString category_;
    ChannelPtr channel_;
    pajlada::Signals::SignalHolder connections_;
    QTimer timer_;
};

}  // namespace chatterino
