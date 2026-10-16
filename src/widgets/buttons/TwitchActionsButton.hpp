// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QSize>
#include <QString>
#include <QTimer>

#include <optional>

namespace chatterino {

/// Your channel points in the channel its split shows, as a small pill
/// beside the input - the way Moltorino has it. A click opens the window
/// with the rewards, and for moderators the forms to start a poll or a
/// prediction. Without the Twitch app login under Twitch-Extras it shows
/// the coin alone.
class TwitchActionsButton : public Button
{
public:
    explicit TwitchActionsButton(BaseWidget *parent = nullptr);

    /// The channel whose points it shows
    void setChannel(const QString &channelLogin);

    /// The size the other buttons beside the input have, unscaled for the
    /// look - the pill is as high, and wider where the number needs it
    void setBaseSize(QSize size);

    /// "1.412.345" - the whole number, with the dots between thousands
    static QString pointsText(qint64 points);

protected:
    void paintContent(QPainter &painter) override;

private:
    void fetch();
    void refreshTooltip();
    /// Makes room for the number, or takes it back
    void fitWidth();

    QString channel_;
    std::optional<qint64> balance_;
    bool loggedIn_ = false;
    QSize baseSize_;
    QTimer refresh_;

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
