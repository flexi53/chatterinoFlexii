// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

namespace chatterino {

struct Message;

/// Who wrote a message, as far as a role stripe tells it - see Look -> Chat
enum class ChatRole {
    None,
    Subscriber,
    Vip,
    Moderator,
    Broadcaster,
};

/// The highest role the badges of @a message show - a moderator who also
/// subscribes is a moderator
ChatRole roleOf(const Message &message);

}  // namespace chatterino
