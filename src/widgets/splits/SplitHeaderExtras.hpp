// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QPixmap>
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

    HeaderPicture(Shape shape, QWidget *parent);

    /// Shows @a picture; a null one hides it
    void setPicture(const QPixmap &picture);

protected:
    void paintEvent(QPaintEvent *event) override;
    void scaleChangedEvent(float scale) override;

private:
    Shape shape_;
    QPixmap picture_;
};

/// How lively the chat was over the last ten minutes, as a small curve in
/// the split header - see Look -> Tabs
class ActivityGraph : public BaseWidget
{
public:
    explicit ActivityGraph(QWidget *parent);

    /// Counts the messages of @a channel from now on
    void setChannel(const ChannelPtr &channel);

protected:
    void paintEvent(QPaintEvent *event) override;
    void scaleChangedEvent(float scale) override;

private:
    /// Every half minute: a new, empty span at the end
    void shift();
    void updateTooltip();

    /// Messages per half minute, oldest first
    std::array<int, 20> counts_{};
    ChannelPtr channel_;
    pajlada::Signals::SignalHolder connections_;
    QTimer timer_;
};

}  // namespace chatterino
