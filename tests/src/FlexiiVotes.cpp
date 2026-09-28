// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for what the banner under the header shows while a channel votes.
// Twitch tells every viewer about polls and predictions over PubSub but
// writes nothing down about those two topics, so the reading is forgiving
// on purpose - these tests hold every shape it is read in.

#include "providers/twitch/pubsubmessages/HypeTrain.hpp"
#include "providers/twitch/pubsubmessages/Polls.hpp"
#include "providers/twitch/pubsubmessages/Predictions.hpp"
#include "Test.hpp"

#include <QJsonDocument>
#include <QJsonObject>

using namespace chatterino;

namespace {

QJsonObject payload(const char *json)
{
    return QJsonDocument::fromJson(json).object();
}

const char *const RUNNING_POLL = R"({
    "type": "POLL_UPDATE",
    "data": {"poll": {
        "poll_id": "poll-1",
        "title": "Welche Karte?",
        "status": "ACTIVE",
        "duration_seconds": 120,
        "remaining_duration_milliseconds": 90000,
        "settings": {"channel_points_votes": {"is_enabled": true,
                                              "cost": 500}},
        "choices": [
            {"choice_id": "a", "title": "Dust2",
             "votes": {"total": 30, "base": 30}},
            {"choice_id": "b", "title": "Mirage",
             "votes": {"total": 70, "base": 70}}
        ],
        "votes": {"total": 100}
    }}
})";

}  // namespace

TEST(FlexiiVotes, APollIsReadOutOfWhatTheTopicSends)
{
    const auto poll = pollFrom(payload(RUNNING_POLL));
    ASSERT_TRUE(poll.has_value());

    EXPECT_EQ(poll->id, "poll-1");
    EXPECT_EQ(poll->title, "Welche Karte?");
    EXPECT_TRUE(poll->running());
    EXPECT_EQ(poll->totalVotes, 100);
    EXPECT_EQ(poll->pointsPerVote, 500);
    ASSERT_EQ(poll->choices.size(), 2);
    EXPECT_EQ(poll->choices.at(1).title, "Mirage");
    EXPECT_EQ(poll->choices.at(1).votes, 70);
    ASSERT_TRUE(poll->leader().has_value());
    EXPECT_EQ(*poll->leader(), 1);

    // A minute and a half, give or take the moment it took to read
    EXPECT_GT(poll->remaining().count(), 89000);
    EXPECT_LE(poll->remaining().count(), 90000);
}

TEST(FlexiiVotes, APollThatSaysNoRemainderIsCountedFromItsStart)
{
    // Began a quarter of a minute ago and runs for one
    const auto started = QDateTime::currentDateTimeUtc().addSecs(-15);
    const auto poll = pollFrom(payload(
        QString(R"({
        "data": {"poll": {
            "poll_id": "p", "title": "Frage", "status": "ACTIVE",
            "started_at": "%1",
            "duration_seconds": 60,
            "choices": [{"title": "Ja", "votes": 1},
                        {"title": "Nein", "votes": 2}]
        }}
    })")
            .arg(started.toString(Qt::ISODate))
            .toUtf8()
            .constData()));
    ASSERT_TRUE(poll.has_value());

    // Three quarters of a minute left, give or take the moment it took
    EXPECT_GT(poll->remaining().count(), 44000);
    EXPECT_LE(poll->remaining().count(), 45000);
    // Counted up itself, since the poll did not say
    EXPECT_EQ(poll->totalVotes, 3);
}

TEST(FlexiiVotes, APollWhoseTimeIsUpHasNothingLeft)
{
    const auto started = QDateTime::currentDateTimeUtc().addSecs(-600);
    const auto poll = pollFrom(payload(
        QString(R"({"data": {"poll": {"title": "F", "status": "ACTIVE",
                   "started_at": "%1", "duration_seconds": 60,
                   "choices": [{"title": "Ja", "votes": 1}]}}})")
            .arg(started.toString(Qt::ISODate))
            .toUtf8()
            .constData()));
    ASSERT_TRUE(poll.has_value());
    EXPECT_EQ(poll->remaining().count(), 0);
}

TEST(FlexiiVotes, AFinishedPollIsNoLongerRunning)
{
    for (const auto *status : {"COMPLETED", "TERMINATED", "ARCHIVED"})
    {
        const auto poll = pollFrom(payload(
            QString(R"({"data": {"poll": {"title": "F", "status": "%1",
                       "choices": [{"title": "Ja", "votes": 1}]}}})")
                .arg(status)
                .toUtf8()
                .constData()));
        ASSERT_TRUE(poll.has_value()) << status;
        EXPECT_FALSE(poll->running()) << status;
        EXPECT_EQ(poll->remaining().count(), 0) << status;
    }
}

