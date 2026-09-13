// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

#include <memory>

namespace chatterino {

class Image;

/// Profile pictures for Twitch names written as @name in highlight captions,
/// so a caption can show at a glance which channels someone moderates. Each
/// name is looked up on Twitch once; until the answer is in, and for a name
/// that is no Twitch user, the caption shows the name as text.
namespace captionavatars {

/// The login @a word stands for when it is written as a Twitch name -
/// "@zarbex" gives "zarbex" - otherwise an empty string
QString loginOf(const QString &word);

/// The picture of @a login, or null while it is not known. Asking about a
/// name not seen before starts looking it up.
std::shared_ptr<Image> image(const QString &login);

/// The name as Twitch writes it, once looked up - otherwise @a login
QString displayName(const QString &login);

/// Looks up the Twitch names in @a captions, so their pictures are ready by
/// the time a highlighted message comes in
void prefetch(const QStringList &captions);

/// Takes a name's picture as known, without asking Twitch
void remember(const QString &login, const QString &displayName,
              std::shared_ptr<Image> image);

}  // namespace captionavatars

}  // namespace chatterino
