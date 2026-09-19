// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QPixmap>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace chatterino {

class Image;

/// What Twitch says about an account, as far as the app needs it
struct TwitchProfile {
    QString login;
    QString displayName;
    /// The picture at Twitch's largest size, 300x300; empty for none
    QString pictureUrl;
    QDateTime createdAt;
};

/// Twitch profiles and their pictures, shared by everything that shows one:
/// tabs, captions in chat, the alert windows, the mod highlights list and the
/// user card. Names asked about within a moment of each other are looked up
/// together, a hundred at a time. What Twitch answered is kept on disk, so
/// after a restart the pictures are there at once - looked up again in the
/// background once it is a week old - and pictures come through the network
/// cache. A name Twitch gave no answer for - not logged in yet, no
/// connection - is asked again soon, and at once when the login is there.
namespace profilepictures {

/// The login @a word stands for when it is written as a Twitch name in a
/// caption - "@zarbex" gives "zarbex" - otherwise an empty string
QString loginOf(const QString &word);

/// The profile of @a login, once looked up. Asking about a name not seen
/// before starts looking it up.
std::optional<TwitchProfile> profile(const QString &login);

/// Calls @a done with the profile of @a login once it is known - right away
/// when it already is. While Twitch gives no answer it keeps waiting for the
/// next try; not at all for a name that is no Twitch user, or once
/// @a context is gone. GUI thread only.
void whenKnown(const QString &login, QObject *context,
               std::function<void(const TwitchProfile &)> done);

/// The picture of @a login for chat, or null while it is not known
std::shared_ptr<Image> image(const QString &login);

/// Calls @a done with the picture of @a login, at least @a side pixels
/// across, once it has loaded - never for an account without one, nor once
/// @a context is gone. GUI thread only.
void pixmap(const QString &login, int side, QObject *context,
            std::function<void(const QPixmap &)> done);

/// The name as Twitch writes it, once looked up - otherwise @a login
QString displayName(const QString &login);

/// Looks up @a logins ahead of time
void prefetch(const QStringList &logins);

/// Looks up the Twitch names written as @name in @a captions, so their
/// pictures are ready by the time a highlighted message comes in
void prefetchCaptions(const QStringList &captions);

/// Takes a profile as known, without asking Twitch
void remember(const TwitchProfile &profile,
              std::shared_ptr<Image> image = nullptr);

}  // namespace profilepictures

}  // namespace chatterino
