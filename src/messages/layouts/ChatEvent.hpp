// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QString>

#include <vector>

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

/// Every kind there is, in the order they are offered in the settings
std::vector<ChatEvent> allEvents();

/// What a kind is kept under in the settings, as "bits"
QString eventKey(ChatEvent event);

/// What a kind is called on the settings page, as "Bits"
QString eventName(ChatEvent event);

/// Whether marking is switched on for @a event - Look -> Chat, Ereignisse,
/// switched on as a whole and this kind not left out
bool eventMarked(ChatEvent event);

/// The symbol shown in front of it
QString eventSymbol(ChatEvent event);

/// Its colour, for the stripe at the left edge
QColor eventColor(ChatEvent event);

}  // namespace chatterino
