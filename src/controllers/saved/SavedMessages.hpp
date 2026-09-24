// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;
struct Message;
using MessagePtr = std::shared_ptr<const Message>;

/// Notizen -> Gemerkte Nachrichten: right-click a message, pick "Merken",
/// and it lands in a tab of its own - with who wrote it, where and when.
/// Stays there over a restart until you throw it away.
class SavedMessages : public QObject
{
public:
    static SavedMessages &instance();

    /// The channel its tab shows, "/gemerkt"
    ChannelPtr channel() const;

    /// Opens the tab in the main window, or shows it when it is there
    static void openTab();

    struct Entry {
        /// What this one is known by, so it can be thrown away again
        QString id;
        QDateTime when;
        /// The channel it was written in, without the #
        QString channel;
        /// The name as it was written, and the name to look them up by
        QString displayName;
        QString login;
        QString text;
        /// Addresses the message carried - those written in it, and those
        /// hidden behind a word like "Link copied", as the message about a
        /// fresh clip has them
        QStringList links;

        QJsonObject toJson() const;
        static Entry fromJson(const QJsonObject &object);
    };

    /// Keeps @a message from @a channelName, and opens the tab the first
    /// time. Does nothing when the very same message is already kept.
    void remember(const QString &channelName, const MessagePtr &message);
    /// Throws away what is kept under @a id
    void forget(const QString &id);
    /// Throws away everything
    void forgetAll();

    /// How many are kept, most recent last
    std::vector<Entry> kept() const;

    /// The message for @a entry, on @a background when that is a colour
    static MessagePtr messageFor(const Entry &entry,
                                 const QColor &background = {});

    /// The colour kept messages are laid on, none when colours are off
    static QColor color();

    /// The id @a message from @a channelName is kept under - the same
    /// message always gives the same one, so nothing is kept twice
    static QString idFor(const QString &channelName,
                         const MessagePtr &message);

    /// Every address @a message points at: those its text spells out and
    /// those that only sit behind one of its words - a clip's address, say.
    /// In the order they stand, none of them twice.
    static QStringList linksOf(const MessagePtr &message);

    /// How many are kept at most; the oldest goes when it is full
    static constexpr int MOST_KEPT = 500;

private:
    SavedMessages();

    /// Puts every kept message in the tab afresh
    void fill();

    void load();
    void save() const;

    ChannelPtr channel_;
    std::vector<Entry> kept_;
};

}  // namespace chatterino