TEST(FlexiiVotes, ATiedPollHasNoLeader)
{
    const auto poll = pollFrom(payload(R"({
        "data": {"poll": {"title": "F", "status": "ACTIVE", "choices": [
            {"title": "Ja", "votes": 5}, {"title": "Nein", "votes": 5}]}}
    })"));
    ASSERT_TRUE(poll.has_value());
    EXPECT_FALSE(poll->leader().has_value());
}

TEST(FlexiiVotes, WhatIsNoPollIsNotRead)
{
    EXPECT_FALSE(pollFrom(payload(R"({"type": "POLL_ARCHIVE"})")).has_value());
    EXPECT_FALSE(pollFrom(payload(R"({"data": {"poll": {"title": "F"}}})"))
                     .has_value());
    EXPECT_FALSE(pollFrom({}).has_value());
}

TEST(FlexiiVotes, APredictionIsReadOutOfWhatTheTopicSends)
{
    const auto prediction = predictionFrom(payload(R"({
        "type": "event-updated",
        "data": {"event": {
            "id": "pred-1",
            "title": "Schafft er es?",
            "status": "ACTIVE",
            "created_at": "2020-09-28T10:00:00Z",
            "prediction_window_seconds": 120,
            "outcomes": [
                {"id": "blue", "color": "BLUE", "title": "Ja",
                 "total_points": 7500, "total_users": 30},
                {"id": "pink", "color": "PINK", "title": "Nein",
                 "total_points": 2500, "total_users": 10}
            ]
        }}
    })"));
    ASSERT_TRUE(prediction.has_value());

    EXPECT_EQ(prediction->id, "pred-1");
    EXPECT_EQ(prediction->title, "Schafft er es?");
    EXPECT_TRUE(prediction->running());
    EXPECT_EQ(prediction->totalPoints(), 10000);
    ASSERT_EQ(prediction->outcomes.size(), 2);
    EXPECT_EQ(prediction->outcomes.at(0).users, 30);
    EXPECT_EQ(prediction->outcomes.at(0).asColor(), QColor(0x38, 0x7a, 0xff));
    EXPECT_EQ(prediction->outcomes.at(1).asColor(), QColor(0xf5, 0x00, 0x9b));

    // 10000 on all of it, 7500 on the first: a third on top
    EXPECT_NEAR(prediction->payoutOf(0), 1.333, 0.001);
    EXPECT_NEAR(prediction->payoutOf(1), 4.0, 0.001);
    EXPECT_EQ(prediction->payoutOf(9), 0.0);

    // Two minutes from when it began, which is long past
    EXPECT_EQ(prediction->locksAt,
              QDateTime::fromString("2020-09-28T10:02:00Z", Qt::ISODate));
    EXPECT_EQ(prediction->remaining().count(), 0);
}

TEST(FlexiiVotes, APredictionStillOpenCountsDownToWhenItLocks)
{
    const auto created = QDateTime::currentDateTimeUtc().addSecs(-30);
    const auto prediction = predictionFrom(payload(
        QString(R"({"data": {"event": {"id": "p", "title": "F",
                   "status": "ACTIVE", "created_at": "%1",
                   "prediction_window_seconds": 120,
                   "outcomes": [{"id": "a", "title": "Ja",
                                 "total_points": 10}]}}})")
            .arg(created.toString(Qt::ISODate))
            .toUtf8()
            .constData()));
    ASSERT_TRUE(prediction.has_value());
    EXPECT_GT(prediction->remaining().count(), 89000);
    EXPECT_LE(prediction->remaining().count(), 90000);
}

TEST(FlexiiVotes, ALockedPredictionStaysUpUntilItIsSettled)
{
    const auto locked = predictionFrom(payload(R"({
        "data": {"event": {"id": "p", "title": "F", "status": "LOCKED",
            "outcomes": [{"id": "a", "title": "Ja", "total_points": 10}]}}
    })"));
    ASSERT_TRUE(locked.has_value());
    EXPECT_TRUE(locked->running());
    EXPECT_EQ(locked->remaining().count(), 0);

    const auto resolved = predictionFrom(payload(R"({
        "data": {"event": {"id": "p", "title": "F", "status": "RESOLVED",
            "winning_outcome_id": "a",
            "outcomes": [{"id": "a", "title": "Ja", "total_points": 10}]}}
    })"));
    ASSERT_TRUE(resolved.has_value());
    EXPECT_FALSE(resolved->running());
    EXPECT_EQ(resolved->winningOutcomeId, "a");

    const auto canceled = predictionFrom(payload(R"({
        "data": {"event": {"id": "p", "title": "F", "status": "CANCELED",
            "outcomes": [{"id": "a", "title": "Ja", "total_points": 10}]}}
    })"));
    ASSERT_TRUE(canceled.has_value());
    EXPECT_FALSE(canceled->running());
}

