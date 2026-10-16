// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/splits/SplitBanner.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QString>

#include <chrono>

class QLabel;
class QPushButton;

namespace chatterino {

class TwitchChannel;

/// ChattiFlexii: the raid the channel is setting up, under the split header
/// - who it goes to, how many go along and how long until it goes by
/// itself. Twitch tells every viewer about it. With the Twitch app login
/// under Twitch-Extras a moderator sends it at once or calls it off here.
class RaidBannerWidget : public SplitBanner
{
    Q_OBJECT

public:
    explicit RaidBannerWidget(QWidget *parent = nullptr);

    void setChannel(TwitchChannel *channel);

    /// Emitted whenever this widget becomes shown or hidden
    pajlada::Signals::NoArgSignal visibilityChanged;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void scaleChangedEvent(float newScale) override;

    void tickCountdown() override;
    /// A raid that went or was called off stays away once it folded away
    void autoHide() override;

private:
    void refresh();
    /// Says how a button went under the target, until the next raid
    void noteResult(const QString &text, bool error);

    TwitchChannel *channel_ = nullptr;
    QLabel *footerLabel_ = nullptr;
    QPushButton *goButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    bool loggedIn_ = false;
    /// Which raid is shown, so a new one starts over
    QString showing_;
    QString note_;
    bool noteIsError_ = false;

    pajlada::Signals::SignalHolder signalHolder_;
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
