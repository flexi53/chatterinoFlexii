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

/// ChattiFlexii: a raid the channel is getting ready, as the topic
/// raid.<channel> tells it. Twitch writes nothing down about this topic, so
/// what is read out of it is forgiving - see the tests.
struct PubSubRaid {
    enum class Kind : std::uint8_t {
        /// Set up, or the number of viewers going along changed
        Update,
        /// It went - the viewers are on their way
        Went,
        /// Called off before it went
        Canceled,
        /// Something else the topic sends
        Other,
    };

    Kind kind = Kind::Other;
    QString id;
    QString targetId;
    QString targetLogin;
    QString targetDisplayName;
    int viewerCount = 0;
    /// When it goes by itself, worked out when the message came in
    QDateTime goesAt;
    /// Whether the message said when the raid was set up - otherwise
    /// goesAt is only counted from the moment it came in
    bool startKnown = false;
    /// How long Twitch waits before it goes by itself
    int waitSeconds = 90;

    /// How long until it goes by itself
    std::chrono::milliseconds remaining() const;
};

/// The raid in @a root, which is a whole PubSub message of the topic. Empty
/// where the message says nothing about one.
std::optional<PubSubRaid> raidFrom(const QJsonObject &root);

}  // namespace chatterino
