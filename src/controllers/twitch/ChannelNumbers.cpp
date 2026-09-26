// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/twitch/ChannelNumbers.hpp"

#include "common/QLogging.hpp"
#include "providers/twitch/api/Helix.hpp"

#include <QHash>

#include <deque>

namespace chatterino::channelnumbers {

namespace {

/// One number asked of Twitch, with the moment it was asked
struct Asked {
    std::optional<int> value;
    QDateTime at;
    bool waiting = false;

    /// Whether it is time to ask again
    bool stale(std::chrono::minutes fresh) const
    {
        if (this->waiting)
        {
            return false;
        }
        return !this->at.isValid() ||
               this->at.secsTo(QDateTime::currentDateTimeUtc()) >
                   qint64(std::chrono::seconds(fresh).count());
    }
};

/// How many watched, and when. Made once and never taken down - see
/// ProfilePictures for why that matters at the end of the program.
QHash<QString, Asked> &followerStore()
{
    static auto *store = new QHash<QString, Asked>();
    return *store;
}

QHash<QString, Asked> &chatterStore()
{
    static auto *store = new QHash<QString, Asked>();
    return *store;
}

QHash<QString, std::deque<std::pair<QDateTime, int>>> &viewerStore()
{
    static auto *store =
        new QHash<QString, std::deque<std::pair<QDateTime, int>>>();
    return *store;
}

}  // namespace

std::optional<int> followers(const QString &broadcasterId)
{
    if (broadcasterId.isEmpty())
    {
        return {};
    }

    auto &asked = followerStore()[broadcasterId];
    if (asked.stale(FOLLOWERS_FRESH))
    {
        asked.waiting = true;
        getHelix()->getChannelFollowers(
            broadcasterId,
            [broadcasterId](const auto &response) {
                auto &entry = followerStore()[broadcasterId];
                entry.value = response.total;
                entry.at = QDateTime::currentDateTimeUtc();
                entry.waiting = false;
            },
            [broadcasterId](const auto &error) {
                auto &entry = followerStore()[broadcasterId];
                entry.at = QDateTime::currentDateTimeUtc();
                entry.waiting = false;
                qCDebug(chatterinoTwitch)
                    << "No follower count for" << broadcasterId << ":" << error;
            });
    }
    return asked.value;
}

std::optional<int> chatters(const QString &broadcasterId,
                            const QString &moderatorId)
{
    if (broadcasterId.isEmpty() || moderatorId.isEmpty())
    {
        return {};
    }

    auto &asked = chatterStore()[broadcasterId];
    if (asked.stale(CHATTERS_FRESH))
    {
        asked.waiting = true;
        // One name is enough - the count comes with every answer
        getHelix()->getChatters(
            broadcasterId, moderatorId, 1,
            [broadcasterId](const auto &result) {
                auto &entry = chatterStore()[broadcasterId];
                entry.value = result.total;
                entry.at = QDateTime::currentDateTimeUtc();
                entry.waiting = false;
            },
            [broadcasterId](auto /*error*/, const QString &message) {
                auto &entry = chatterStore()[broadcasterId];
                entry.at = QDateTime::currentDateTimeUtc();
                entry.waiting = false;
                qCDebug(chatterinoTwitch) << "No chatter count for"
                                          << broadcasterId << ":" << message;
            });
    }
    return asked.value;
}

void noteViewers(const QString &broadcasterId, int count, QDateTime when)
{
    if (broadcasterId.isEmpty() || count < 0 || !when.isValid())
    {
        return;
    }

    auto &counts = viewerStore()[broadcasterId];
    // The same moment twice - the header is drawn more often than Twitch
    // says anything new
    if (!counts.empty() && counts.back().first >= when)
    {
        counts.back().second = count;
        return;
    }
    counts.emplace_back(when, count);

    const auto keeps = qint64(std::chrono::seconds(TREND_KEEPS).count());
    while (!counts.empty() && counts.front().first.secsTo(when) > keeps)
    {
        counts.pop_front();
    }
}

std::optional<double> viewerTrend(const QString &broadcasterId, QDateTime now)
{
    const auto found = viewerStore().find(broadcasterId);
    if (found == viewerStore().end() || found->size() < 2)
    {
        return {};
    }
    const auto &counts = *found;

    const auto latest = counts.back();
    if (latest.first.secsTo(now) >
        qint64(std::chrono::seconds(TREND_LATEST).count()))
    {
        // Nothing recent to compare - the channel may have gone offline
        return {};
    }

    // What it was half an hour ago, or as far back as is known - but not
    // so recent that the two say the same thing
    const auto over = qint64(std::chrono::seconds(TREND_OVER).count());
    const auto needs = qint64(std::chrono::seconds(TREND_NEEDS).count());
    std::optional<std::pair<QDateTime, int>> before;
    for (const auto &count : counts)
    {
        const auto age = count.first.secsTo(latest.first);
        if (age < needs)
        {
            break;
        }
        if (!before || std::abs(age - over) < std::abs(before->first.secsTo(
                                                           latest.first) -
                                                       over))
        {
            before = count;
        }
    }
    if (!before || before->second <= 0)
    {
        return {};
    }

    return (double(latest.second) - double(before->second)) /
           double(before->second);
}

void forget()
{
    followerStore().clear();
    chatterStore().clear();
    viewerStore().clear();
}

}  // namespace chatterino::channelnumbers
