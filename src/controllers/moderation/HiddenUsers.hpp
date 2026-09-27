// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

/// People whose messages you would rather not read, hidden for you alone.
/// Nothing is sent to Twitch: they are neither blocked nor banned, they
/// simply do not arrive here any more. See /hide, /unhide and /hidden.
namespace chatterino::hiddenusers {

/// Everyone hidden, in small letters
QStringList all();

/// Whether @a login is one of them
bool hides(const QString &login);

/// Hides @a login - true when it was not hidden before
bool add(const QString &login);

/// Shows them again - true when they were hidden
bool remove(const QString &login);

/// The names in @a text, however they are written down
QStringList read(const QString &text);

}  // namespace chatterino::hiddenusers
