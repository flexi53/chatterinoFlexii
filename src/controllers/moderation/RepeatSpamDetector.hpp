// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>

#include <vector>

namespace chatterino {

/// Spots a chatter sending the same message - or nearly the same - over and
/// over, and puts the case in front of the moderator with the timeout that
/// fits it: the first step for three in a row, the next one each time they
/// carry on after serving a timeout.
///
/// It only ever offers the timeout. Pressing the button is left to the user.
/// Everything here runs on the GUI thread.
class RepeatSpamDetector
{
public:
    static RepeatSpamDetector &instance();

    bool isEnabled(const QString &channel) const;
    void setEnabled(const QString &channel, bool enabled);

    /// A live chat message. @a badges is the raw badges tag.
    void onMessage(const QString &channel, const QString &login,
                   const QString &displayName, const QString &text,
                   const QString &badges, const QDateTime &time);

    /// Someone - anyone - timed out @a login for @a seconds, or banned them
    /// (0 seconds)
    void onTimeout(const QString &channel, const QString &login, int seconds);

    /// The timeouts offered at each step, in seconds. Never empty.
    static std::vector<int> steps();
    /// Reads a list like "30s, 1m, 5m". Empty if any part of it is not a
    /// duration.
    static std::vector<int> parseSteps(const QString &text);

private:
    RepeatSpamDetector() = default;

    struct Entry {
        QDateTime time;
        QString text;
        QString normalised;
        /// -1 for a message, otherwise the length of a timeout, 0 for a ban
        int timeoutSeconds = -1;
    };

    struct UserState {
        /// Their recent messages and the timeouts in between, oldest first
        QList<Entry> history;
        /// The message they were flagged for, normalised
        QString flaggedText;
        /// Timeouts they have served since being flagged
        int timeouts = 0;
        QDateTime lastActivity;
    };

    void showAlert(const QString &channel, const QString &login,
                   const QString &displayName, int seconds,
                   int timeoutsServed);

    QHash<QString, UserState> users_;
};

}  // namespace chatterino
