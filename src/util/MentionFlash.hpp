// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

/// What happens to a message when someone calls you by name: the shape of
/// the flashing, and the question of whether a message is meant for you.
/// Kept apart from the chat widget so both can be looked at on their own.
/// See Aussehen -> Chat -> "Erwähnungen kurz aufblinken lassen".
namespace chatterino::mentionflash {

/// How long the flashing lasts altogether, and how many turns fit in it
constexpr int LENGTH_MS = 900;
constexpr double FLASHES = 3.0;

/// How thick the tint lies over the message at the height of a flash. The
/// theme's own value belongs to the gentle glow of a message jumped to and
/// is far too thin to be noticed in passing.
constexpr int ALPHA = 150;

/// How much of the tint is gone at @a progress: 0 means it lies on in full,
/// 1 that nothing of it is left. Begins and ends at nothing, with FLASHES
/// turns in between, each one a little weaker than the one before.
double shapeAt(double progress);

/// Whether @a text calls @a name by name - that is what being pinged means
bool namedIn(const QString &text, const QString &name);

}  // namespace chatterino::mentionflash
