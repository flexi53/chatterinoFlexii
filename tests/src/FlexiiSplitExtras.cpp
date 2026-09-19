// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for the two small things around a split: the activity curve in its
// header - how far back it reaches and where it marks a change of what the
// channel streams - and the bar under the input that runs out while the
// channel makes you wait. Kept apart from Flexii.cpp as a widget needs an
// application with a theme.

#include "common/Channel.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "mocks/BaseApplication.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"
#include "widgets/splits/SendWaitBar.hpp"
#include "widgets/splits/SplitHeaderExtras.hpp"

using namespace chatterino;

namespace {

class MockApplication : public mock::BaseApplication
{
public:
    MockApplication()
        : windowManager(this->args, this->paths_, this->settings, this->theme,
                        this->fonts)
    {
    }

    WindowManager *getWindows() override
    {
        return &this->windowManager;
    }

    WindowManager windowManager;
};

class FlexiiActivityGraphFixture : public ::testing::Test
{
protected:
    FlexiiActivityGraphFixture()
        : channel(std::make_shared<Channel>("test", Channel::Type::Twitch))
    {
        // A clock the test moves on itself, so a stream of hours passes in
        // a moment
        this->graph.setClock([this] {
            return this->clock;
        });
        this->graph.setChannel(this->channel);
    }

    /// One message from a chatter, as the curve counts them. Not logged -
    /// there is no chat logger in a mock application.
    void chat(MessageFlags flags = {})
    {
        MessageBuilder builder;
        builder.message().messageText = "hey";
        builder.message().flags = flags;
        builder.message().flags.set(MessageFlag::DoNotLog);
        this->channel->addMessage(builder.release(), MessageContext::Original);
    }

    /// Lets @a seconds pass
    void pass(int seconds)
    {
        this->clock = this->clock.addSecs(seconds);
        this->graph.catchUp();
    }

    MockApplication app;
    QDateTime clock{QDateTime::currentDateTimeUtc()};
    ChannelPtr channel;
    ActivityGraph graph{nullptr};
};

}  // namespace

TEST_F(FlexiiActivityGraphFixture, WithoutAStreamItCoversTheLastQuarterHour)
{
    EXPECT_TRUE(this->graph.description().contains("letzten 15 Minuten"))
        << this->graph.description().toStdString();
}

TEST_F(FlexiiActivityGraphFixture, WhatPeopleWriteIsCounted)
{
    this->chat();
    this->chat();
    EXPECT_TRUE(this->graph.description().contains("2 Nachrichten"));

    // A message the app itself writes is not chat
    this->chat(MessageFlags{MessageFlag::System});
    EXPECT_TRUE(this->graph.description().contains("2 Nachrichten"));
}

TEST_F(FlexiiActivityGraphFixture, ItFollowsTheStreamFromWhenItWentLive)
{
    this->graph.followStream("42", this->clock.addSecs(-3 * 3600));

    const auto text = this->graph.description();
    EXPECT_TRUE(text.contains("seit Streamstart")) << text.toStdString();
    EXPECT_TRUE(text.contains("3 Std.")) << text.toStdString();
    // Three hours get a tick every half hour
    EXPECT_TRUE(text.contains("je 30 Min.")) << text.toStdString();
}

TEST_F(FlexiiActivityGraphFixture, WhatHappenedBeforeTheChannelWasOpenIsSaid)
{
    this->graph.followStream("42", this->clock.addSecs(-2 * 3600));
    EXPECT_TRUE(this->graph.description().contains("Vor dem Öffnen des Kanals"))
        << this->graph.description().toStdString();
}

TEST_F(FlexiiActivityGraphFixture, ANewStreamLetsGoOfWhatCameBefore)
{
    this->graph.followStream("42", this->clock.addSecs(-3600));
    this->chat();
    this->chat();
    this->pass(300);
    ASSERT_TRUE(this->graph.description().contains("2 Nachrichten"));

    // A stream of its own: what belonged to the last one is let go of
    this->graph.followStream("43", this->clock);
    EXPECT_TRUE(this->graph.description().contains("0 Nachrichten"))
        << this->graph.description().toStdString();
}

TEST_F(FlexiiActivityGraphFixture, ATabThatWasOpenBeforeKeepsWhatItCounted)
{
    // The tab was already open, so the chat before the stream is counted
    // too - but it belongs to no stream
    this->chat();
    this->pass(600);
    const auto wentLive = this->clock;

    // Five minutes into the stream it is noticed
    this->pass(300);
    this->chat();
    this->chat();
    this->graph.followStream("42", wentLive);

    const auto text = this->graph.description();
    // The two since it went live, not the one from before
    EXPECT_TRUE(text.contains("2 Nachrichten")) << text.toStdString();
    // Nothing is missing, so nothing is said about it
    EXPECT_FALSE(text.contains("Vor dem Öffnen des Kanals"))
        << text.toStdString();
}

