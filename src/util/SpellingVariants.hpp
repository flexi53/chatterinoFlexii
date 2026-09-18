// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QRegularExpression>
#include <QString>

#include <vector>

namespace chatterino::spelling {

/// Which ways of writing a word differently to find as well
struct Options {
    /// f0ll0w3r, $pam - digits and signs for letters
    bool leet = true;
    /// fоllower with a Cyrillic о, föllower - letters that look alike
    bool lookalikes = true;
    /// fooollower, folower - a letter stretched, or a double one left single
    bool stretched = true;
    /// f.o.l.l.o.w, f o l l o w - something between the letters
    bool separated = true;
    /// Only where it stands on its own, not inside a longer word
    bool wholeWord = true;
};

/// A regular expression that finds @a text however it is written, as far
/// as @a options go. It starts with a comment naming @a text, so it can be
/// told apart in the list of highlights. Empty when there is nothing to find.
///
/// Meant to be matched case-insensitively and with Unicode properties, the
/// way highlights are - see compile().
QString pattern(const QString &text, const Options &options);

/// @a pattern compiled the way a highlight compiles it
QRegularExpression compile(const QString &pattern);

struct Example {
    QString text;
    /// Why it is an example, where that does not show - a Cyrillic letter
    /// looks the same as a Latin one
    QString note;
};

/// A few ways of writing @a text that the pattern for it finds, one for each
/// option switched on that changes something
std::vector<Example> examples(const QString &text, const Options &options);

}  // namespace chatterino::spelling
