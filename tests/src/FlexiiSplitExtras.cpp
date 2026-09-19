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

    MockApplication app;
    ChannelPtr channel;
    ActivityGraph graph{nullptr};
};

}  // namespace

TEST_F(FlexiiActivityGraphFixture, TheCurveReachesBackAQuarterOfAnHour)
{
    EXPECT_EQ(int(ActivityGraph::SPANS) * ActivityGraph::SPAN_SECONDS, 900);
    EXPECT_TRUE(this->graph.description().contains("15 Minuten"));
    // The line of time under the curve is explained where the numbers are
    EXPECT_TRUE(this->graph.description().contains("je Minute"));
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

TEST_F(FlexiiActivityGraphFixture, TheFirstCategoryIsNotAChange)
{
    // A stream starting is not a change from something else
    this->graph.noteCategory("Just Chatting");
    EXPECT_FALSE(this->graph.description().contains("Just Chatting"));
}

TEST_F(FlexiiActivityGraphFixture, AChangeOfCategoryIsMarked)
{
    this->graph.noteCategory("Just Chatting");
    this->graph.noteCategory("Valorant");

    const auto tooltip = this->graph.description();
    EXPECT_TRUE(tooltip.contains("Valorant")) << tooltip.toStdString();
    // Just now, so half a minute at most
    EXPECT_TRUE(tooltip.contains("vor 0 Min.")) << tooltip.toStdString();

    // The same category again changes nothing
    this->graph.noteCategory("Valorant");
    EXPECT_EQ(this->graph.description().count("Valorant"), 1);
}

TEST_F(FlexiiActivityGraphFixture, AMarkMovesAlongAndFallsOffTheEnd)
{
    this->graph.noteCategory("Just Chatting");
    this->graph.noteCategory("Valorant");

    for (int i = 0; i < 4; i++)
    {
        this->graph.shift();
    }
    // Four half minutes back
    EXPECT_TRUE(this->graph.description().contains("vor 2 Min."))
        << this->graph.description().toStdString();

    for (size_t i = 0; i < ActivityGraph::SPANS; i++)
    {
        this->graph.shift();
    }
    EXPECT_FALSE(this->graph.description().contains("Valorant"));
}

TEST_F(FlexiiActivityGraphFixture, ShiftingLeavesTheNewestSpanEmpty)
{
    this->chat();
    this->graph.shift();
    this->chat();
    EXPECT_TRUE(this->graph.description().contains("2 Nachrichten"));

    for (size_t i = 0; i < ActivityGraph::SPANS; i++)
    {
        this->graph.shift();
    }
    EXPECT_TRUE(this->graph.description().contains("0 Nachrichten"));
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
