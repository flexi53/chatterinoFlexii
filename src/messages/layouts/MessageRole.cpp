// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "messages/layouts/MessageRole.hpp"

#include "messages/Message.hpp"
#include "providers/twitch/TwitchBadge.hpp"

#include <algorithm>

namespace chatterino {

ChatRole roleOf(const Message &message)
{
    auto role = ChatRole::None;
    for (const auto &badge : message.twitchBadges)
    {
        const auto &key = badge.key_;
        auto found = ChatRole::None;
        if (key == u"broadcaster")
        {
            found = ChatRole::Broadcaster;
        }
        else if (key == u"moderator" || key == u"lead_moderator")
        {
            found = ChatRole::Moderator;
        }
        else if (key == u"vip")
        {
            found = ChatRole::Vip;
        }
        else if (key == u"subscriber" || key == u"founder")
        {
            found = ChatRole::Subscriber;
        }
        role = std::max(role, found);
    }
    return role;
}

}  // namespace chatterino