TEST(FlexiiVotes, WhatIsNoPredictionIsNotRead)
{
    EXPECT_FALSE(predictionFrom(payload(R"({"type": "event-updated"})"))
                     .has_value());
    EXPECT_FALSE(
        predictionFrom(payload(R"({"data": {"event": {"title": "F"}}})"))
            .has_value());
    EXPECT_FALSE(predictionFrom({}).has_value());
}

TEST(FlexiiVotes, AHypeTrainIsReadOutOfWhatTheTopicSends)
{
    const auto train = hypeTrainFrom(payload(R"({
        "type": "hype-train-progression",
        "data": {
            "user_id": "123",
            "quantity": 500,
            "progress": {
                "level": {"value": 3, "goal": 4500},
                "value": 2300,
                "goal": 4500,
                "total": 12300,
                "remaining_seconds": 240
            }
        }
    })"));
    ASSERT_TRUE(train.has_value());

    EXPECT_EQ(train->kind, PubSubHypeTrain::Kind::Progress);
    EXPECT_TRUE(train->hasProgress);
    EXPECT_FALSE(train->over);
    EXPECT_EQ(train->level, 3);
    EXPECT_EQ(train->value, 2300);
    EXPECT_EQ(train->goal, 4500);
    EXPECT_NEAR(train->share(), 0.511, 0.001);
    // Four minutes, give or take the moment it took to read
    EXPECT_GT(train->remaining().count(), 239000);
    EXPECT_LE(train->remaining().count(), 240000);
}

TEST(FlexiiVotes, AHypeTrainThatStartsBringsItsOwnEnd)
{
    const auto expires = QDateTime::currentDateTimeUtc().addSecs(300);
    const auto train = hypeTrainFrom(payload(
        QString(R"({"type": "hype-train-start", "data": {
            "id": "train-1", "expires_at": "%1",
            "progress": {"level": {"value": 1, "goal": 1800}, "value": 400}}})")
            .arg(expires.toString(Qt::ISODate))
            .toUtf8()
            .constData()));
    ASSERT_TRUE(train.has_value());

    EXPECT_EQ(train->kind, PubSubHypeTrain::Kind::Started);
    EXPECT_EQ(train->id, "train-1");
    EXPECT_EQ(train->level, 1);
    EXPECT_EQ(train->value, 400);
    // The level's goal, where the progress itself names none
    EXPECT_EQ(train->goal, 1800);
    EXPECT_GT(train->remaining().count(), 299000);
}

TEST(FlexiiVotes, AHypeTrainEndsWithoutSayingWhereItStood)
{
    const auto done = hypeTrainFrom(payload(R"({
        "type": "hype-train-end",
        "data": {"ended_at": 1759053660000, "ending_reason": "COMPLETED"}
    })"));
    ASSERT_TRUE(done.has_value());
    EXPECT_EQ(done->kind, PubSubHypeTrain::Kind::Ended);
    EXPECT_TRUE(done->over);
    EXPECT_TRUE(done->completed);
    EXPECT_FALSE(done->hasProgress);
    EXPECT_EQ(done->remaining().count(), 0);

    const auto expired = hypeTrainFrom(payload(R"({
        "type": "hype-train-end",
        "data": {"ending_reason": "EXPIRE"}
    })"));
    ASSERT_TRUE(expired.has_value());
    EXPECT_TRUE(expired->over);
    EXPECT_FALSE(expired->completed);
}

TEST(FlexiiVotes, WhatTheHypeTopicElseSendsSaysNothingAboutTheTrain)
{
    // A conductor change carries no progress - it must not wipe what we
    // have, which is why it says so
    const auto conductor = hypeTrainFrom(payload(R"({
        "type": "hype-train-conductor-update",
        "data": {"source": "BITS", "user": {"id": "1"}}
    })"));
    EXPECT_FALSE(conductor.has_value());

    const auto levelUp = hypeTrainFrom(payload(R"({
        "type": "hype-train-level-up",
        "data": {"time_to_expire": 1759053660000,
                 "progress": {"level": {"value": 4, "goal": 7600},
                              "value": 100}}
    })"));
    ASSERT_TRUE(levelUp.has_value());
    EXPECT_EQ(levelUp->kind, PubSubHypeTrain::Kind::LevelUp);
    EXPECT_EQ(levelUp->level, 4);
    EXPECT_TRUE(levelUp->hasProgress);

    EXPECT_FALSE(hypeTrainFrom({}).has_value());
}
