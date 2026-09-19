// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QString>

namespace chatterino {

struct Message;

/// What happened, for a message that tells of an event rather than being
/// written by someone - see Look -> Chat, Ereignisse
enum class ChatEvent {
    None,
    Sub,
    Gift,
    Raid,
    Announcement,
    Timeout,
    Ban,
    Bits,
    Redeem,
    Streak,
};

/// The event @a message tells of, if it tells of one
ChatEvent eventOf(const Message &message);

/// The symbol shown in front of it
QString eventSymbol(ChatEvent event);

/// Its colour, for the stripe at the left edge
QColor eventColor(ChatEvent event);

}  // namespace chatterino
