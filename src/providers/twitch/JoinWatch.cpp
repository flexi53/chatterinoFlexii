// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/JoinWatch.hpp"

#include <algorithm>
#include <vector>

namespace chatterino::joinwatch {

QStringList overdue(const std::map<QString, Pending> &pending,
                    const QDateTime &now, std::chrono::seconds after,
                    int mostTries)
{
    std::vector<std::pair<QDateTime, QString>> waiting;

    for (const auto &[channel, one] : pending)
    {
        if (one.tries >= mostTries || !one.asked.isValid())
        {
            continue;
        }
        if (one.asked.secsTo(now) < after.count())
        {
            continue;
        }
        waiting.emplace_back(one.asked, channel);
    }

    // The one waiting longest goes first
    std::ranges::sort(waiting);

    QStringList channels;
    channels.reserve(static_cast<int>(waiting.size()));
    for (const auto &[asked, channel] : waiting)
    {
        channels.append(channel);
    }
    return channels;
}

}  // namespace chatterino::joinwatch
