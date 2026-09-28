// SPDX-FileCopyrightText: 2022 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/liveupdates/Diag.hpp"

#include <pajlada/signals/signal.hpp>
#include "providers/twitch/pubsubmessages/Polls.hpp"
#include "providers/twitch/pubsubmessages/Predictions.hpp"

#include <QJsonObject>
#include <QString>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

#if __has_include(<gtest/gtest_prod.h>)
#    include <gtest/gtest_prod.h>
#endif

namespace chatterino {

class PubSubManagerPrivate;

/**
 * This handles the Twitch PubSub connection
 *
 * Known issues:
 *  - Upon closing a channel, we don't unsubscribe to its pubsub connections
 *  - Stop is never called, meaning we never do a clean shutdown
 */
class PubSub
{
    template <typename T>
    using Signal = pajlada::Signals::Signal<T>;

public:
    PubSub(const QString &host,
           std::chrono::seconds heartbeatInterval = std::chrono::seconds(15));
    ~PubSub();

    PubSub(const PubSub &) = delete;
    PubSub(PubSub &&) = delete;
    PubSub &operator=(const PubSub &) = delete;
    PubSub &operator=(PubSub &&) = delete;

    struct {
        Signal<const QJsonObject &> redeemed;
    } pointReward;

    struct {
        /// Emitted when a message is pinned or its pin is updated.
        /// The arguments are the channel name and, as ChattiFlexii added,
        /// what the event itself said - enough to show the banner without
        /// moderator rights.
        pajlada::Signals::Signal<const QString &, const QJsonObject &>
            pinned;
        /// Emitted when the pinned message is removed.
        /// The argument is the channel name.
        Signal<const QString &> unpinned;
    } pinnedChatUpdates;

    struct {
        /// ChattiFlexii: a poll was opened, voted in or closed. The
        /// arguments are the channel's id and the poll.
        pajlada::Signals::Signal<const QString &, const PubSubPoll &> changed;
    } polls;

    struct {
        /// ChattiFlexii: a prediction was opened, bet on, locked or
        /// settled. The arguments are the channel's id and the prediction.
        pajlada::Signals::Signal<const QString &, const PubSubPrediction &>
            changed;
    } predictions;

    /**
     * Listen to incoming channel point redemptions in the given channel.
     * This topic is relevant for everyone.
     *
     * PubSub topic: community-points-channel-v1.{channelID}
     */
    void listenToChannelPointRewards(const QString &channelID);

    void reconnect();
    /**
     * Listen to real time pin/unpin events in the given channel.
     * This topic is relevant for everyone.
     *
     * PubSub topic: pinned-chat-updates-v1.{channelID}
     */
    void listenToPinnedChatUpdates(const QString &channelID);

    /**
     * ChattiFlexii: listen to the polls of the given channel. Twitch answers
     * this topic to everyone, not only to the broadcaster - which is why the
     * banner works in every channel.
     *
     * PubSub topic: polls.{channelID}
     */
    void listenToPolls(const QString &channelID);

    /**
     * ChattiFlexii: listen to the predictions of the given channel. As with
     * polls, this topic is answered to everyone.
     *
     * PubSub topic: predictions-channel-v1.{channelID}
     */
    void listenToPredictions(const QString &channelID);

    struct {
        std::atomic<uint32_t> messagesReceived{0};
        std::atomic<uint32_t> messagesFailedToParse{0};
        std::atomic<uint32_t> failedListenResponses{0};
        std::atomic<uint32_t> listenResponses{0};
        std::atomic<uint32_t> unlistenResponses{0};
    } diag;

    /// Statistics about the opened/closed connections and received messages
    ///
    /// Used in tests.
    const liveupdates::Diag &wsDiag() const;

private:
    void stop();

    std::unique_ptr<PubSubManagerPrivate> private_;

#ifdef FRIEND_TEST
    friend class FTest;

    FRIEND_TEST(TwitchPubSubClient, ServerRespondsToPings);
    FRIEND_TEST(TwitchPubSubClient, ServerDoesntRespondToPings);
    FRIEND_TEST(TwitchPubSubClient, DisconnectedAfter1s);
    FRIEND_TEST(TwitchPubSubClient, ExceedTopicLimit);
    FRIEND_TEST(TwitchPubSubClient, ExceedTopicLimitSingleStep);
#endif
};

}  // namespace chatterino
