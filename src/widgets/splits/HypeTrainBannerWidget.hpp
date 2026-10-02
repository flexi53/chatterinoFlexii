// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/splits/SplitBanner.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QString>

#include <chrono>

class QLabel;

namespace chatterino {

class TwitchChannel;
class VoteBarWidget;

/// ChattiFlexii: the hype train going in the channel, under the split
/// header - which level it is on, how far that level has come and how long
/// it has left. Twitch tells every viewer about it, so this shows in every
/// channel.
class HypeTrainBannerWidget : public SplitBanner
{
    Q_OBJECT

public:
    explicit HypeTrainBannerWidget(QWidget *parent = nullptr);

    void setChannel(TwitchChannel *channel);

    /// Brings it back after it hid itself, or hides it - the button in the
    /// split header
    void toggleUserPinned();

    /// Emitted whenever this widget becomes shown or hidden
    pajlada::Signals::NoArgSignal visibilityChanged;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void scaleChangedEvent(float newScale) override;

    void tickCountdown() override;

private:
    void refresh();

    /// ChattiFlexii: Twitch says when this level runs out, never how long
    /// it had - so the bar measures against the longest time ever seen left
    /// on it, which is what it had when it first turned up here. Each level
    /// counts for itself: reaching the next one winds the clock up again,
    /// and the bar fills up with it.
    QString timedLevel_;
    std::chrono::milliseconds timedTotal_{0};
    /// What is left of this level's time, from 1 to 0 - below zero once the
    /// train is over and there is nothing left to run out
    double timeShareOf(const QString &level, std::chrono::milliseconds left);

    TwitchChannel *channel_ = nullptr;
    VoteBarWidget *bar_ = nullptr;
    QLabel *footerLabel_ = nullptr;
    bool userToggled_ = false;
    /// Which train is being shown, so a new one starts over
    QString showing_;

    pajlada::Signals::SignalHolder signalHolder_;
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
