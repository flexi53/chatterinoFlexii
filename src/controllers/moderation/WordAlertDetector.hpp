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
    };

    /// The words on the list, ready to match with. Built once and kept
    /// until the list or the way of finding them changes.
    static const std::vector<Watched> &words();

    /// Reads a list of words, one per line, and builds what finds each of
    /// them. Lines starting with # are left out, as are empty ones.
    static std::vector<Watched> parseWords(const QString &text,
                                           bool variants = true,
                                           bool wholeWord = true);

    /// Which word of @a list the text holds, empty when none does
    static QString found(const QString &text, const std::vector<Watched> &list);

    /// What is on offer at each step: DELETE, or a timeout length in
    /// seconds. Never empty.
    static std::vector<int> steps();
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
