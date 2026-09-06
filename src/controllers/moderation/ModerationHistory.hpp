// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/QStringHash.hpp"

#include <QString>

#include <chrono>
#include <shared_mutex>
#include <unordered_map>

namespace chatterino {

/// How often a user has been warned, timed out and banned in a channel.
struct ModerationCounts {
    int warnings = 0;
    int timeouts = 0;
    int bans = 0;

    bool isEmpty() const
    {
        return this->warnings == 0 && this->timeouts == 0 && this->bans == 0;
    }

    /// The "0/2/1" shorthand Twitch's own moderator card uses
    QString toShortString() const;
};

/// Counts the moderation actions taken against each user, per channel.
///
/// Twitch has no public endpoint for a user's past punishments - its own
/// moderator card reads them from an internal API. So this builds the history
/// up from the `channel.moderate` events that arrive live, which means the
/// counts start at zero and only cover channels the user moderates.
class ModerationHistory
{
public:
    enum class Action {
        Warning,
        Timeout,
        Ban,
    };

    ModerationHistory();
    ~ModerationHistory();

    ModerationHistory(const ModerationHistory &) = delete;
    ModerationHistory(ModerationHistory &&) = delete;
    ModerationHistory &operator=(const ModerationHistory &) = delete;
    ModerationHistory &operator=(ModerationHistory &&) = delete;

    /// Records one action. Safe to call from any thread.
    void record(const QString &channelID, const QString &userID, Action action);

    /// What has been recorded against @a userID in @a channelID so far
    ModerationCounts counts(const QString &channelID,
                            const QString &userID) const;

    void save();

private:
    void load();
    static QString filePath();

    mutable std::shared_mutex mutex_;

    /// channel ID -> user ID -> counts. Guarded by mutex_
    std::unordered_map<QString, std::unordered_map<QString, ModerationCounts>>
        data_;

    bool dirty_ = false;
    std::chrono::steady_clock::time_point lastSave_;
};

}  // namespace chatterino
