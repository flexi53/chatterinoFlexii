// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace chatterino {

namespace filters {
class Filter;
}  // namespace filters

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

    /// The filter that says the same as @a people, in the language of the
    /// filters page: (author.name == "a") || (author.name == "b")
    static QString expressionFor(const QStringList &people);

    /// What is wrong with @a expression, empty when it is fine. An empty
    /// expression is fine as well - then the list counts.
    static QString problemWith(const QString &expression);

    /// Puts @a message from @a channel in the tab, when it is one to keep -
    /// what the filter says, or, without a filter, who wrote it
    void onMessage(Channel *channel, const MessagePtr &message);

private:
    WatchedPeople();

    /// Builds the filter afresh from the setting; none when it is empty or
    /// does not hold up
    void rebuildFilter();

    ChannelPtr channel_;
    std::unique_ptr<filters::Filter> filter_;
};

}  // namespace chatterino
