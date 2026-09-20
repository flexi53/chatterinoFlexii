// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/AlertMute.hpp"

#include "singletons/Settings.hpp"

#include <QSet>
#include <QStringList>

namespace chatterino::alertmute {

namespace {

QSet<QString> muted()
{
    const auto value = getSettings()->modAlertMutedChannels.getValue();
    QSet<QString> channels;
    for (const auto &channel : value.split(',', Qt::SkipEmptyParts))
    {
        channels.insert(channel);
    }
    return channels;
}

}  // namespace

bool isMuted(const QString &channel)
{
    if (channel.isEmpty())
    {
        return false;
    }
    return muted().contains(channel.toLower());
}

void setMuted(const QString &channel, bool mute)
{
    if (channel.isEmpty())
    {
        return;
    }

    auto channels = muted();
    if (mute)
    {
        channels.insert(channel.toLower());
    }
    else
    {
        channels.remove(channel.toLower());
    }

    auto names = QStringList(channels.begin(), channels.end());
    names.sort();
    getSettings()->modAlertMutedChannels.setValue(names.join(','));
}

}  // namespace chatterino::alertmute
