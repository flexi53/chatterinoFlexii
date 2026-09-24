// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

/// The little row of buttons right of the input field: which of them there
/// are, what they are called, and in which order they stand. The order is
/// yours to change under Buttons.
namespace chatterino::inputbuttons {

inline const QString MOD_ASSIST = QStringLiteral("modAssist");
inline const QString ALERT_MUTE = QStringLiteral("alertMute");
inline const QString CLEAR = QStringLiteral("clear");
inline const QString FOCUS = QStringLiteral("focus");
inline const QString FOLLOW = QStringLiteral("follow");
inline const QString BADGE = QStringLiteral("badge");
inline const QString CLIP = QStringLiteral("clip");
inline const QString EMOTE = QStringLiteral("emote");

/// All of them, in the order they stood in before anyone moved them
QStringList defaults();

/// The order they stand in now
QStringList order();

/// Keeps @a keys as the order; what is missing falls back to its usual place
void setOrder(const QStringList &keys);

/// What the button @a key is called on the settings page
QString nameOf(const QString &key);

/// Moves @a key one place towards the front (@a up) or the back, and keeps
/// the result. Says whether anything moved.
bool move(const QString &key, bool up);

}  // namespace chatterino::inputbuttons
