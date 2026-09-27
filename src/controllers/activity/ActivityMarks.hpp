// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QString>

#include <vector>

/// What happened in a channel while you were watching it, kept with the
/// moment it happened: where your name fell, where an alert window came up,
/// and where you moderated. The activity curve in the split header draws a
/// small mark for each of them, so a peak in the curve can be told apart -
/// lively chat, or trouble.
///
/// Only what comes in while the program runs is kept, and only for as long
/// as it runs - the curve knows nothing older either.
namespace chatterino::activitymarks {

enum class Kind {
    /// Someone called you by name
    Mention,
    /// An alert window came up
    Alert,
    /// You gave a timeout or a ban
    ModAction,
};

struct Mark {
    QDateTime when;
    Kind kind;
    /// Who it was about, for the tooltip
    QString who;
};

/// How many are kept per channel - a day of a busy chat fits in easily
constexpr size_t MOST_PER_CHANNEL = 300;

/// Notes that something happened in @a channel
void note(const QString &channel, Kind kind, const QString &who,
          QDateTime when = QDateTime::currentDateTimeUtc());

/// What happened in @a channel between @a from and @a to, oldest first
std::vector<Mark> marks(const QString &channel, const QDateTime &from,
                        const QDateTime &to);

/// What a mark is called on screen
QString nameOf(Kind kind);

/// Everything forgotten - for the tests
void forget();

}  // namespace chatterino::activitymarks
