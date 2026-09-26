// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QString>

#include <chrono>
#include <optional>

/// The numbers behind a channel that the title bar can show: how many follow
/// it, how many stand in its chat, and which way its audience is going.
/// Asked of Twitch no more often than they change, and kept here so every
/// split of the same channel shares one answer. See Buttons -> Titelleiste.
namespace chatterino::channelnumbers {

using namespace std::chrono_literals;

/// How long an answer counts as fresh - followers move slowly, the chat
/// fills and empties faster
constexpr auto FOLLOWERS_FRESH = 5min;
constexpr auto CHATTERS_FRESH = 2min;

/// The stretch the trend looks back over, the least it needs to say
/// anything at all, and how long counts are kept
constexpr auto TREND_OVER = 30min;
constexpr auto TREND_NEEDS = 10min;
constexpr auto TREND_KEEPS = 2h;
/// A count older than this says nothing about now
constexpr auto TREND_LATEST = 5min;
/// Below this the audience is simply holding steady, and nothing is shown
constexpr double TREND_WORTH_SAYING = 0.03;

/// How many follow the channel @a broadcasterId. Empty until the first
/// answer is in; asking again happens by itself once it is stale.
std::optional<int> followers(const QString &broadcasterId);

/// How many stand in the chat of @a broadcasterId. Twitch answers this
/// only to a moderator of that channel, so @a moderatorId is your own id.
std::optional<int> chatters(const QString &broadcasterId,
                            const QString &moderatorId);

/// Notes that @a count were watching @a broadcasterId at @a when
void noteViewers(const QString &broadcasterId, int count,
                 QDateTime when = QDateTime::currentDateTimeUtc());

/// Which way the audience went over the last half hour, as a share of what
/// it was - 0.18 for a fifth more. Empty while too little is known.
std::optional<double> viewerTrend(
    const QString &broadcasterId,
    QDateTime now = QDateTime::currentDateTimeUtc());

/// Everything forgotten - for the tests
void forget();

}  // namespace chatterino::channelnumbers
