// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for the pinned message the banner shows. Chatterino asks Helix for
// it, which only answers moderators - so we read it out of the pin event
// itself, which every viewer receives. What that event looks like is not
// written down anywhere by Twitch, so the reading is deliberately forgiving:
// these tests hold every shape it is read in.

#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/pubsubmessages/PinnedChatUpdates.hpp"
#include "Test.hpp"

#include <QJsonDocument>
#include <QJsonObject>

using namespace chatterino;

namespace {

QJsonObject payload(const char *json)
{
    return QJsonDocument::fromJson(json).object();
}

}  // namespace

TEST(FlexiiPinnedMessage, TheEventAloneIsEnoughForTheBanner)
{
    const auto as = pinnedMessageAsHelix(payload(R"({
        "id": "pin-1",
        "message": {
            "id": "abc-123",
            "sender": {
                "id": "1234",
                "login": "someone",
                "display_name": "Someone"
            },
            "content": {"text": "hallo welt"},
            "starts_at": "2026-09-28T10:00:00Z",
            "ends_at": "2026-09-28T10:01:00Z"
        },
        "pinned_by": {"id": "99", "login": "themod", "display_name": "TheMod"}
    })"));
    ASSERT_TRUE(as.has_value());

    const HelixPinnedChatMessage pin(*as);
    EXPECT_EQ(pin.messageID, "abc-123");
    EXPECT_EQ(pin.messageText, "hallo welt");
    EXPECT_EQ(pin.sender.login, "someone");
    EXPECT_EQ(pin.sender.displayName, "Someone");
    EXPECT_EQ(pin.pinnedBy.displayName, "TheMod");
    EXPECT_EQ(pin.startsAt,
              QDateTime::fromString("2026-09-28T10:00:00Z", Qt::ISODate));
    ASSERT_TRUE(pin.endsAt.has_value());
    EXPECT_EQ(*pin.endsAt,
              QDateTime::fromString("2026-09-28T10:01:00Z", Qt::ISODate));
}

TEST(FlexiiPinnedMessage, SecondsSinceTheEpochBecomeATime)
{
    const auto as = pinnedMessageAsHelix(payload(R"({
        "message": {
            "id": "abc",
            "sender": {"id": "1", "login": "someone", "display_name": "Someone"},
            "content": {"text": "hi"},
            "starts_at": 1759053600,
            "ends_at": 1759053660
        }
    })"));
    ASSERT_TRUE(as.has_value());

    const HelixPinnedChatMessage pin(*as);
    EXPECT_EQ(pin.startsAt.toSecsSinceEpoch(), 1759053600);
    ASSERT_TRUE(pin.endsAt.has_value());
    EXPECT_EQ(pin.endsAt->toSecsSinceEpoch(), 1759053660);
}

TEST(FlexiiPinnedMessage, MillisecondsWorkJustTheSame)
{
    const auto as = pinnedMessageAsHelix(payload(R"({
        "message": {
            "sender": {"display_name": "Someone"},
            "content": {"text": "hi"},
            "starts_at": 1759053600000
        }
    })"));
    ASSERT_TRUE(as.has_value());
    EXPECT_EQ(HelixPinnedChatMessage(*as).startsAt.toSecsSinceEpoch(),
              1759053600);
}

TEST(FlexiiPinnedMessage, TextWrittenInPiecesIsPutBackTogether)
{
    const auto as = pinnedMessageAsHelix(payload(R"({
        "message": {
            "sender": {"display_name": "Someone"},
            "content": {"fragments": [
                {"text": "erst das "},
                {"text": "und dann das"}
            ]}
        }
    })"));
    ASSERT_TRUE(as.has_value());
    EXPECT_EQ(HelixPinnedChatMessage(*as).messageText, "erst das und dann das");
}

TEST(FlexiiPinnedMessage, WithoutALoginTheDisplayNameInSmallLettersDoes)
{
    const auto as = pinnedMessageAsHelix(payload(R"({
        "message": {
            "sender": {"id": "1", "display_name": "SomeOne"},
            "text": "hi"
        },
        "pinned_by": {"display_name": "TheMod"}
    })"));
    ASSERT_TRUE(as.has_value());

    const HelixPinnedChatMessage pin(*as);
    EXPECT_EQ(pin.sender.login, "someone");
    EXPECT_EQ(pin.sender.displayName, "SomeOne");
    EXPECT_EQ(pin.pinnedBy.login, "themod");
}

TEST(FlexiiPinnedMessage, WithoutATimeItCountsAsJustNow)
{
    const auto before = QDateTime::currentDateTimeUtc().addSecs(-2);
    const auto as = pinnedMessageAsHelix(payload(R"({
        "message": {
            "sender": {"display_name": "Someone"},
            "content": {"text": "hi"}
        }
    })"));
    ASSERT_TRUE(as.has_value());

    const HelixPinnedChatMessage pin(*as);
    EXPECT_TRUE(pin.startsAt >= before);
    EXPECT_FALSE(pin.endsAt.has_value());
}

TEST(FlexiiPinnedMessage, APayloadThatIsTheMessageItselfIsReadToo)
{
    // Should the event ever hand the message over without wrapping it
    const auto as = pinnedMessageAsHelix(payload(R"({
        "id": "abc",
        "sender": {"id": "1", "login": "someone", "display_name": "Someone"},
        "content": {"text": "hi"}
    })"));
    ASSERT_TRUE(as.has_value());

    const HelixPinnedChatMessage pin(*as);
    EXPECT_EQ(pin.messageID, "abc");
    EXPECT_EQ(pin.messageText, "hi");
    EXPECT_EQ(pin.sender.login, "someone");
}

TEST(FlexiiPinnedMessage, AnEventWithoutAMessageLeavesItToHelix)
{
    // What an update or unpin event carries: ids, no words
    EXPECT_FALSE(pinnedMessageAsHelix(payload(R"({
        "id": "pin-1",
        "message_id": "abc",
        "ends_at": 1759053660
    })"))
                     .has_value());

    // Words, but nobody who wrote them
    EXPECT_FALSE(pinnedMessageAsHelix(payload(R"({
        "message": {"id": "abc", "content": {"text": "hi"}}
    })"))
                     .has_value());

    EXPECT_FALSE(pinnedMessageAsHelix({}).has_value());
}
