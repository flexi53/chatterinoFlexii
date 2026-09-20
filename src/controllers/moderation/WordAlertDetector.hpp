// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/moderation/StepEscalation.hpp"

#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace chatterino {

/// Watches for words the moderator has put on a list - "sybau" and the like -
/// and puts the case in front of them with a timeout to offer. A word is
/// found however it is written: sybau, syb4u, s.y.b.a.u, with a Cyrillic
/// letter in it. The list is set under Mod-Assistent -> Wörter.
///
/// It only ever offers the action. Pressing the button is left to the user.
/// Everything here runs on the GUI thread.
class WordAlertDetector
{
public:
    /// The step that deletes the message rather than timing anyone out
    static constexpr int DELETE = -1;

    static WordAlertDetector &instance();

    bool isEnabled(const QString &channel) const;
    void setEnabled(const QString &channel, bool enabled);

    /// A live chat message. @a badges is the raw badges tag.
    void onMessage(const QString &channel, const QString &login,
                   const QString &displayName, const QString &text,
                   const QString &badges, const QString &messageId);

    /// Someone deleted one of @a login's messages, or timed them out
    void onAction(const QString &channel, const QString &login);

    /// One word on the list and what finds it
    struct Watched {
        /// As it was typed in
        QString word;
        QRegularExpression pattern;
        /// What this word alone offers, empty where it follows the steps
        /// set for all of them
        std::vector<int> steps;
    };

    /// A word of the list found in a message
    struct Match {
        /// The word as it stands on the list
        QString word;
        /// How it was written in the message - "fеlix" with a Cyrillic е,
        /// say, where the list says "felix"
        QString asWritten;
        /// What this word offers, empty where it follows the general steps
        std::vector<int> steps;
    };

    /// The words on the list, ready to match with. Built once and kept
    /// until the list or the way of finding them changes.
    static const std::vector<Watched> &words();

    /// Reads a list of words, one per line, and builds what finds each of
    /// them. A line may name what that word alone offers, after an equals
    /// sign: "kys = 1d, bann". Lines starting with # are left out, as are
    /// empty ones.
    static std::vector<Watched> parseWords(const QString &text,
                                           bool variants = true,
                                           bool wholeWord = true);

    /// Writes the list back the way parseWords reads it
    static QString writeWords(const std::vector<Watched> &list);

    /// Which word of @a list the text holds, and how it was written.
    /// Nothing when none does.
    static std::optional<Match> find(const QString &text,
                                     const std::vector<Watched> &list);

    /// What is on offer at each step for the words that name nothing of
    /// their own: DELETE, a timeout length in seconds, or 0 for a ban.
    /// Never empty.
    static std::vector<int> steps();

    /// The durations the buttons of one word offer, in the order they are
    /// shown - what a word may be given instead of the general steps
    static const std::vector<int> &palette();

    /// How many messages the delete button takes down, for the number set
    /// under Mod-Assistent -> Wörter. Nothing set means all that were
    /// found, and never more than MOST_DELETED.
    static constexpr int MOST_DELETED = 10;
    static int deleteLimit(int setting);
    /// Reads a list like "löschen, 5m, 1d". Empty if any part of it is
    /// neither.
    static std::vector<int> parseSteps(const QString &text);

private:
    WordAlertDetector() = default;

    struct UserState {
        StepEscalation escalation;
        /// The messages the alert would delete
        QStringList pendingIds;
        QDateTime lastActivity;
    };

    QHash<QString, UserState> users_;
};

}  // namespace chatterino
