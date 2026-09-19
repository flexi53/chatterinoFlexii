// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "messages/layouts/MessageRole.hpp"

#include "controllers/highlights/HighlightBadge.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/TwitchBadge.hpp"

#include <algorithm>

namespace chatterino {

namespace {

ChatRole roleOfBadge(const QString &key)
{
    if (key == u"broadcaster")
    {
        return ChatRole::Broadcaster;
    }
    if (key == u"moderator" || key == u"lead_moderator")
    {
        return ChatRole::Moderator;
    }
    if (key == u"vip")
    {
        return ChatRole::Vip;
    }
    if (key == u"subscriber" || key == u"founder")
    {
        return ChatRole::Subscriber;
    }
    return ChatRole::None;
}

/// The badge everyone with @a role has
QString plainBadge(ChatRole role)
{
    switch (role)
    {
        case ChatRole::Broadcaster:
            return QStringLiteral("broadcaster");
        case ChatRole::Moderator:
            return QStringLiteral("moderator");
        case ChatRole::Vip:
            return QStringLiteral("vip");
        case ChatRole::Subscriber:
            return QStringLiteral("subscriber");
        case ChatRole::None:
        default:
            return {};
    }
}

std::optional<QColor> highlightFor(
    const TwitchBadge &badge, const std::vector<HighlightBadge> &highlights)
{
    for (const auto &highlight : highlights)
    {
        if (highlight.isMatch(badge) && highlight.getColor() &&
            highlight.getColor()->isValid())
        {
            auto color = *highlight.getColor();
            color.setAlpha(255);
            return color;
        }
    }
    return std::nullopt;
}

}  // namespace

ChatRole roleOf(const Message &message)
{
    auto role = ChatRole::None;
    for (const auto &badge : message.twitchBadges)
    {
        role = std::max(role, roleOfBadge(badge.key_));
    }
    return role;
}

QColor stripeColor(const Message &message,
                   const std::vector<HighlightBadge> &highlights,
                   const RoleColors &colors)
{
    const auto role = roleOf(message);
    if (role == ChatRole::None)
    {
        return {};
    }

    for (const auto &badge : message.twitchBadges)
    {
        if (roleOfBadge(badge.key_) == role)
        {
            if (auto color = highlightFor(badge, highlights))
            {
                return *color;
            }
        }
    }
    if (auto color = badgeStripeColor(role, highlights))
    {
        return *color;
    }

    switch (role)
    {
        case ChatRole::Broadcaster:
            return colors.broadcaster;
        case ChatRole::Moderator:
            return colors.moderator;
        case ChatRole::Vip:
            return colors.vip;
        case ChatRole::Subscriber:
            return colors.subscriber;
        case ChatRole::None:
        default:
            return {};
    }
}

std::optional<QColor> badgeStripeColor(
    ChatRole role, const std::vector<HighlightBadge> &highlights)
{
    if (role == ChatRole::None)
    {
        return std::nullopt;
    }
    return highlightFor(TwitchBadge(plainBadge(role), QStringLiteral("1")),
                        highlights);
}

}  // namespace chatterino
