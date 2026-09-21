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
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include "controllers/moderation/AlertMute.hpp"
#include "controllers/moderation/ModerationAssistant.hpp"
#include "widgets/helper/ActiveBorder.hpp"
#include "widgets/splits/SendWaitBar.hpp"
#include "providers/twitch/TwitchWebBadges.hpp"
#include "util/QMagicEnumTagged.hpp"
#include "widgets/buttons/BadgeButton.hpp"
#include "util/UiStyle.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/splits/HeaderParts.hpp"
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

TEST_F(FlexiiActivityGraphFixture, TheHeaderCanAskForAnotherWidth)
{
    const auto own = this->graph.sizeHint().width();
    EXPECT_EQ(this->graph.ownWidth(), own);

    this->graph.setWantedWidth(own + 80);
    EXPECT_EQ(this->graph.sizeHint().width(), own + 80);
    EXPECT_EQ(this->graph.ownWidth(), own);

    // Nothing asked for is its own width again
    this->graph.setWantedWidth(0);
    EXPECT_EQ(this->graph.sizeHint().width(), own);
    this->graph.setWantedWidth(-40);
    EXPECT_EQ(this->graph.sizeHint().width(), own);
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

namespace {

class FlexiiActiveBorderFixture : public ::testing::Test
{
protected:
    ~FlexiiActiveBorderFixture() override
    {
        auto *s = getSettings();
        s->activeSplitBorderColor.setValue(
            s->activeSplitBorderColor.getDefaultValue());
        s->activeTabBorderColor.setValue(
            s->activeTabBorderColor.getDefaultValue());
    }

    MockApplication app;
};

}  // namespace

TEST_F(FlexiiActiveBorderFixture, BothBordersStartOutTheSameRed)
{
    EXPECT_EQ(activeborder::forSplit(), activeborder::fallback());
    EXPECT_EQ(activeborder::forTab(), activeborder::fallback());
}

TEST_F(FlexiiActiveBorderFixture, TheTabFollowsTheSplitUntilItIsGivenItsOwn)
{
    getSettings()->activeSplitBorderColor.setValue("#1060ff");
    EXPECT_EQ(activeborder::forSplit(), QColor("#1060ff"));
    EXPECT_EQ(activeborder::forTab(), QColor("#1060ff"));

    getSettings()->activeTabBorderColor.setValue("#32e01b");
    EXPECT_EQ(activeborder::forTab(), QColor("#32e01b"));
    // The split keeps its own
    EXPECT_EQ(activeborder::forSplit(), QColor("#1060ff"));
}

TEST_F(FlexiiActiveBorderFixture, AColourThatMakesNoSenseFallsBack)
{
    getSettings()->activeSplitBorderColor.setValue("not a colour");
    EXPECT_EQ(activeborder::forSplit(), activeborder::fallback());
    EXPECT_EQ(activeborder::forTab(), activeborder::fallback());
}

TEST_F(FlexiiActiveBorderFixture, TheBorderAroundTheTabStartsOff)
{
    EXPECT_FALSE(getSettings()->activeTabBorder.getDefaultValue());
}

TEST(FlexiiButtons, EveryButtonStartsWhereItWas)
{
    MockApplication app;
    const auto *s = getSettings();

    // Switching them off is the new part; nothing moves for someone who
    // never opens the page
    EXPECT_TRUE(s->showEmoteButton.getDefaultValue());
    EXPECT_TRUE(s->showClearChatButton.getDefaultValue());
    EXPECT_TRUE(s->showFocusButton.getDefaultValue());
    EXPECT_TRUE(s->showModAssistButton.getDefaultValue());
    EXPECT_TRUE(s->showAlertMuteButton.getDefaultValue());

    // Choosing a badge needs the browser login, so it waits to be asked for
    EXPECT_FALSE(s->showBadgeButton.getDefaultValue());

    // Chatterino's own send button stays off, as it always was
    EXPECT_FALSE(s->showSendButton.getDefaultValue());
}

namespace {

class FlexiiAlertMuteFixture : public ::testing::Test
{
protected:
    ~FlexiiAlertMuteFixture() override
    {
        getSettings()->modAlertMutedChannels.setValue(
            getSettings()->modAlertMutedChannels.getDefaultValue());
    }

    MockApplication app;
};

}  // namespace

TEST_F(FlexiiAlertMuteFixture, NothingIsSilencedToBeginWith)
{
    EXPECT_FALSE(alertmute::isMuted("trymacs"));
    // A split without a channel switches nothing
    EXPECT_FALSE(alertmute::isMuted(""));
}

TEST_F(FlexiiAlertMuteFixture, OneChannelAtATime)
{
    alertmute::setMuted("trymacs", true);
    EXPECT_TRUE(alertmute::isMuted("trymacs"));
    // The others go on as they were
    EXPECT_FALSE(alertmute::isMuted("zarbex"));

    alertmute::setMuted("zarbex", true);
    EXPECT_TRUE(alertmute::isMuted("trymacs"));
    EXPECT_TRUE(alertmute::isMuted("zarbex"));

    alertmute::setMuted("trymacs", false);
    EXPECT_FALSE(alertmute::isMuted("trymacs"));
    EXPECT_TRUE(alertmute::isMuted("zarbex"));
}

TEST_F(FlexiiAlertMuteFixture, TheNameIsTakenAsItComes)
{
    alertmute::setMuted("TryMacs", true);
    EXPECT_TRUE(alertmute::isMuted("trymacs"));
    EXPECT_TRUE(alertmute::isMuted("TRYMACS"));
}

namespace {

class FlexiiAssistantRejectionFixture : public ::testing::Test
{
protected:
    /// A channel of its own for every run: the cases live in a profile the
    /// tests share, and the assistant keeps what it read in memory
    FlexiiAssistantRejectionFixture()
        : channel(QStringLiteral("rejectiontest%1")
                      .arg(QDateTime::currentMSecsSinceEpoch()))
    {
    }

    ~FlexiiAssistantRejectionFixture() override
    {
        QFile::remove(
            QDir(getApp()->getPaths().miscDirectory)
                .absoluteFilePath("moderation-assistant/" + channel + ".json"));
    }

    MockApplication app;
    QString channel;
};

}  // namespace

TEST_F(FlexiiAssistantRejectionFixture,
       WhatAModeratorTurnsDownIsNotSuggestedAgain)
{
    auto &assistant = ModerationAssistant::instance();
    const QString text = "gratis follower bei www.example.net";

    EXPECT_FALSE(assistant.wasRejected(channel, text));

    assistant.reject(channel, text);
    EXPECT_TRUE(assistant.wasRejected(channel, text));

    // Near enough counts as the same - that is how suggestions match too
    EXPECT_TRUE(
        assistant.wasRejected(channel, "gratis follower bei www.example.com"));

    // Something else does not
    EXPECT_FALSE(assistant.wasRejected(channel, "guten morgen zusammen leute"));

    // And it holds for that channel alone
    EXPECT_FALSE(assistant.wasRejected(channel + "other", text));
}

TEST(FlexiiActivityGraphLabels, MarksReadTheWayOneSaysThem)
{
    EXPECT_EQ(ActivityGraph::timeLabel(600), "10 min");
    EXPECT_EQ(ActivityGraph::timeLabel(1800), "30 min");
    EXPECT_EQ(ActivityGraph::timeLabel(3600), "1h");
    EXPECT_EQ(ActivityGraph::timeLabel(7200), "2h");
    EXPECT_EQ(ActivityGraph::timeLabel(5400), "1h30");
    EXPECT_EQ(ActivityGraph::timeLabel(3900), "1h05");
}

namespace {

class FlexiiHeaderPartsFixture : public ::testing::Test
{
protected:
    ~FlexiiHeaderPartsFixture() override
    {
        headerparts::reset();
    }

    MockApplication app;
};

using headerparts::Part;

}  // namespace

TEST(FlexiiHeaderParts, NothingChangedIsChatterinosOrder)
{
    const auto order = headerparts::parseOrder({});
    ASSERT_EQ(order.size(), headerparts::all().size());
    for (size_t i = 0; i < order.size(); i++)
    {
        EXPECT_EQ(order.at(i), headerparts::all().at(i).part);
    }
    EXPECT_TRUE(headerparts::writeOrder(order).isEmpty());
}

TEST(FlexiiHeaderParts, AnOrderIsKeptAsWritten)
{
    const auto order = headerparts::parseOrder(
        "picture,cover,activity,title,mode,moderation,chatters,menu,add");
    EXPECT_EQ(order.at(2), Part::Activity);
    EXPECT_EQ(order.at(3), Part::Title);
    EXPECT_EQ(headerparts::writeOrder(order),
              "picture,cover,activity,title,mode,moderation,chatters,menu,add");
}

TEST(FlexiiHeaderParts, WhatIsMissingGoesWhereItBelongs)
{
    // Saved before there was a curve: it goes behind the title, as it does
    // by default. What is unknown or there twice is left out.
    const auto order = headerparts::parseOrder(
        "menu, nonsense, title, picture, cover, mode, moderation, chatters, "
        "add, menu");
    const std::vector<Part> expected{
        Part::Menu,  Part::Title,      Part::Activity,
        Part::Picture, Part::Cover,    Part::Mode,
        Part::Moderation, Part::Chatters, Part::Add,
    };
    EXPECT_EQ(order, expected);
}

TEST(FlexiiHeaderParts, WithoutASharePlainHalfOfWhatIsFree)
{
    // 600 shared, the title needs 100, the curve's own 200: 300 are free,
    // the curve takes half of them
    EXPECT_EQ(headerparts::curveWidth(600, 100, 200, 0, 100), 350);

    // A title that needs everything leaves the curve its own width
    EXPECT_EQ(headerparts::curveWidth(600, 500, 200, 0, 100), 200);
}

TEST(FlexiiHeaderParts, ADraggedShareIsKeptButTheTitleStaysReadable)
{
    EXPECT_EQ(headerparts::curveWidth(600, 100, 200, 50, 100), 300);

    // At 90 per cent the title would get 60 - it keeps 100
    EXPECT_EQ(headerparts::curveWidth(600, 300, 200, 90, 100), 500);

    // A short title keeps only what it needs
    EXPECT_EQ(headerparts::curveWidth(600, 40, 200, 90, 100), 540);

    // Out of range is brought back in
    EXPECT_EQ(headerparts::curveWidth(600, 40, 200, 150, 100), 540);
    EXPECT_EQ(headerparts::curveWidth(600, 40, 200, 3, 100), 60);
}

TEST_F(FlexiiHeaderPartsFixture, TitleAndMenuCanNotBeSwitchedOff)
{
    headerparts::setShown(Part::Title, false);
    headerparts::setShown(Part::Menu, false);
    EXPECT_TRUE(headerparts::isShown(Part::Title));
    EXPECT_TRUE(headerparts::isShown(Part::Menu));
}

TEST_F(FlexiiHeaderPartsFixture, ChatterinosButtonsStartOnTheExtrasOff)
{
    EXPECT_TRUE(headerparts::isShown(Part::Mode));
    EXPECT_TRUE(headerparts::isShown(Part::Moderation));
    EXPECT_TRUE(headerparts::isShown(Part::Chatters));
    EXPECT_TRUE(headerparts::isShown(Part::Add));
    EXPECT_FALSE(headerparts::isShown(Part::Picture));
    EXPECT_FALSE(headerparts::isShown(Part::Cover));
    EXPECT_FALSE(headerparts::isShown(Part::Activity));
    EXPECT_EQ(getSettings()->splitHeaderActivityShare.getValue(), 0);
}

TEST_F(FlexiiHeaderPartsFixture, OnePictureCanComeOnWithoutTheOther)
{
    headerparts::setShown(Part::Cover, true);
    EXPECT_TRUE(headerparts::isShown(Part::Cover));
    EXPECT_FALSE(headerparts::isShown(Part::Picture));
    EXPECT_TRUE(getSettings()->splitHeaderPictures.getValue());

    headerparts::setShown(Part::Picture, true);
    EXPECT_TRUE(headerparts::isShown(Part::Picture));

    // Both off again is what Look calls off - nothing left behind
    headerparts::setShown(Part::Picture, false);
    headerparts::setShown(Part::Cover, false);
    EXPECT_FALSE(getSettings()->splitHeaderPictures.getValue());
    EXPECT_TRUE(getSettings()->splitHeaderHidden.getValue().isEmpty());
}

TEST_F(FlexiiHeaderPartsFixture, AButtonSwitchedOffStaysOff)
{
    headerparts::setShown(Part::Chatters, false);
    EXPECT_FALSE(headerparts::isShown(Part::Chatters));
    EXPECT_TRUE(headerparts::isShown(Part::Moderation));

    headerparts::setShown(Part::Chatters, true);
    EXPECT_TRUE(headerparts::isShown(Part::Chatters));
    EXPECT_TRUE(getSettings()->splitHeaderHidden.getValue().isEmpty());
}

TEST_F(FlexiiHeaderPartsFixture, StandardPutsEverythingBack)
{
    headerparts::setOrder({Part::Menu, Part::Title});
    headerparts::setShown(Part::Add, false);
    headerparts::setShown(Part::Activity, true);
    getSettings()->splitHeaderActivityShare.setValue(70);

    headerparts::reset();
    EXPECT_TRUE(getSettings()->splitHeaderOrder.getValue().isEmpty());
    EXPECT_TRUE(headerparts::isShown(Part::Add));
    EXPECT_FALSE(headerparts::isShown(Part::Activity));
    EXPECT_EQ(getSettings()->splitHeaderActivityShare.getValue(), 0);
}

namespace {

TwitchChannel::StreamStatus liveStream()
{
    TwitchChannel::StreamStatus status;
    status.live = true;
    status.streamType = "live";
    status.uptime = "2h 13m";
    status.viewerCount = 42;
    status.game = "Just Chatting";
    status.title = "Hallo  Chat";
    return status;
}

}  // namespace

TEST_F(FlexiiHeaderPartsFixture, TheTitleSaysLiveAndNothingMoreAtFirst)
{
    EXPECT_EQ(headerparts::titleAfterName(liveStream()), " (live)");
}

TEST_F(FlexiiHeaderPartsFixture, EachPartOfTheTitleComesOnByItself)
{
    auto *s = getSettings();
    s->headerUptime.setValue(true);
    EXPECT_EQ(headerparts::titleAfterName(liveStream()), " (live) - 2h 13m");

    s->headerUptime.setValue(false);
    s->headerViewerCount.setValue(true);
    s->headerStreamTitle.setValue(true);
    EXPECT_EQ(headerparts::titleAfterName(liveStream()),
              " (live) - 42 - Hallo Chat");

    s->headerGame.setValue(true);
    EXPECT_EQ(headerparts::titleAfterName(liveStream()),
              " (live) - 42 - Just Chatting - Hallo Chat");
}

TEST_F(FlexiiHeaderPartsFixture, LiveCanBeLeftOut)
{
    auto *s = getSettings();
    s->headerLiveMarker.setValue(false);
    EXPECT_TRUE(headerparts::titleAfterName(liveStream()).isEmpty());

    s->headerUptime.setValue(true);
    EXPECT_EQ(headerparts::titleAfterName(liveStream()), " - 2h 13m");

    // Standard brings it back, with the rest off again
    headerparts::reset();
    EXPECT_EQ(headerparts::titleAfterName(liveStream()), " (live)");
}

TEST_F(FlexiiHeaderPartsFixture, TheNameStaysUnlessItIsSwitchedOff)
{
    EXPECT_EQ(headerparts::composeTitle("trymacs", " (live) - 2h 13m", true),
              "trymacs (live) - 2h 13m");
}

TEST_F(FlexiiHeaderPartsFixture, WithoutTheNameTheRestMovesToTheFront)
{
    getSettings()->headerChannelName.setValue(false);
    EXPECT_EQ(headerparts::composeTitle("trymacs", " (live) - 2h 13m", true),
              "(live) - 2h 13m");

    // Without "(live)" the dash it was joined on goes as well
    EXPECT_EQ(headerparts::composeTitle("trymacs", " - 2h 13m - 42", true),
              "2h 13m - 42");

    // Offline there is nothing left - the picture says whose chat it is
    EXPECT_TRUE(headerparts::composeTitle("trymacs", "", true).isEmpty());
}

TEST_F(FlexiiHeaderPartsFixture, WithoutAPictureTheNameStays)
{
    getSettings()->headerChannelName.setValue(false);
    EXPECT_EQ(headerparts::composeTitle("trymacs", " (live)", false),
              "trymacs (live)");
}

namespace {

class FlexiiUiStyleFixture : public ::testing::Test
{
protected:
    ~FlexiiUiStyleFixture() override
    {
        getSettings()->uiStyle.setValue(QStringLiteral("classic"));
    }

    MockApplication app;
};

}  // namespace

TEST_F(FlexiiUiStyleFixture, ClassicIsChatterinoAsItWas)
{
    EXPECT_EQ(getSettings()->uiStyle.getEnum(), UiStyle::Classic);
    EXPECT_EQ(uistyle::tabHeight(), NOTEBOOK_TAB_HEIGHT);
    EXPECT_EQ(uistyle::headerHeight(), ActivityGraph::HEIGHT);
    EXPECT_EQ(uistyle::scrollbarWidth(), 16);
    EXPECT_EQ(uistyle::inputButton(), QSize(24, 18));
    EXPECT_FALSE(uistyle::drawnIcons());
    EXPECT_FALSE(uistyle::compact());
    EXPECT_FALSE(uistyle::flat());
}

TEST_F(FlexiiUiStyleFixture, CompactSavesRoomEverywhere)
{
    getSettings()->uiStyle.setValue(QStringLiteral("compact"));
    EXPECT_TRUE(uistyle::compact());
    EXPECT_LT(uistyle::tabHeight(), NOTEBOOK_TAB_HEIGHT);
    EXPECT_LT(uistyle::headerHeight(), ActivityGraph::HEIGHT);
    EXPECT_LT(uistyle::scrollbarWidth(), 16);
    EXPECT_LT(uistyle::inputButton().height(), 18);
    EXPECT_LT(uistyle::alertMargin(), 12);
    // Classic's shapes and icons
    EXPECT_FALSE(uistyle::drawnIcons());
}

TEST_F(FlexiiUiStyleFixture, FlatKeepsTheSizesAndDrawsItsIcons)
{
    getSettings()->uiStyle.setValue(QStringLiteral("flat"));
    EXPECT_TRUE(uistyle::flat());
    EXPECT_FALSE(uistyle::modern());
    EXPECT_TRUE(uistyle::drawnIcons());
    EXPECT_EQ(uistyle::tabHeight(), NOTEBOOK_TAB_HEIGHT);
    EXPECT_EQ(uistyle::scrollbarWidth(), 16);
}

TEST_F(FlexiiUiStyleFixture, WhatWasSavedBeforeStillReadsTheSame)
{
    // Saved by their English names, shown by German ones where they differ
    EXPECT_EQ(qmagicenum::enumNameString(UiStyle::Modern), "Modern");
    EXPECT_EQ(qmagicenum::enumNameString(UiStyle::Compact), "Compact");
    EXPECT_EQ(qmagicenum::enumDisplayNameString(UiStyle::Classic), "Classic");
    EXPECT_EQ(qmagicenum::enumDisplayNameString(UiStyle::Compact), "Kompakt");
    EXPECT_EQ(qmagicenum::enumDisplayNameString(UiStyle::Flat), "Flach");

    getSettings()->uiStyle.setValue(QStringLiteral("modern"));
    EXPECT_TRUE(uistyle::modern());
    EXPECT_TRUE(uistyle::drawnIcons());

    // Something unknown falls back to Classic
    getSettings()->uiStyle.setValue(QStringLiteral("glas"));
    EXPECT_EQ(getSettings()->uiStyle.getEnum(), UiStyle::Classic);
}

TEST(FlexiiWebBadges, WhatIsPastedIsCleanedUp)
{
    const QString token = "abcdefghijklmnopqrstuvwxyz1234";
    EXPECT_EQ(webbadges::normalize("  " + token + "\n"), token);
    EXPECT_EQ(webbadges::normalize("OAuth " + token), token);
    EXPECT_EQ(webbadges::normalize("\"" + token + "\""), token);
    EXPECT_EQ(webbadges::normalize("auth-token=" + token), token);
}

TEST(FlexiiWebBadges, OnlyWhatLooksLikeALoginIsKept)
{
    EXPECT_TRUE(webbadges::looksLikeToken("abcdefghijklmnopqrstuvwxyz1234"));
    EXPECT_FALSE(webbadges::looksLikeToken(""));
    EXPECT_FALSE(webbadges::looksLikeToken("abc"));
    EXPECT_FALSE(webbadges::looksLikeToken("abcdefghij klmnopqrstuvwxyz1234"));
    EXPECT_FALSE(webbadges::looksLikeToken("abcdefghijklmnopqrstuvwxyz12;4"));
    // Nothing is kept that does not look like one
    EXPECT_FALSE(webbadges::store("abc"));
}

namespace {

QJsonObject badgeJson(const QString &set, const QString &version,
                      const QString &title)
{
    return {
        {"setID", set},
        {"version", version},
        {"title", title},
        {"imageURL", "https://static-cdn.jtvnw.net/badges/" + set},
    };
}

}  // namespace

TEST(FlexiiWebBadges, TheAnswerIsReadIntoWhatCanBeChosen)
{
    const QJsonObject answer{
        {"data",
         QJsonObject{
             {"currentUser",
              QJsonObject{
                  {"selectedBadge", badgeJson("premium", "1", "Prime Gaming")},
                  {"availableBadges",
                   QJsonArray{badgeJson("premium", "1", "Prime Gaming"),
                              badgeJson("glhf-pledge", "1", "GLHF Pledge")}},
              }},
             {"user",
              QJsonObject{
                  {"self",
                   QJsonObject{
                       {"selectedBadge", QJsonValue()},
                       {"availableBadges",
                        QJsonArray{badgeJson("subscriber", "12", "1-Year Sub"),
                                   badgeJson("moderator", "1", "Moderator"),
                                   // Worn everywhere - not listed twice
                                   badgeJson("premium", "1", "Prime Gaming")}},
                   }},
              }},
         }},
    };

    const auto choices = webbadges::parseChoices(answer);
    EXPECT_TRUE(choices.problem.isEmpty());
    ASSERT_EQ(choices.channel.size(), 2);
    EXPECT_EQ(choices.channel.at(0).title, "1-Year Sub");
    EXPECT_EQ(choices.channel.at(1).setID, "moderator");
    ASSERT_EQ(choices.global.size(), 2);
    EXPECT_FALSE(choices.channelWorn.has_value());
    ASSERT_TRUE(choices.globalWorn.has_value());

    // Nothing of the channel's worn: the global one is what is seen
    ASSERT_TRUE(choices.shown().has_value());
    EXPECT_EQ(choices.shown()->setID, "premium");
}

TEST(FlexiiWebBadges, ALoginTwitchDoesNotTakeIsSaid)
{
    const QJsonObject answer{
        {"data", QJsonObject{{"currentUser", QJsonValue()}}},
        {"errors",
         QJsonArray{QJsonObject{{"message", "unauthenticated"}}}},
    };
    const auto choices = webbadges::parseChoices(answer);
    EXPECT_TRUE(choices.problem.contains("unauthenticated"));
    EXPECT_TRUE(choices.channel.empty());
    EXPECT_FALSE(choices.shown().has_value());
}

TEST(FlexiiWebBadges, TheMenuStaysOnTheScreenOfItsButton)
{
    // Two screens, the second one above the first
    const QRect lower(0, 1080, 1920, 1080);
    const QSize menu(220, 300);

    // Room above the button: it opens upwards
    const QRect low(900, 2000, 24, 18);
    EXPECT_EQ(BadgeButton::placeMenu(low, menu, lower), QPoint(900, 1700));

    // Near the top of its screen it would reach onto the one above - it
    // opens downwards instead
    const QRect high(900, 1200, 24, 18);
    const auto at = BadgeButton::placeMenu(high, menu, lower);
    EXPECT_EQ(at, QPoint(900, 1218));
    EXPECT_TRUE(lower.contains(QRect(at, menu)));

    // At the right edge it moves in rather than onto the next screen
    const QRect right(1910, 2000, 24, 18);
    EXPECT_EQ(BadgeButton::placeMenu(right, menu, lower).x(), 1920 - 220);
}

TEST(FlexiiWebBadges, AChoiceJustMadeCountsUntilTwitchCatchesUp)
{
    const auto now = QDateTime::currentDateTimeUtc();
    const webbadges::Badge sub{.setID = "subscriber",
                               .version = "12",
                               .title = "1-Year Sub",
                               .image = {}};
    const webbadges::Badge prime{
        .setID = "premium", .version = "1", .title = "Prime", .image = {}};

    // Twitch still says nothing is worn in the channel
    webbadges::Choices choices;
    webbadges::remember("flexii-test-channel", sub, false, now);
    webbadges::applyRecent(choices, "flexii-test-channel", now.addSecs(20));
    ASSERT_TRUE(choices.channelWorn.has_value());
    EXPECT_EQ(choices.channelWorn->title, "1-Year Sub");

    // Only for that channel
    webbadges::Choices elsewhere;
    webbadges::applyRecent(elsewhere, "flexii-other-channel", now);
    EXPECT_FALSE(elsewhere.channelWorn.has_value());

    // Long after, what Twitch says counts again
    webbadges::Choices later;
    webbadges::applyRecent(later, "flexii-test-channel", now.addSecs(600));
    EXPECT_FALSE(later.channelWorn.has_value());

    // One worn everywhere counts in every channel
    webbadges::remember({}, prime, true, now);
    webbadges::Choices anywhere;
    webbadges::applyRecent(anywhere, "flexii-other-channel", now);
    ASSERT_TRUE(anywhere.globalWorn.has_value());
    EXPECT_EQ(anywhere.globalWorn->setID, "premium");
}
