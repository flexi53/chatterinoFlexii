// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QPixmap>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

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

/// A badge that can be worn
struct Badge {
    QString setID;
    QString version;
    QString title;
    /// Where its picture is, twice the size it is shown at
    QString image;

    bool operator==(const Badge &other) const
    {
        return this->setID == other.setID && this->version == other.version;
    }
};

/// What can be chosen in one channel: the badges of this channel - sub,
/// bits, mod and the like - and those worn everywhere, with the one of each
/// that is worn now
struct Choices {
    std::vector<Badge> channel;
    std::vector<Badge> global;
    std::optional<Badge> channelWorn;
    std::optional<Badge> globalWorn;
    /// Set when there is nothing to show - no login, or Twitch said no
    QString problem;

    /// What is seen next to the name here: the channel's badge, or the
    /// global one when none of the channel's is worn
    std::optional<Badge> shown() const;
};

/// Reads Twitch's answer to the question fetchChoices asks
Choices parseChoices(const QJsonObject &answer);

/// What can be chosen in the channel with the id @a channelId
void fetchChoices(const QString &channelId, QObject *caller,
                  std::function<void(const Choices &)> done);

/// Twitch takes a moment before it tells what was just chosen - until then
/// the choice made here is what counts. @a remember keeps one, @a applyRecent
/// puts those younger than a few minutes over what Twitch said.
void remember(const QString &channelId, const Badge &badge, bool global,
              const QDateTime &now = QDateTime::currentDateTimeUtc());
void applyRecent(Choices &choices, const QString &channelId,
                 const QDateTime &now = QDateTime::currentDateTimeUtc());

/// Wears @a badge - in the channel with the id @a channelId, or everywhere
/// when @a global. @a done gets what went wrong, or nothing.
void choose(const QString &channelId, const Badge &badge, bool global,
            QObject *caller, std::function<void(const QString &)> done);

/// The picture at @a url, once it is there - kept for as long as the app
/// runs, as the same few badges come up again and again
void picture(const QString &url, QObject *caller,
             std::function<void(const QPixmap &)> done);

/// Checks with the kept login what can be done: who it belongs to, which
/// badges there are, and whether Twitch lets this app change one - by
/// choosing again the one that is chosen already, so nothing changes.
/// @a done gets a report to show.
void test(QObject *caller, std::function<void(const QString &report)> done);

}  // namespace chatterino::webbadges
