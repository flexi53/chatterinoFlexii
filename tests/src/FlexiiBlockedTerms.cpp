// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for /blockterm, /unblockterm and /blockterms - the words AutoMod
// holds back in a channel. What talks to Twitch is a thin passthrough; what
// is checked here is what happens before and after it.

#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/builtin/twitch/BlockedTerms.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "controllers/commands/Command.hpp"
#include "controllers/commands/CommandController.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "mocks/BaseApplication.hpp"
#include "mocks/EmoteController.hpp"
#include "mocks/Helix.hpp"
#include "mocks/Logging.hpp"
#include "mocks/TwitchIrcServer.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "widgets/dialogs/NukePopup.hpp"
#include "singletons/Settings.hpp"
#include "Test.hpp"

#include <QJsonObject>

using namespace chatterino;

namespace {

class MockApplication : public mock::BaseApplication
{
public:
    MockApplication()
        : commands(this->paths_)
    {
    }

    ITwitchIrcServer *getTwitch() override
    {
        return &this->twitch;
    }
    AccountController *getAccounts() override
    {
        return &this->accounts;
    }
    CommandController *getCommands() override
    {
        return &this->commands;
    }
    EmoteController *getEmotes() override
    {
        return &this->emotes;
    }
    ILogging *getChatLogger() override
    {
        return &this->chatLogger;
    }

    mock::EmptyLogging chatLogger;
    AccountController accounts;
    CommandController commands;
    mock::MockTwitchIrcServer twitch;
    mock::EmoteController emotes;
};

/// A term as Twitch hands it back
HelixBlockedTerm term(const QString &id, const QString &text)
{
    return HelixBlockedTerm(QJsonObject{{"id", id}, {"text", text}});
}

/// The last thing said in @a channel
QString lastSaid(const ChannelPtr &channel)
{
    const auto &messages = channel->getMessageSnapshot();
    if (messages.size() == 0)
    {
        return {};
    }
    return messages[messages.size() - 1]->messageText;
}

}  // namespace

TEST(FlexiiBlockedTerms, TheIdIsFoundHoweverItIsWritten)
{
    const std::vector<HelixBlockedTerm> terms{
        term("1", "hallo"),
        term("2", "Kaufe Follower"),
    };

    EXPECT_EQ(commands::idOfTerm(terms, "hallo"), "1");
    // Twitch keeps them as they were written, we look past that
    EXPECT_EQ(commands::idOfTerm(terms, "HALLO"), "1");
    EXPECT_EQ(commands::idOfTerm(terms, "kaufe follower"), "2");

    EXPECT_FALSE(commands::idOfTerm(terms, "kaufe").has_value());
    EXPECT_FALSE(commands::idOfTerm(terms, "").has_value());
    EXPECT_FALSE(commands::idOfTerm({}, "hallo").has_value());
}

TEST(FlexiiBlockedTerms, WithoutAWordItSaysHowItGoes)
{
    MockApplication app;
    auto channel = std::make_shared<TwitchChannel>("forsen");

    CommandContext ctx{
        .words = {"/blockterm"},
        .channel = channel,
        .twitchChannel = channel.get(),
    };
    commands::blockTerm(ctx);
    EXPECT_TRUE(lastSaid(channel).contains("/blockterm <Wort oder Satz>"))
        << lastSaid(channel).toStdString();

    ctx.words = QStringList{"/unblockterm"};
    commands::unblockTerm(ctx);
    EXPECT_TRUE(lastSaid(channel).contains("/unblockterm <Wort oder Satz>"))
        << lastSaid(channel).toStdString();
}

TEST(FlexiiBlockedTerms, WithoutAnAccountItSaysSo)
{
    MockApplication app;
    auto channel = std::make_shared<TwitchChannel>("forsen");

    const CommandContext ctx{
        .words = {"/blockterm", "kaufe", "follower"},
        .channel = channel,
        .twitchChannel = channel.get(),
    };
    commands::blockTerm(ctx);
    EXPECT_TRUE(lastSaid(channel).contains("angemeldet"))
        << lastSaid(channel).toStdString();
}

