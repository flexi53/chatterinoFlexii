// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/splits/PinnedSplits.hpp"

#include "common/Channel.hpp"
#include "singletons/Settings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace chatterino::pinnedsplits {

namespace {

/// What was kept for every held chat, by name
QJsonObject kept()
{
    const auto value = getSettings()->pinnedSplitsState.getValue();
    if (value.isEmpty())
    {
        return {};
    }
    return QJsonDocument::fromJson(value.toUtf8()).object();
}

void keep(const QJsonObject &all)
{
    getSettings()->pinnedSplitsState.setValue(
        all.isEmpty() ? QString()
                      : QString::fromUtf8(
                            QJsonDocument(all).toJson(QJsonDocument::Compact)));
}

}  // namespace

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

State stateOf(const QString &entry)
{
    State state;
    const auto mine = kept().value(entry).toObject();
    if (mine.isEmpty())
    {
        return state;
    }

    state.moderationMode = mine.value("moderationMode").toBool(false);
    state.showActivity = mine.value("activityGraph").toBool(true);
    for (const auto &id : mine.value("filters").toArray())
    {
        const auto uuid = QUuid::fromString(id.toString());
        if (!uuid.isNull())
        {
            state.filters.append(uuid);
        }
    }
    if (mine.contains("checkSpelling"))
    {
        state.checkSpelling = mine.value("checkSpelling").toBool();
    }
    return state;
}

void setStateOf(const QString &entry, const State &state)
{
    if (entry.isEmpty())
    {
        return;
    }

    QJsonObject mine;
    // Only what was moved away from the plain state is written down, so a
    // list nobody touched stays empty
    if (state.moderationMode)
    {
        mine["moderationMode"] = true;
    }
    if (!state.showActivity)
    {
        mine["activityGraph"] = false;
    }
    if (!state.filters.isEmpty())
    {
        QJsonArray filters;
        for (const auto &id : state.filters)
        {
            filters.append(id.toString(QUuid::WithBraces));
        }
        mine["filters"] = filters;
    }
    if (state.checkSpelling)
    {
        mine["checkSpelling"] = *state.checkSpelling;
    }

    auto all = kept();
    if (mine.isEmpty())
    {
        all.remove(entry);
    }
    else
    {
        all[entry] = mine;
    }
    keep(all);
}

void forgetUnheld()
{
    const auto chats = held();
    auto all = kept();
    bool changed = false;
    for (const auto &entry : all.keys())
    {
        if (!chats.contains(entry))
        {
            all.remove(entry);
            changed = true;
        }
    }
    if (changed)
    {
        keep(all);
    }
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
