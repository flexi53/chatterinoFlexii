// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

#include <vector>

namespace chatterino::usernotes {

/// The mark that stands in front of the name of someone you noted something
/// about
inline const QString MARK = QStringLiteral("✎");

/// What you noted about the user with @a userId, empty when nothing
QString noteFor(const QString &userId);

/// A note as one line: its first line, cut at @a most letters with an
/// ellipsis. Notes are markdown and may run over many lines; a list needs
/// one.
QString oneLine(const QString &note, int most = 80);

struct Entry {
    /// The Twitch user id the note is kept under
    QString userId;
    QString note;
};

/// Everyone you noted something about, in no particular order
std::vector<Entry> all();

}  // namespace chatterino::usernotes
