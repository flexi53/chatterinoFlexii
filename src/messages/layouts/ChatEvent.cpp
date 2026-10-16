// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "messages/layouts/ChatEvent.hpp"

#include "messages/Message.hpp"
#include "messages/MessageFlag.hpp"
#include "singletons/Settings.hpp"

namespace chatterino {

ChatEvent eventOf(const Message &message)
{
    const auto &flags = message.flags;
    const auto &text = message.messageText;

    if (flags.has(MessageFlag::Timeout))
    {
        return text.contains(u"banned", Qt::CaseInsensitive)
                   ? ChatEvent::Ban
                   : ChatEvent::Timeout;
    }
    if (flags.has(MessageFlag::Untimeout))
    {
        return ChatEvent::None;
    }
    // Every notice Twitch sends about a user - subs, gifts, raids,
    // announcements - carries the one flag, so the text tells them apart
    if (flags.has(MessageFlag::Subscription))
    {
        if (text.contains(u"raider", Qt::CaseInsensitive))
        {
            return ChatEvent::Raid;
        }
        if (text.startsWith(u"Announcement"))
        {
            return ChatEvent::Announcement;
        }
        if (text.contains(u"gift", Qt::CaseInsensitive))
        {
            return ChatEvent::Gift;
        }
        return ChatEvent::Sub;
    }
    if (flags.has(MessageFlag::CheerMessage))
    {
        return ChatEvent::Bits;
    }
    if (flags.has(MessageFlag::RedeemedChannelPointReward))
    {
        return ChatEvent::Redeem;
    }
    if (flags.has(MessageFlag::WatchStreak))
    {
        return ChatEvent::Streak;
    }
    return ChatEvent::None;
}

std::vector<ChatEvent> allEvents()
{
    return {ChatEvent::Sub,     ChatEvent::Gift,         ChatEvent::Raid,
            ChatEvent::Announcement, ChatEvent::Timeout, ChatEvent::Ban,
            ChatEvent::Bits,    ChatEvent::Redeem,       ChatEvent::Streak};
}

QString eventKey(ChatEvent event)
{
    switch (event)
    {
        case ChatEvent::Sub:
            return QStringLiteral("sub");
        case ChatEvent::Gift:
            return QStringLiteral("gift");
        case ChatEvent::Raid:
            return QStringLiteral("raid");
        case ChatEvent::Announcement:
            return QStringLiteral("announcement");
        case ChatEvent::Timeout:
            return QStringLiteral("timeout");
        case ChatEvent::Ban:
            return QStringLiteral("ban");
        case ChatEvent::Bits:
            return QStringLiteral("bits");
        case ChatEvent::Redeem:
            return QStringLiteral("redeem");
        case ChatEvent::Streak:
            return QStringLiteral("streak");
        case ChatEvent::None:
        default:
            return {};
    }
}

QString eventName(ChatEvent event)
{
    switch (event)
    {
        case ChatEvent::Sub:
            return QStringLiteral("Subs");
        case ChatEvent::Gift:
            return QStringLiteral("Gifts");
        case ChatEvent::Raid:
            return QStringLiteral("Raids");
        case ChatEvent::Announcement:
            return QStringLiteral("Ankündigungen");
        case ChatEvent::Timeout:
            return QStringLiteral("Timeouts");
        case ChatEvent::Ban:
            return QStringLiteral("Banns");
        case ChatEvent::Bits:
            return QStringLiteral("Bits");
        case ChatEvent::Redeem:
            return QStringLiteral("Eingelöste Punkte");
        case ChatEvent::Streak:
            return QStringLiteral("Watch-Streaks");
        case ChatEvent::None:
        default:
            return {};
    }
}

bool eventMarked(ChatEvent event)
{
    if (event == ChatEvent::None || !getSettings()->eventSymbols)
    {
        return false;
    }
    const auto off = getSettings()->eventSymbolsOff.getValue().split(
        u',', Qt::SkipEmptyParts);
    return !off.contains(eventKey(event));
}

QString eventSymbol(ChatEvent event)
{
    switch (event)
    {
        case ChatEvent::Sub:
            return QStringLiteral("⭐");
        case ChatEvent::Gift:
            return QStringLiteral("🎁");
        case ChatEvent::Raid:
            return QStringLiteral("🚀");
        case ChatEvent::Announcement:
            return QStringLiteral("📣");
        case ChatEvent::Timeout:
            return QStringLiteral("⏱️");
        case ChatEvent::Ban:
            return QStringLiteral("🔨");
        case ChatEvent::Bits:
            return QStringLiteral("💎");
        case ChatEvent::Redeem:
            return QStringLiteral("🎟️");
        case ChatEvent::Streak:
            return QStringLiteral("🔥");
        case ChatEvent::None:
        default:
            return {};
    }
}

QColor eventColor(ChatEvent event)
{
    switch (event)
    {
        case ChatEvent::Sub:
            return {0x91, 0x46, 0xff};
        case ChatEvent::Gift:
            return {0xe0, 0x5a, 0xc8};
        case ChatEvent::Raid:
            return {0xff, 0x8c, 0x1a};
        case ChatEvent::Announcement:
            return {0x1e, 0x90, 0xff};
        case ChatEvent::Timeout:
            return {0xe0, 0xa0, 0x40};
        case ChatEvent::Ban:
            return {0xe0, 0x50, 0x50};
        case ChatEvent::Bits:
            return {0x3c, 0xc8, 0xe6};
        case ChatEvent::Redeem:
            return {0x2e, 0xc4, 0x9a};
        case ChatEvent::Streak:
            return {0xff, 0x6a, 0x2a};
        case ChatEvent::None:
        default:
            return {};
    }
}

}  // namespace chatterino
