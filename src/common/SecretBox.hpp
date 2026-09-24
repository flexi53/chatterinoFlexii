// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>
#include <QString>

/// Locks a secret to this computer. Used where the secrets are kept beside
/// the settings instead of in the system's keychain: the file alone is of no
/// use on another machine, and a stray backup gives nothing away. It does
/// not protect against a program running as you here - that program could
/// ask this one just as well. See Badges -> "ohne Schlüsselbund".
namespace chatterino::secretbox {

/// The key material this computer is known by
QByteArray machineKey();

/// A fresh salt, kept beside the secrets it locks
QByteArray freshSalt();

/// @a value locked with @a salt and @a key, as text fit for a JSON file.
/// Empty in, empty out.
QString hide(const QString &value, const QByteArray &salt,
             const QByteArray &key);

/// What hide() made, back in the clear - empty when it was written on
/// another computer, or when the text is not one of ours
QString reveal(const QString &hidden, const QByteArray &salt,
               const QByteArray &key);

}  // namespace chatterino::secretbox
