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
struct Message;
using MessagePtr = std::shared_ptr<const Message>;

/// Notizen -> Leute im Blick: a tab of its own that collects what the people
/// you picked write - in every channel you have open, with the channel
/// beside each line. So you see where they are about without watching every
/// tab yourself.
class WatchedPeople
{
public:
    static WatchedPeople &instance();

    /// The channel its tab shows, "/leute"
    ChannelPtr channel() const;

    /// Opens the tab in the main window, or shows it when it is there
    static void openTab();

    /// The logins picked, as they are kept: one per line, lowercase, without
    /// a leading @
    static QStringList people();
    /// Reads a written list - line by line or separated by commas
    static QStringList read(const QString &written);
    /// Writes @a people back the way the setting keeps them
    static QString write(const QStringList &people);

    /// Whether @a login is one of @a people
    static bool watches(const QString &login, const QStringList &people);
    /// Whether @a login is watched right now
    static bool watches(const QString &login);

    /// Adds @a login to the list, or takes it out again. Gives what holds
    /// afterwards.
    static bool toggle(const QString &login);

    /// Puts @a message from the channel @a channelName in the tab, when its
    /// writer is watched
    void onMessage(const QString &channelName, const MessagePtr &message);

private:
    WatchedPeople();

    ChannelPtr channel_;
};

}  // namespace chatterino
