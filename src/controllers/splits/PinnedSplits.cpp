// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/splits/PinnedSplits.hpp"

#include "common/Channel.hpp"
#include "singletons/Settings.hpp"

namespace chatterino::pinnedsplits {

QStringList held()
{
    return getSettings()->pinnedSplits.getValue().split(',',
                                                        Qt::SkipEmptyParts);
}

void setHeld(const QStringList &chats)
{
    getSettings()->pinnedSplits.setValue(chats.join(','));
}

QString nameOf(const ChannelPtr &channel)
{
    if (channel == nullptr)
    {
        return {};
    }

    switch (channel->getType())
    {
        case Channel::Type::Twitch:
            return "twitch:" + channel->getName().toLower();
        case Channel::Type::TwitchMentions:
            return "mentions";
        case Channel::Type::TwitchLive:
            return "live";
        case Channel::Type::TwitchAutomod:
            return "automod";
        case Channel::Type::TwitchWhispers:
            return "whispers";
        default:
            // A channel of a plugin, or the empty one - nothing to hold on to
            return {};
    }
}

QString label(const QString &entry)
{
    if (entry.startsWith("twitch:"))
    {
        return entry.mid(7);
    }
    if (entry == "mentions")
    {
        return "Erwähnungen";
    }
    if (entry == "live")
    {
        return "Wer live ist";
    }
    if (entry == "automod")
    {
        return "AutoMod";
    }
    if (entry == "whispers")
    {
        return "Flüstern";
    }
    return entry;
}

bool contains(const QString &entry)
{
    return !entry.isEmpty() && held().contains(entry);
}

void add(const QString &entry)
{
    if (entry.isEmpty() || contains(entry))
    {
        return;
    }
    auto chats = held();
    chats.append(entry);
    setHeld(chats);
}

void remove(const QString &entry)
{
    auto chats = held();
    if (chats.removeAll(entry) > 0)
    {
        setHeld(chats);
    }
}

}  // namespace chatterino::pinnedsplits
