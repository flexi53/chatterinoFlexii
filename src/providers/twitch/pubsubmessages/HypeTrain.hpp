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

namespace chatterino {

/// ChattiFlexii: a hype train as the topic hype-train-events-v1.<channel>
/// tells it. Twitch writes nothing down about this topic either, so what is
/// read out of it is forgiving - see the tests.
struct PubSubHypeTrain {
    enum class Kind : std::uint8_t {
        /// One just got going
        Started,
        /// Somebody put something in
        Progress,
        /// It reached the next level
        LevelUp,
        /// Over - either done or run out
        Ended,
        /// Something else the topic sends, which says nothing about a train
        Other,
    };

    Kind kind = Kind::Other;
    QString id;
    /// Whether this message said where the train stands
    bool hasProgress = false;
    int level = 1;
    /// How far into this level it is, and what the level needs
    qint64 value = 0;
    qint64 goal = 0;
    /// When it runs out unless somebody puts something in
    QDateTime expiresAt;
    /// Only where it ended: whether it was completed rather than run out
    bool completed = false;
    /// Set once it is over
    bool over = false;

    /// How far this level has come, between 0 and 1
    double share() const;
    /// How long it has left
    std::chrono::milliseconds remaining() const;
};

/// The hype train in @a root, which is a whole PubSub message of the topic.
/// Empty where the message says nothing about one.
std::optional<PubSubHypeTrain> hypeTrainFrom(const QJsonObject &root);

}  // namespace chatterino