TEST(FlexiiBlockedTerms, ItOnlyWorksInATwitchChannel)
{
    MockApplication app;
    auto channel = std::make_shared<Channel>("/mentions",
                                             Channel::Type::TwitchMentions);

    const CommandContext ctx{
        .words = {"/blockterms"},
        .channel = channel,
    };
    commands::listBlockedTerms(ctx);
    EXPECT_TRUE(lastSaid(channel).contains("Twitch-Kanal"))
        << lastSaid(channel).toStdString();
}

namespace {

/// A message as it stands in a channel
void say(const ChannelPtr &channel, const QString &login, const QString &text,
         const QStringList &badges = {}, int secondsAgo = 5)
{
    MessageBuilder builder;
    builder->loginName = login;
    builder->displayName = login;
    builder->messageText = text;
    builder->serverReceivedTime =
        QDateTime::currentDateTimeUtc().addSecs(-secondsAgo);
    builder->flags.set(MessageFlag::DoNotLog);
    for (const auto &badge : badges)
    {
        builder->twitchBadges.emplace_back(badge, "1");
    }
    channel->addMessage(builder.release(), MessageContext::Original);
}

}  // namespace

TEST(FlexiiNuke, ItCatchesTheRightPeopleAndOnlyThem)
{
    MockApplication app;
    auto channel = std::make_shared<TwitchChannel>("forsen");

    say(channel, "spammer1", "KAUFE FOLLOWER billig");
    say(channel, "spammer2", "kaufe follower hier");
    say(channel, "normalo", "was ist denn hier los");
    say(channel, "eininmod", "kaufe follower - nicht!", {"moderator"});
    say(channel, "derstreamer", "kaufe follower", {"broadcaster"});
    say(channel, "einvip", "kaufe follower", {"vip"});
    say(channel, "fx_flexii", "kaufe follower");
    // Long past, outside the stretch looked at
    say(channel, "vorhin", "kaufe follower", {}, 3600);

    const auto caught =
        NukePopup::whoWrote(channel, "kaufe follower", 10, "fx_flexii");

    QStringList names;
    for (const auto &one : caught)
    {
        names.append(one.login);
    }
    names.sort();
    EXPECT_EQ(names, (QStringList{"spammer1", "spammer2"}))
        << names.join(", ").toStdString();
}

TEST(FlexiiNuke, EveryoneIsListedOnce)
{
    MockApplication app;
    auto channel = std::make_shared<TwitchChannel>("forsen");

    say(channel, "spammer", "erste");
    say(channel, "spammer", "zweite");
    say(channel, "spammer", "dritte");

    const auto caught = NukePopup::whoWrote(channel, "e", 10, "ich");
    ASSERT_EQ(caught.size(), 1u);
    // and with the last thing they wrote
    EXPECT_EQ(caught.at(0).message, "dritte");
}

TEST(FlexiiNuke, WhoIsBeyondATimeout)
{
    EXPECT_TRUE(NukePopup::beyondReach({TwitchBadge("moderator", "1")}));
    EXPECT_TRUE(NukePopup::beyondReach({TwitchBadge("broadcaster", "1")}));
    EXPECT_TRUE(NukePopup::beyondReach({TwitchBadge("vip", "1")}));
    EXPECT_TRUE(NukePopup::beyondReach(
        {TwitchBadge("subscriber", "12"), TwitchBadge("staff", "1")}));

    EXPECT_FALSE(NukePopup::beyondReach({}));
    EXPECT_FALSE(NukePopup::beyondReach({TwitchBadge("subscriber", "12")}));
    EXPECT_FALSE(NukePopup::beyondReach({TwitchBadge("premium", "1")}));
}
