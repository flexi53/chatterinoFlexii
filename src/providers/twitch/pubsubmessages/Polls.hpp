// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace chatterino {

/// ChattiFlexii: a poll as the topic polls.<channel> tells it. Twitch writes
/// nothing down about this topic, so what is read out of it is deliberately
/// forgiving - see the tests.
struct PubSubPoll {
    enum class Status : std::uint8_t {
        /// Still being voted on
        Active,
        /// Over, with a result
        Completed,
        /// Stopped early; what was voted still counts
        Terminated,
        /// Thrown away by the streamer
        Archived,
        Invalid,
    };

    struct Choice {
        QString title;
        int votes = 0;
    };

    QString id;
    QString title;
    Status status = Status::Invalid;
    std::vector<Choice> choices;
    int totalVotes = 0;
    /// When voting closes, worked out when the message came in - so the
    /// countdown keeps running between two messages
    QDateTime endsAt;
    /// Whether votes cost channel points, and how many
    int pointsPerVote = 0;

    /// Whether the banner should still be showing it
    bool running() const;
    /// How long there is left to vote
    std::chrono::milliseconds remaining() const;
    /// Which choice leads, or nothing while it is a tie or nobody voted
    std::optional<size_t> leader() const;
};

/// The poll in @a root, which is a whole PubSub message of the topic. Empty
/// where there is none to be found.
std::optional<PubSubPoll> pollFrom(const QJsonObject &root);

}  // namespace chatterino
