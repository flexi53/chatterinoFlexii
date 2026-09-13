// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "messages/Message.hpp"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <vector>

namespace chatterino {

/// Spots a chatter flooding the chat with emotes and puts the case in front
/// of the moderator with what fits: deleting the messages at first, a timeout
/// further on.
///
/// It adds up the emotes of a chatter's messages over a short time, counting
/// the messages where emotes outweigh words - so a string of short bursts
/// counts as much as one long wall. The next step comes once the chatter has
/// actually had a message deleted or been timed out.
///
/// It only ever offers the action. Pressing the button is left to the user.
/// Everything here runs on the GUI thread.
class EmoteSpamDetector
{
public:
    /// The step that deletes the messages rather than timing anyone out
    static constexpr int DELETE = -1;

    static EmoteSpamDetector &instance();

    bool isEnabled(const QString &channel) const;
    void setEnabled(const QString &channel, bool enabled);

    /// A live chat message. @a badges is the raw badges tag.
    void onMessage(const QString &channel, const MessagePtr &message,
                   const QString &badges);

    /// Someone deleted one of @a login's messages, or timed them out
    void onAction(const QString &channel, const QString &login);

    /// How many emotes the message counts for: all of them if emotes
    /// outweigh words, otherwise none. Cheers never count - they pay the
    /// streamer rather than fill the chat.
    static int emoteCount(const Message &message);

    /// What is on offer at each step: DELETE, or a timeout length in
    /// seconds. Never empty.
    static std::vector<int> steps();
    /// Reads a list like "delete, delete, 30s". Empty if any part of it is
    /// neither.
    static std::vector<int> parseSteps(const QString &text);

private:
    EmoteSpamDetector() = default;

    struct Counted {
        QDateTime time;
        int emotes = 0;
        QString id;
    };

    struct UserState {
        /// Their emote heavy messages within the counting window
        QList<Counted> recent;
        /// Deletions and timeouts they have had since their first alert
        int actions = 0;
        /// An alert went up and nothing has been done about it yet
        bool alerted = false;
        QDateTime lastAlert;
        /// The messages the open alert would delete
        QStringList pendingIds;
        QDateTime lastActivity;
    };

    QHash<QString, UserState> users_;
};

}  // namespace chatterino
