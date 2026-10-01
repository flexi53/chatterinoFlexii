// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/banners/BannerChannels.hpp"

#include "singletons/Settings.hpp"

#include <QSet>
#include <QStringList>

namespace chatterino::banners {

namespace {

QSet<QString> hypeOff()
{
    const auto value = getSettings()->hypeTrainOffChannels.getValue();
    QSet<QString> channels;
    for (const auto &channel : value.split(',', Qt::SkipEmptyParts))
    {
        channels.insert(channel);
    }
    return channels;
}

}  // namespace

bool hypeShownIn(const QString &channel)
{
    if (channel.isEmpty())
    {
        return true;
    }
    return !hypeOff().contains(channel.toLower());
}

void setHypeShownIn(const QString &channel, bool shown)
{
    if (channel.isEmpty())
    {
        return;
    }

    auto channels = hypeOff();
    if (shown)
    {
        channels.remove(channel.toLower());
    }
    else
    {
        channels.insert(channel.toLower());
    }

    auto names = QStringList(channels.begin(), channels.end());
    names.sort();
    getSettings()->hypeTrainOffChannels.setValue(names.join(','));
}

}  // namespace chatterino::banners
