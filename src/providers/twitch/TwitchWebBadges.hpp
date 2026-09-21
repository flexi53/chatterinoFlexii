// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>

class QObject;

/// Choosing which badge you wear, as the chat on twitch.tv lets you. Twitch
/// offers no way to do that to other apps, so this goes the way its own
/// website goes - with the login of your browser, which is kept in the
/// system's keychain and nowhere else: not in the settings, an export, a
/// backup or the sync between computers. Off until someone puts one in.
namespace chatterino::webbadges {

/// Whether a browser login can be kept here at all - only where the system
/// has a keychain, so not in the portable version
bool canStore();

/// What was pasted, cleaned up: "OAuth abc", quotes and spaces taken off
QString normalize(const QString &pasted);
/// Whether @a token looks like the "auth-token" cookie of twitch.tv
bool looksLikeToken(const QString &token);

/// Keeps @a token in the keychain. False when it does not look like one or
/// there is no keychain.
bool store(const QString &token);
void erase();
/// Hands the kept login to @a done - an empty one when there is none
void load(QObject *receiver, std::function<void(const QString &)> done);

/// One question to Twitch, as its website asks it, with @a variables for
/// what the question needs. @a done gets the whole answer, errors included;
/// @a failed only what went wrong on the way.
void ask(const QString &token, const QString &query,
         const QJsonObject &variables, QObject *caller,
         std::function<void(const QJsonObject &)> done,
         std::function<void(const QString &)> failed);

/// Checks with the kept login what can be done: who it belongs to, which
/// badges there are, and whether Twitch lets this app change one - by
/// choosing again the one that is chosen already, so nothing changes.
/// @a done gets a report to show.
void test(QObject *caller, std::function<void(const QString &report)> done);

}  // namespace chatterino::webbadges
