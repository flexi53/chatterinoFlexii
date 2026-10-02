// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

#include <memory>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;

/// ChattiFlexii: the chats held at the bottom of the window. One list for
/// all of them - what is in it stands under the tabs, whichever tab is open
/// and in which window. That is what one otherwise keeps a second window
/// open for.
namespace pinnedsplits {

/// How a chat is written down: "twitch:forsen", or one of the channels of
/// its own - "mentions", "live", "automod", "whispers"
QStringList held();
void setHeld(const QStringList &chats);

/// What @a channel is called in that list - empty where it cannot be held
QString nameOf(const ChannelPtr &channel);
/// How an entry reads on the settings page
QString label(const QString &entry);

bool contains(const QString &entry);
void add(const QString &entry);
void remove(const QString &entry);

}  // namespace pinnedsplits

}  // namespace chatterino