TEST_F(FlexiiActivityGraphFixture, AStreamEndingBringsBackTheQuarterHour)
{
    this->graph.followStream("42", this->clock.addSecs(-3600));
    ASSERT_TRUE(this->graph.description().contains("seit Streamstart"));

    this->graph.unfollowStream();
    EXPECT_TRUE(this->graph.description().contains("letzten 15 Minuten"));
}

TEST_F(FlexiiActivityGraphFixture, TheFirstCategoryIsNotAChange)
{
    // A stream starting is not a change from something else
    this->graph.noteCategory("Just Chatting");
    EXPECT_FALSE(this->graph.description().contains("Just Chatting"));
}

TEST_F(FlexiiActivityGraphFixture, EveryChangeOfCategoryIsKeptWithItsTime)
{
    this->graph.followStream("42", this->clock.addSecs(-4 * 3600));

    this->graph.noteCategory("Just Chatting");
    this->pass(3600);
    this->graph.noteCategory("Valorant");
    const auto valorantAt = this->clock.toLocalTime().toString("HH:mm");
    this->pass(3600);
    this->graph.noteCategory("Fortnite");
    const auto fortniteAt = this->clock.toLocalTime().toString("HH:mm");

    const auto text = this->graph.description();
    EXPECT_TRUE(text.contains(valorantAt + " Uhr: Valorant"))
        << text.toStdString();
    EXPECT_TRUE(text.contains(fortniteAt + " Uhr: Fortnite"))
        << text.toStdString();

    // The same category again is no change
    this->graph.noteCategory("Fortnite");
    EXPECT_EQ(text.count("Fortnite"), 1);
}

TEST_F(FlexiiActivityGraphFixture, AChangeBeforeTheCurveStartsIsNotListed)
{
    this->graph.noteCategory("Just Chatting");
    this->graph.noteCategory("Valorant");
    ASSERT_TRUE(this->graph.description().contains("Valorant"));

    // Without a stream the curve only covers the last quarter hour
    this->pass(20 * 60);
    EXPECT_FALSE(this->graph.description().contains("Valorant"))
        << this->graph.description().toStdString();
}

TEST_F(FlexiiActivityGraphFixture, ALongStreamDoesNotKeepEverything)
{
    this->graph.followStream("42", this->clock);
    this->chat();

    // More than a day later the oldest counts have been let go of, and
    // nothing falls over
    this->pass(26 * 3600);
    this->chat();
    EXPECT_TRUE(this->graph.description().contains("1 Nachrichten"))
        << this->graph.description().toStdString();
}

TEST_F(FlexiiActivityGraphFixture, ATickEveryFewMinutesAtMost)
{
    // A stream that just started is still drawn over five minutes, so the
    // scale does not jump about
    this->graph.followStream("42", this->clock.addSecs(-30));
    EXPECT_TRUE(this->graph.description().contains("je 1 Min."))
        << this->graph.description().toStdString();

    // Half a day gets hours
    this->graph.followStream("43", this->clock.addSecs(-12 * 3600));
    EXPECT_TRUE(this->graph.description().contains("Std."))
        << this->graph.description().toStdString();
}

namespace {

class FlexiiSendWaitBarFixture : public ::testing::Test
{
protected:
    FlexiiSendWaitBarFixture()
    {
        getSettings()->slowModeBar.setValue(true);
    }

    ~FlexiiSendWaitBarFixture() override
    {
        getSettings()->slowModeBar.setValue(
            getSettings()->slowModeBar.getDefaultValue());
    }

    MockApplication app;
    SendWaitBar bar{nullptr};
};

}  // namespace

TEST(FlexiiSendWaitBar, TheBarStartsOffAsChatterinoHasNoSuchThing)
{
    MockApplication app;
    EXPECT_FALSE(getSettings()->slowModeBar.getDefaultValue());
}

TEST_F(FlexiiSendWaitBarFixture, AWaitShowsTheBarAndTheEndHidesIt)
{
    using namespace std::chrono_literals;

    this->bar.run(10s, 10s);
    EXPECT_FALSE(this->bar.isHidden());

    this->bar.stop();
    EXPECT_TRUE(this->bar.isHidden());
}

TEST_F(FlexiiSendWaitBarFixture, AWaitThatIsOverIsNotShown)
{
    using namespace std::chrono_literals;

    this->bar.run(0s, 10s);
    EXPECT_TRUE(this->bar.isHidden());

    // Nor one that never was
    this->bar.run(5s, 0s);
    EXPECT_TRUE(this->bar.isHidden());
}

TEST_F(FlexiiSendWaitBarFixture, WithTheSwitchOffNothingShows)
{
    using namespace std::chrono_literals;

    getSettings()->slowModeBar.setValue(false);
    this->bar.run(10s, 10s);
    EXPECT_TRUE(this->bar.isHidden());
}

TEST_F(FlexiiSendWaitBarFixture, SwitchingItOffWhileItRunsTakesItAway)
{
    using namespace std::chrono_literals;

    this->bar.run(10s, 10s);
    ASSERT_FALSE(this->bar.isHidden());

    getSettings()->slowModeBar.setValue(false);
    EXPECT_TRUE(this->bar.isHidden());
}
