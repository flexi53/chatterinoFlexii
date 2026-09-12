// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>

namespace chatterino {

class RepeatSpamPopup;

/// Spots a chatter sending the same message over and over and puts the case
/// in front of the moderator with the timeout that fits it: 30 seconds for
/// three in a row, a minute for going at it again after a timeout.
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

    /// Someone - anyone - timed out or banned @a login
    void onTimeout(const QString &channel, const QString &login);

private:
    RepeatSpamDetector() = default;

    struct Said {
        QDateTime time;
        QString text;
        QString normalised;
    };

    struct UserState {
        QList<Said> recent;
        /// The message they were flagged for, normalised
        QString flaggedText;
        /// Timeouts they have served since being flagged
        int timeouts = 0;
        QDateTime lastActivity;
    };

    void showAlert(const QString &channel, const QString &login,
                   const QString &displayName, const UserState &state,
                   int seconds, bool again);

    QHash<QString, UserState> users_;
    QHash<QString, QPointer<RepeatSpamPopup>> popups_;
};

}  // namespace chatterino
