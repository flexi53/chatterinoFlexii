// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/moderation/StepEscalation.hpp"
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
/// had a message deleted or been timed out - or when nobody acts and they
/// flood as much again.
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

    /// Whether the message is nothing but emotes - not a word in it
    static bool onlyEmotes(const Message &message);

    /// The most messages one alert deletes, however many were counted
    static constexpr int MOST_DELETED = 30;

    /// How many messages the delete button takes down, for the number set
    /// under Mod-Assistent -> Emote-Spam. Nothing set means all of them,
    /// and never more than MOST_DELETED - deleting a whole afternoon of
    /// them one by one would take a while.
    static int deleteLimit(int setting);

    /// What put the case in front of the moderator
    enum class Reason {
        /// Nothing did
        None,
        /// Enough emotes added up over the counting window
        Window,
        /// This one message alone had that many emotes
        SingleMessage,
        /// That many messages in a row held nothing but emotes
        Streak,
    };

    /// The numbers the alert goes off at, as set under Mod-Assistent ->
    /// Emote-Spam. A zero means that rule is switched off.
    struct Thresholds {
        /// Emotes added up over the window
        int window = 0;
        /// Emotes in one message
        int single = 0;
        /// Messages in a row holding nothing but emotes
        int streak = 0;

        /// What is set right now
        static Thresholds fromSettings();
    };

    /// Why the alert should go off for a chatter who has @a total emotes
    /// within the window, @a inMessage of them in the message that just
    /// came, and @a streak messages in a row holding nothing but emotes
    static Reason reasonFor(const Thresholds &thresholds, int total,
                            int inMessage, int streak);

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
        /// How many of their messages in a row held nothing but emotes
        int streak = 0;
        StepEscalation escalation;
        /// The messages the alert would delete
        QStringList pendingIds;
        QDateTime lastActivity;
    };

    QHash<QString, UserState> users_;
};

}  // namespace chatterino
