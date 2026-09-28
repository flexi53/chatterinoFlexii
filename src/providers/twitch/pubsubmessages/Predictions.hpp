// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace chatterino {

/// ChattiFlexii: a prediction as the topic predictions-channel-v1.<channel>
/// tells it. As with polls, Twitch writes nothing down about it.
struct PubSubPrediction {
    enum class Status : std::uint8_t {
        /// Points can still be put in
        Active,
        /// Closed, the outcome is not known yet
        Locked,
        /// Over, one outcome won
        Resolved,
        /// Called off, everyone got their points back
        Canceled,
        Invalid,
    };

    struct Outcome {
        QString id;
        QString title;
        /// What Twitch calls the outcome's colour: BLUE or PINK
        QString color;
        qint64 points = 0;
        int users = 0;

        /// The colour to draw it in
        QColor asColor() const;
    };

    QString id;
    QString title;
    Status status = Status::Invalid;
    std::vector<Outcome> outcomes;
    QString winningOutcomeId;
    /// When betting closes - only while it is running
    QDateTime locksAt;

    /// Whether the banner should still be showing it
    bool running() const;
    /// How long there is left to bet
    std::chrono::milliseconds remaining() const;
    /// All the points put in
    qint64 totalPoints() const;
    /// What a point on @a outcome pays out, as in 2.4 for 2.4 times back.
    /// Zero where nothing was put on it.
    double payoutOf(size_t outcome) const;
};

/// The prediction in @a root, which is a whole PubSub message of the topic.
/// Empty where there is none to be found.
std::optional<PubSubPrediction> predictionFrom(const QJsonObject &root);

}  // namespace chatterino
