// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;

}  // namespace chatterino

/// Shared Chat: Twitch shows the chats of several channels as one, but each
/// channel is moderated on its own - a timeout given in one of them holds
/// only there. Where the user moderates more than one channel of a session,
/// these carry what was just done over to the others.
namespace chatterino::sharedchat {

/// What was done to the chatter
struct Deed {
    /// Seconds for a timeout, none for a ban
    std::optional<int> duration;
    /// Lifting a ban or timeout rather than giving one
    bool lifted = false;
    QString reason;
};

/// Of @a participants, those an action should reach as well: every one but
/// @a acted, and only those @a moderates says yes to. Keeps their order.
QStringList alsoIn(const QString &acted, const QStringList &participants,
                   const std::function<bool(const QString &)> &moderates);

/// What is said in the chat about a deed carried over to @a channelName
QString noteFor(const QString &channelName, const Deed &deed);

/// Carries @a deed, just done in the channel with the id @a actedID, over to
/// the other channels of its shared chat session that the user moderates as
/// well - as far as they are open here. Says in @a channel what came of it.
void carryOver(const ChannelPtr &channel, const QString &actedID,
               const QString &moderatorID, const QString &targetID,
               const Deed &deed);

}  // namespace chatterino::sharedchat
