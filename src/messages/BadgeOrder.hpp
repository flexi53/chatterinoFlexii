// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

/// The badges in front of a name, by where they come from: which there are,
/// what they are called, and in which order they stand. The order is yours
/// to change under Aussehen -> Chat; it holds for messages from then on.
namespace chatterino::badgeorder {

/// Twitch's own - roles, subscriptions, Prime, bits and the shared chat's
inline const QString FROM_TWITCH = QStringLiteral("twitch");
inline const QString FROM_CHATTERINO = QStringLiteral("chatterino");
inline const QString FROM_FFZ = QStringLiteral("ffz");
inline const QString FROM_BTTV = QStringLiteral("bttv");
inline const QString FROM_SEVENTV = QStringLiteral("seventv");
inline const QString FROM_HOMIES = QStringLiteral("homies");
inline const QString FROM_MOLTORINO = QStringLiteral("moltorino");
/// Uploaded on sonkerino.kuestenfunk.org
inline const QString FROM_SONKERINO = QStringLiteral("sonkerino");
/// The one someone uploaded under Chatti-Badge
inline const QString FROM_CHATTI = QStringLiteral("chatti");
/// ChattiFlexii-Nutzer or -Supporter - never both
inline const QString CHATTI_STATUS = QStringLiteral("chattiStatus");

/// All of them, in the order they stood in before anyone moved them
QStringList defaults();

/// The order they stand in now
QStringList order();

/// Keeps @a keys as the order; what is missing falls back to its usual place
void setOrder(const QStringList &keys);

/// What the source @a key is called on the settings page
QString nameOf(const QString &key);

/// Moves @a key one place towards the front (@a up) or the back, and keeps
/// the result. Says whether anything moved.
bool move(const QString &key, bool up);

}  // namespace chatterino::badgeorder
