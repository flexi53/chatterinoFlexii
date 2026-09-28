// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/splits/SplitBanner.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QString>

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
