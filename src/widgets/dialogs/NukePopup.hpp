// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Channel.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "widgets/BasePopup.hpp"

#include <QDateTime>
#include <QString>

#include <vector>

class QLabel;
class QListWidget;

namespace chatterino {

/// What /nuke found and what it would do about it. Nothing happens until
/// the button is pressed - as with every other window of ours, and all the
/// more so here, where one press reaches many people at once.
class NukePopup : public BasePopup
{
    Q_OBJECT

public:
    /// One chatter the phrase was found with
    struct Caught {
        QString login;
        QString displayName;
        /// The last thing they wrote that matched
        QString message;
        QDateTime when;
    };

    NukePopup(ChannelPtr channel, QString phrase, int seconds,
              std::vector<Caught> caught, QWidget *parent);

    /// Everyone in @a channel who wrote @a phrase over the last @a minutes -
    /// moderators, VIPs, the broadcaster and you are left out
    static std::vector<Caught> whoWrote(const ChannelPtr &channel,
                                        const QString &phrase, int minutes,
                                        const QString &ownLogin);

    /// Whether the badges of a message say the writer is beyond a timeout
    static bool beyondReach(const std::vector<TwitchBadge> &badges);

private:
    void give();

    ChannelPtr channel_;
    QString phrase_;
    int seconds_;
    std::vector<Caught> caught_;
};

}  // namespace chatterino
