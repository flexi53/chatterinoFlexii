// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/SharedChatActions.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "util/FormatTime.hpp"

namespace chatterino::sharedchat {

QStringList alsoIn(const QString &acted, const QStringList &participants,
                   const std::function<bool(const QString &)> &moderates)
{
    QStringList others;
    for (const auto &id : participants)
    {
        if (id.isEmpty() || id == acted || others.contains(id))
        {
            continue;
        }
        if (moderates && !moderates(id))
        {
            continue;
        }
        others.append(id);
    }
    return others;
}

QString noteFor(const QString &channelName, const Deed &deed)
{
    if (deed.lifted)
    {
        return QStringLiteral("Shared Chat: in #%1 ebenfalls aufgehoben.")
            .arg(channelName);
    }
    if (deed.duration)
    {
        return QStringLiteral("Shared Chat: in #%1 ebenfalls für %2 "
                              "getimeoutet.")
            .arg(channelName, formatTime(*deed.duration));
    }
    return QStringLiteral("Shared Chat: in #%1 ebenfalls gebannt.")
        .arg(channelName);
}

void carryOver(const ChannelPtr &channel, const QString &actedID,
               const QString &moderatorID, const QString &targetID,
               const Deed &deed)
{
    if (!getSettings()->sharedChatCarryOver)
    {
        return;
    }
    if (channel == nullptr || actedID.isEmpty() || moderatorID.isEmpty() ||
        targetID.isEmpty())
    {
        return;
    }

    getHelix()->getSharedChatSession(
        actedID,
        [channel, actedID, moderatorID, targetID,
         deed](const std::optional<HelixSharedChatSession> &session) {
            if (!session || session->participantIDs.size() < 2)
            {
                return;
            }

            // Only channels open here: whether the user moderates one they
            // are not in is nothing this can tell, and a timeout should
            // never land somewhere unseen
            const auto moderates = [](const QString &id) {
                auto other = getApp()->getTwitch()->getChannelOrEmptyByID(id);
                auto *twitch = dynamic_cast<TwitchChannel *>(other.get());
                return twitch != nullptr && !twitch->isEmpty() &&
                       (twitch->isMod() || twitch->isBroadcaster());
            };

            for (const auto &id :
                 alsoIn(actedID, session->participantIDs, moderates))
            {
                auto other = getApp()->getTwitch()->getChannelOrEmptyByID(id);
                const auto name = other->getName();

                if (deed.lifted)
                {
                    getHelix()->unbanUser(
                        id, moderatorID, targetID,
                        [channel, name, deed] {
                            channel->addSystemMessage(noteFor(name, deed));
                        },
                        [channel, name](auto /*error*/, const QString &text) {
                            channel->addSystemMessage(
                                QStringLiteral("Shared Chat: in #%1 ging es "
                                               "nicht - %2")
                                    .arg(name, text));
                        });
                    continue;
                }

                getHelix()->banUser(
                    id, moderatorID, targetID, deed.duration, deed.reason,
                    [channel, name, deed] {
                        channel->addSystemMessage(noteFor(name, deed));
                    },
                    [channel, name](auto /*error*/, const QString &text) {
                        channel->addSystemMessage(
                            QStringLiteral(
                                "Shared Chat: in #%1 ging es nicht - %2")
                                .arg(name, text));
                    });
            }
        },
        [] {
            // No session, or Twitch did not say - then there is nothing to
            // carry over
        });
}

}  // namespace chatterino::sharedchat
