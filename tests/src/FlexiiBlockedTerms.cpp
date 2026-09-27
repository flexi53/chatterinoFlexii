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
#include "mocks/BaseApplication.hpp"
#include "mocks/EmoteController.hpp"
#include "mocks/Helix.hpp"
#include "mocks/Logging.hpp"
#include "mocks/TwitchIrcServer.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
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
