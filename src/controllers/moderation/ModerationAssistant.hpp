// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace chatterino {

/// What the moderation assistant does in a channel
enum class ModAssistMode : std::uint8_t {
    /// Collects nothing and suggests nothing
    Off,
    /// Collects cases, suggests nothing
    Learn,
    /// Collects cases and suggests an action for similar messages
    Suggest,
};

/// One timeout or ban and what the user wrote before it
struct ModCase {
    QDateTime time;
    QString moderator;
    QString user;
    /// Length of the timeout in seconds, 0 for a ban
    int seconds = 0;
    QString reason;
    /// The user's last messages before the action, oldest first
    QStringList messages;
};

/// What the assistant would do about a message
struct ModSuggestion {
    /// Length of the timeout in seconds, 0 for a ban
    int seconds = 0;
    /// How many past cases resemble the message
    int similarCases = 0;
    /// The actions those cases ended in, e.g. "10m x7, 1h x2"
    QString spread;
};

/// Learns from the timeouts and bans moderators hand out in a channel, and
/// suggests an action when someone writes something like what got others
/// timed out. It only ever suggests - acting on a suggestion is left to the
/// user.
///
/// Which channels it runs in lives in the settings; the cases themselves are
/// kept per channel in the misc directory.
class ModerationAssistant
{
public:
    static ModerationAssistant &instance();

    ModAssistMode mode(const QString &channel) const;
    /// Must be called from the GUI thread
    void setMode(const QString &channel, ModAssistMode mode);

    /// Adds a case if the channel is collecting. Cases without any messages
    /// to learn from are dropped.
    void record(const QString &channel, ModCase modCase);

    /// All cases of a channel, newest first
    std::vector<ModCase> cases(const QString &channel) const;
    void removeCase(const QString &channel, const QDateTime &time,
                    const QString &user);

    /// A suggestion for a message, if the channel is suggesting and enough
    /// past cases resemble it
    std::optional<ModSuggestion> suggest(const QString &channel,
                                         const QString &text) const;

    /// A live chat message. Opens a suggestion for it if the channel is
    /// suggesting and enough earlier cases resemble it. @a badges is the raw
    /// badges tag.
    void onMessage(const QString &channel, const QString &login,
                   const QString &displayName, const QString &text,
                   const QString &badges);

    /// Reads timeouts and bans out of the channel's chat logs.
    /// @returns how many new cases were added, or -1 if there are no logs
    int importFromLogs(const QString &channel);

private:
    ModerationAssistant() = default;

    struct Entry {
        ModCase modCase;
        /// The words of each message, prepared for matching
        QList<QSet<QString>> words;
    };

    struct ChannelData {
        bool loaded = false;
        /// Oldest first
        std::vector<Entry> entries;
    };

    /// Loads a channel's cases on first use. The caller holds mutex_.
    ChannelData &data(const QString &channel) const;
    /// The caller holds mutex_
    void save(const QString &channel, const ChannelData &data) const;
    /// @returns false if the case was already known. The caller holds mutex_.
    static bool add(ChannelData &data, ModCase modCase);

    mutable std::mutex mutex_;
    mutable QHash<QString, ChannelData> channels_;
};

}  // namespace chatterino
