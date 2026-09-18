// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for what ChattiFlexii adds on top of Chatterino. They run on every
// push before a download is published.

#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/ModHighlights.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "controllers/moderation/StepEscalation.hpp"
#include "messages/layouts/AlternateBackground.hpp"
#include "messages/layouts/MessageLayout.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "Test.hpp"
#include "util/ProfileSetup.hpp"
#include "util/ProfileSync.hpp"
#include "util/SpellingVariants.hpp"
#include "util/Twitch.hpp"
#include "util/UpdateCheck.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUrl>

#include <optional>
#include <vector>

using namespace chatterino;

namespace {

void writeJson(const QString &path, const QJsonObject &object)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(object).toJson());
}

void writeText(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(text);
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QString tabBarColor(const QJsonObject &settings)
{
    return settings.value("appearance")
        .toObject()
        .value("tabBar")
        .toObject()
        .value("backgroundColor")
        .toString();
}

/// A profile as it lies on disk: logged in as @a login, with a tab bar
/// colour to tell it apart
void makeProfile(const QString &root, const QString &login,
                 const QString &color)
{
    writeJson(
        root + "/Settings/settings.json",
        QJsonObject{
            {"accounts",
             QJsonObject{
                 {"current", login},
                 {"uid1", QJsonObject{{"username", login},
                                      {"oauthToken", "SECRET-" + login}}},
             }},
            {"appearance",
             QJsonObject{{"tabBar", QJsonObject{{"backgroundColor", color}}}}},
        });
    writeText(root + "/Settings/settings.json.bkp-1", "{}");
    writeText(root + "/Misc/probe.txt", login.toUtf8());
}

}  // namespace

// ----- timeout steps -----

TEST(FlexiiSteps, RepeatedMessageSteps)
{
    EXPECT_EQ(RepeatSpamDetector::parseSteps("30s, 1m, 5m, 10m, 30m"),
              (std::vector<int>{30, 60, 300, 600, 1800}));
    EXPECT_EQ(RepeatSpamDetector::parseSteps("1h30m"), (std::vector<int>{5400}));
    EXPECT_EQ(RepeatSpamDetector::parseSteps("45"), (std::vector<int>{45}));
    EXPECT_EQ(RepeatSpamDetector::parseSteps("2D"),
              (std::vector<int>{2 * 24 * 60 * 60}));
}

TEST(FlexiiSteps, TimeoutsStopAtTwoWeeks)
{
    EXPECT_EQ(RepeatSpamDetector::parseSteps("3w"),
              (std::vector<int>{14 * 24 * 60 * 60}));
}

TEST(FlexiiSteps, AListThatDoesNotReadIsRejectedWhole)
{
    EXPECT_TRUE(RepeatSpamDetector::parseSteps("").empty());
    EXPECT_TRUE(RepeatSpamDetector::parseSteps("30s, soon").empty());
    EXPECT_TRUE(RepeatSpamDetector::parseSteps("0s").empty());
    EXPECT_TRUE(RepeatSpamDetector::parseSteps("5x").empty());
}

TEST(FlexiiSteps, EmoteSpamStepsDeleteFirst)
{
    const auto del = EmoteSpamDetector::DELETE;
    EXPECT_EQ(EmoteSpamDetector::parseSteps("löschen, löschen, 30s"),
              (std::vector<int>{del, del, 30}));
    EXPECT_EQ(EmoteSpamDetector::parseSteps("delete, del, loeschen, 1m"),
              (std::vector<int>{del, del, del, 60}));
    EXPECT_TRUE(EmoteSpamDetector::parseSteps("löschen, weg").empty());
}

// ----- stepping up -----

TEST(FlexiiEscalation, FirstOfferIsTheFirstStep)
{
    StepEscalation escalation;
    EXPECT_EQ(escalation.more(1, 3), 0);
}

TEST(FlexiiEscalation, IgnoredRepeatsStepUpEveryThree)
{
    StepEscalation escalation;
    EXPECT_EQ(escalation.more(1, 3), 0);
    // Nobody acts - they carry on
    EXPECT_EQ(escalation.more(1, 3), std::nullopt);
    EXPECT_EQ(escalation.more(1, 3), std::nullopt);
    EXPECT_EQ(escalation.more(1, 3), 1);
    EXPECT_EQ(escalation.more(1, 3), std::nullopt);
    EXPECT_EQ(escalation.more(1, 3), std::nullopt);
    EXPECT_EQ(escalation.more(1, 3), 2);
    EXPECT_EQ(escalation.actions, 0);
}

TEST(FlexiiEscalation, IgnoredEmoteFloodsStepUpAsManyEmotesAgain)
{
    StepEscalation escalation;
    // The alert starts at 8 emotes
    EXPECT_EQ(escalation.more(9, 8), 0);
    EXPECT_EQ(escalation.more(3, 8), std::nullopt);
    EXPECT_EQ(escalation.more(4, 8), std::nullopt);
    EXPECT_EQ(escalation.more(2, 8), 1);
}

TEST(FlexiiEscalation, AnActionBringsTheNextStepAtOnce)
{
    StepEscalation escalation;
    EXPECT_EQ(escalation.more(1, 3), 0);
    escalation.actedOn();
    EXPECT_EQ(escalation.more(1, 3), 1);
    EXPECT_EQ(escalation.actions, 1);
}

TEST(FlexiiEscalation, ActionsAndIgnoredAlertsAddUp)
{
    StepEscalation escalation;
    EXPECT_EQ(escalation.more(1, 3), 0);
    escalation.more(1, 3);
    escalation.more(1, 3);
    EXPECT_EQ(escalation.more(1, 3), 1);  // ignored, stepped up
    escalation.actedOn();                  // the 1 minute was given
    EXPECT_EQ(escalation.more(1, 3), 2);  // next one straight away
}

// ----- Twitch names -----

TEST(FlexiiNames, TwitchLogins)
{
    EXPECT_TRUE(isValidTwitchLogin("zarbex"));
    EXPECT_TRUE(isValidTwitchLogin("fx_flexii"));
    EXPECT_TRUE(isValidTwitchLogin("a"));
    EXPECT_TRUE(isValidTwitchLogin(QString(25, 'a')));

    EXPECT_FALSE(isValidTwitchLogin(""));
    EXPECT_FALSE(isValidTwitchLogin(QString(26, 'a')));
    EXPECT_FALSE(isValidTwitchLogin("Zarbex"));
    EXPECT_FALSE(isValidTwitchLogin("zar bex"));
    EXPECT_FALSE(isValidTwitchLogin("zar-bex"));
    EXPECT_FALSE(isValidTwitchLogin("#zarbex"));
}

TEST(FlexiiNames, CaptionNamesNeedTheAt)
{
    EXPECT_EQ(profilepictures::loginOf("@zarbex"), "zarbex");
    EXPECT_EQ(profilepictures::loginOf("@Zarbex"), "zarbex");
    EXPECT_EQ(profilepictures::loginOf("zarbex"), "");
    EXPECT_EQ(profilepictures::loginOf("@"), "");
    EXPECT_EQ(profilepictures::loginOf("@zar-bex"), "");
    EXPECT_EQ(profilepictures::loginOf("Mod"), "");
}

// ----- reason colours -----

TEST(FlexiiColors, PickedColoursAreLitUp)
{
    const QColor fallback("#1ae8ff");

    const auto red = ModAlertPopup::vividColor(QColor(120, 20, 20), fallback);
    EXPECT_GE(red.hsvSaturationF(), 0.79F);
    EXPECT_FLOAT_EQ(red.valueF(), 1.0F);
    EXPECT_EQ(red.hsvHue(), QColor(120, 20, 20).hsvHue());
}

TEST(FlexiiColors, GreyBlackAndWhiteFallBack)
{
    const QColor fallback("#1ae8ff");
    for (const auto &colorless :
         {QColor(Qt::gray), QColor(Qt::black), QColor(Qt::white), QColor()})
    {
        EXPECT_EQ(ModAlertPopup::vividColor(colorless, fallback).hsvHue(),
                  fallback.hsvHue());
    }
}

// ----- alert sounds -----

TEST(FlexiiSounds, NoChoicePlaysThePing)
{
    EXPECT_EQ(ModAlertPopup::soundUrl(""), QUrl("qrc:/sounds/ping2.wav"));
}

TEST(FlexiiSounds, AFileThatIsGoneFallsBackToThePing)
{
    EXPECT_EQ(ModAlertPopup::soundUrl("/nowhere/at/all.wav"),
              QUrl("qrc:/sounds/ping2.wav"));
}

TEST(FlexiiSounds, AChosenFileIsPlayedAsItIs)
{
    QTemporaryDir dir;
    const auto path = dir.path() + "/mine.wav";
    writeText(path, "RIFF");
    EXPECT_EQ(ModAlertPopup::soundUrl(path), QUrl::fromLocalFile(path));
}

TEST(FlexiiSounds, EveryBuiltInSoundComesWithTheApp)
{
    for (const auto &[value, name] : ModAlertPopup::builtInSounds())
    {
        const auto file = ":/sounds/alert-" + value.mid(8) + ".wav";
        EXPECT_TRUE(QFile::exists(file)) << file;
    }
}

// ----- mod highlights -----

TEST(FlexiiModHighlights, ListedBotsAreLeftOut)
{
    const QString listed = "fossabot, Nightbot;@moobot  #aecrobot";
    EXPECT_TRUE(ModHighlights::isExcludedMod("fossabot", listed, false));
    EXPECT_TRUE(ModHighlights::isExcludedMod("nightbot", listed, false));
    EXPECT_TRUE(ModHighlights::isExcludedMod("moobot", listed, false));
    EXPECT_TRUE(ModHighlights::isExcludedMod("aecrobot", listed, false));
    EXPECT_FALSE(ModHighlights::isExcludedMod("tobini", listed, false));
}

TEST(FlexiiModHighlights, NamesEndingInBotOnlyWhenSwitchedOn)
{
    EXPECT_TRUE(ModHighlights::isExcludedMod("sery_bot", "", true));
    EXPECT_FALSE(ModHighlights::isExcludedMod("sery_bot", "", false));
    EXPECT_FALSE(ModHighlights::isExcludedMod("botanist", "", true));
}

// ----- export & import -----

TEST(FlexiiProfile, ExportLeavesTheLoginAndBackupsBehind)
{
    QTemporaryDir source;
    QTemporaryDir exports;
    makeProfile(source.path(), "alice", "#112233");

    QString error;
    const auto folder =
        exportProfileTo(source.path(), exports.path(), false, error);
    ASSERT_FALSE(folder.isEmpty()) << error;
    EXPECT_TRUE(isProfileExport(folder));

    const auto exported = readJson(folder + "/Settings/settings.json");
    EXPECT_FALSE(exported.contains("accounts"));
    EXPECT_EQ(tabBarColor(exported), "#112233");
    EXPECT_FALSE(QFileInfo::exists(folder + "/Settings/settings.json.bkp-1"));
    EXPECT_TRUE(QFileInfo::exists(folder + "/Misc/probe.txt"));
}

TEST(FlexiiProfile, ExportCanTakeTheLoginAlong)
{
    QTemporaryDir source;
    QTemporaryDir exports;
    makeProfile(source.path(), "alice", "#112233");

    QString error;
    const auto folder =
        exportProfileTo(source.path(), exports.path(), true, error);
    ASSERT_FALSE(folder.isEmpty()) << error;

    const auto exported = readJson(folder + "/Settings/settings.json");
    EXPECT_EQ(exported.value("accounts").toObject().value("current").toString(),
              "alice");
}

TEST(FlexiiProfile, ImportTakesTheSetupAndKeepsTheLoginHere)
{
    QTemporaryDir source;
    QTemporaryDir exports;
    QTemporaryDir target;
    makeProfile(source.path(), "alice", "#112233");
    makeProfile(target.path(), "bob", "#445566");

    QString error;
    const auto folder =
        exportProfileTo(source.path(), exports.path(), false, error);
    ASSERT_FALSE(folder.isEmpty()) << error;
    ASSERT_TRUE(stageProfileImportAt(target.path(), folder, error)) << error;
    applyPendingImportAt(target.path());

    const auto imported = readJson(target.path() + "/Settings/settings.json");
    EXPECT_EQ(tabBarColor(imported), "#112233");
    EXPECT_EQ(imported.value("accounts").toObject().value("current").toString(),
              "bob");
    EXPECT_FALSE(QFileInfo::exists(target.path() + "/PendingImport"));

    // What was there before is kept aside, not lost
    const auto backups =
        QDir(target.path())
            .entryList({"Sicherung vor Import*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    ASSERT_EQ(backups.size(), 1);
    EXPECT_EQ(tabBarColor(readJson(target.path() + "/" + backups.front() +
                                   "/Settings/settings.json")),
              "#445566");
}

TEST(FlexiiProfile, NoImportIsAppliedWithoutAnExport)
{
    QTemporaryDir target;
    makeProfile(target.path(), "bob", "#445566");
    writeText(target.path() + "/PendingImport/Settings/settings.json", "{}");

    applyPendingImportAt(target.path());

    EXPECT_EQ(tabBarColor(readJson(target.path() + "/Settings/settings.json")),
              "#445566");
    EXPECT_FALSE(QFileInfo::exists(target.path() + "/PendingImport"));
}

namespace {

std::shared_ptr<MessageLayout> messageFrom(const QString &login)
{
    auto message = std::make_shared<Message>();
    message->loginName = login;
    return std::make_shared<MessageLayout>(message);
}

/// Lays out messages from these senders one after another the way the chat
/// does, and returns which ones got the alternate background
std::vector<bool> backgrounds(const std::vector<QString> &senders,
                              bool bySender)
{
    std::vector<bool> alternate;
    std::shared_ptr<MessageLayout> previous;
    for (const auto &sender : senders)
    {
        auto layout = messageFrom(sender);
        layout->flags.set(MessageLayoutFlag::AlternateBackground,
                          alternatebg::alternateNextTo(
                              previous.get(), *layout->getMessage(), bySender));
        alternate.push_back(
            layout->flags.has(MessageLayoutFlag::AlternateBackground));
        previous = layout;
    }
    return alternate;
}

}  // namespace

TEST(FlexiiReadability, EveryOtherMessageStandsOut)
{
    EXPECT_EQ(backgrounds({"anna", "anna", "ben", "ben", "ben"}, false),
              (std::vector<bool>{false, true, false, true, false}));
}

TEST(FlexiiReadability, OneSendersMessagesInARowKeepOneBackground)
{
    EXPECT_EQ(backgrounds({"anna", "anna", "ben", "ben", "ben", "anna"}, true),
              (std::vector<bool>{false, false, true, true, true, false}));
}

TEST(FlexiiReadability, SystemMessagesInARowCountAsOneSender)
{
    EXPECT_EQ(backgrounds({"anna", "", "", "anna"}, true),
              (std::vector<bool>{false, true, true, false}));
}

TEST(FlexiiReadability, ThemeShadeUntilAStrengthOrColourIsChosen)
{
    const QColor regular("#191919");
    const QColor theme("#222222");
    EXPECT_EQ(alternatebg::background(regular, theme, 0, QColor()), theme);
}

TEST(FlexiiReadability, NeutralIsLighterOnDarkAndDarkerOnLight)
{
    const auto dark =
        alternatebg::background(QColor("#191919"), QColor(), 20, QColor());
    EXPECT_EQ(dark, QColor(71, 71, 71));

    const auto light =
        alternatebg::background(QColor("#ffffff"), QColor(), 20, QColor());
    EXPECT_EQ(light, QColor(204, 204, 204));
}

TEST(FlexiiReadability, AColourTintsAsStronglyAsChosen)
{
    const QColor regular("#191919");
    const QColor violet("#9146ff");
    EXPECT_EQ(alternatebg::background(regular, QColor(), 20, violet),
              QColor(49, 34, 71));
    // Left to the theme, a colour still shows - at the gentlest strength
    EXPECT_EQ(alternatebg::background(regular, QColor(), 0, violet),
              alternatebg::background(regular, QColor(),
                                      alternatebg::TINT_STRENGTH, violet));
}

namespace {

bool finds(const QString &word, const QString &message,
           const spelling::Options &options = {})
{
    return spelling::compile(spelling::pattern(word, options))
        .match(message)
        .hasMatch();
}

}  // namespace

TEST(FlexiiSpelling, FindsTheWordAsItIs)
{
    EXPECT_TRUE(finds("follower", "gratis follower hier"));
    EXPECT_TRUE(finds("follower", "FOLLOWER"));
}

TEST(FlexiiSpelling, FindsItDisguised)
{
    for (const auto *message :
         {"f0ll0w3r", "f\u043Ellower", "f o l l o w e r", "f.o.l.l.o.w.e.r",
          "fooollower", "folower", "f\u00F3llower", "fol.lower"})
    {
        EXPECT_TRUE(finds("follower", QString::fromUtf8(message))) << message;
    }
}

TEST(FlexiiSpelling, LeavesOtherWordsAlone)
{
    EXPECT_FALSE(finds("follower", "flower"));
    EXPECT_FALSE(finds("lol", "lollipop"));
    EXPECT_FALSE(finds("follower", "followers"));

    spelling::Options inside;
    inside.wholeWord = false;
    EXPECT_TRUE(finds("follower", "followers", inside));
}

TEST(FlexiiSpelling, UmlautsWrittenOut)
{
    for (const auto *message : {"daemlich", "damlich", "D\u00C4MLICH", "d4emlich"})
    {
        EXPECT_TRUE(finds(QString::fromUtf8("d\u00E4mlich"),
                          QString::fromUtf8(message)))
            << message;
    }
    EXPECT_TRUE(finds(QString::fromUtf8("d\u00E4mlich"), "d.a.e.m.l.i.c.h"));
    EXPECT_TRUE(finds(QString::fromUtf8("schei\u00DFe"), "scheisse"));
    EXPECT_TRUE(finds(QString::fromUtf8("schei\u00DFe"), "s c h e i s s e"));
}

TEST(FlexiiSpelling, PhrasesWithOrWithoutTheGap)
{
    for (const auto *message :
         {"gratis follower", "gratis-follower", "gratisfollower",
          "gratis   f0llower"})
    {
        EXPECT_TRUE(finds("gratis follower", message)) << message;
    }
}

TEST(FlexiiSpelling, OnlyWhatIsSwitchedOn)
{
    const spelling::Options none{false, false, false, false, false};
    EXPECT_EQ(spelling::pattern("lol", none), "(?#lol)lol");
    EXPECT_TRUE(finds("lol", "xlolx", none));
    EXPECT_FALSE(finds("lol", "l0l", none));
    EXPECT_FALSE(finds("lol", "l o l", none));
}

TEST(FlexiiSpelling, EveryExampleIsFound)
{
    for (const auto *word : {"follower", "d\u00E4mlich", "gratis follower",
                             "lol", "scheisse"})
    {
        const auto text = QString::fromUtf8(word);
        const auto examples = spelling::examples(text, {});
        EXPECT_FALSE(examples.empty()) << word;
        for (const auto &example : examples)
        {
            EXPECT_TRUE(finds(text, example.text))
                << word << " -> " << example.text.toStdString();
        }
    }
}

TEST(FlexiiSpelling, AnyInputGivesAValidPattern)
{
    for (const auto *word : {"a+b (c)] ^-\\", "[x]", "$$$", "#1 fan"})
    {
        const auto pattern = spelling::pattern(QString::fromUtf8(word), {});
        EXPECT_TRUE(spelling::compile(pattern).isValid()) << word;
    }
    EXPECT_TRUE(spelling::pattern("   ", {}).isEmpty());
}

namespace {

QJsonObject releaseJson(const QString &body)
{
    return {
        {"body", body},
        {"published_at", "2026-09-19T10:00:00Z"},
        {"assets",
         QJsonArray{
             QJsonObject{
                 {"name", "ChattiFlexii-windows-x64.zip"},
                 {"browser_download_url", "https://example.com/win.zip"},
                 {"updated_at", "2026-09-19T10:05:00Z"},
             },
             QJsonObject{
                 {"name", "ChattiFlexii-macOS-arm64.dmg"},
                 {"browser_download_url", "https://example.com/mac.dmg"},
                 {"updated_at", "2026-09-19T10:06:00Z"},
             },
         }},
    };
}

const QString COMMIT = "8efbc9422aa6d0c2c1f3b3a1f0e8d7c6b5a49382";

}  // namespace

TEST(FlexiiUpdate, ReadsTheReleaseForThisComputer)
{
    const auto release = updatecheck::parseRelease(
        releaseJson("Automatically built from `flexii-7.5.5`\n(commit " +
                    COMMIT + ")."),
        "ChattiFlexii-macOS-arm64.dmg");
    ASSERT_TRUE(release.has_value());
    EXPECT_EQ(release->commit, COMMIT);
    EXPECT_EQ(release->downloadUrl, "https://example.com/mac.dmg");
    EXPECT_EQ(release->published,
              QDateTime::fromString("2026-09-19T10:06:00Z", Qt::ISODate));
}

TEST(FlexiiUpdate, NothingWithoutACommitOrADownload)
{
    EXPECT_FALSE(updatecheck::parseRelease(releaseJson("no commit here"),
                                           "ChattiFlexii-macOS-arm64.dmg"));
    EXPECT_FALSE(updatecheck::parseRelease(
        releaseJson("(commit " + COMMIT + ")"), "ChattiFlexii-linux.AppImage"));
}

TEST(FlexiiUpdate, NewerOnlyWhenBuiltFromAnotherCommit)
{
    const updatecheck::Release release{COMMIT, "https://example.com", {}};
    // The app knows its commit in short
    EXPECT_FALSE(updatecheck::isNewer(release, "8efbc9422"));
    EXPECT_FALSE(updatecheck::isNewer(release, COMMIT));
    EXPECT_TRUE(updatecheck::isNewer(release, "a9ca32cc0"));
    // Nothing to compare with: no offer
    EXPECT_FALSE(updatecheck::isNewer(release, "GIT-REPOSITORY-NOT-FOUND"));
    EXPECT_FALSE(updatecheck::isNewer(release, ""));
}

namespace {

profilesync::Shared sharedFrom(const QString &computer, const QString &written,
                               const QString &fingerprint)
{
    return {computer, computer + " name", written, fingerprint};
}

}  // namespace

TEST(FlexiiSync, WritesWhatChanged)
{
    using profilesync::Step;
    // Nothing in the folder yet
    EXPECT_EQ(profilesync::decide(std::nullopt, "mac", "", "A", ""),
              Step::Write);
    // Its own setup there, and nothing changed since
    EXPECT_EQ(profilesync::decide(sharedFrom("mac", "t1", "A"), "mac", "t1",
                                  "A", "A"),
              Step::Nothing);
    // Changed since
    EXPECT_EQ(profilesync::decide(sharedFrom("mac", "t1", "A"), "mac", "t1",
                                  "B", "A"),
              Step::Write);
}

TEST(FlexiiSync, OffersAnotherComputersNewerSetupAndNeverWritesOverIt)
{
    using profilesync::Step;
    // The MacBook left something new - even with changes here, it is offered
    // rather than written over
    EXPECT_EQ(profilesync::decide(sharedFrom("macbook", "t2", "C"), "mac", "t1",
                                  "B", "A"),
              Step::Offer);
    // Once answered, what changed here is written again
    EXPECT_EQ(profilesync::decide(sharedFrom("macbook", "t2", "C"), "mac", "t2",
                                  "B", "A"),
              Step::Write);
    // The same as here: nothing to take
    EXPECT_EQ(profilesync::decide(sharedFrom("macbook", "t2", "B"), "mac", "t1",
                                  "B", "A"),
              Step::Settle);
}

TEST(FlexiiSync, FingerprintLeavesOutLoginAndWindowPlaces)
{
    QTemporaryDir root;
    ASSERT_TRUE(root.isValid());
    const auto settings = root.path() + "/Settings/settings.json";
    const auto layout = root.path() + "/Settings/window-layout.json";

    const QJsonObject tabs{{"windows",
                            QJsonArray{QJsonObject{{"type", "main"},
                                                   {"x", 10},
                                                   {"y", 20},
                                                   {"width", 800},
                                                   {"tabs", QJsonArray{"a"}}}}}};
    writeJson(settings,
              {{"appearance", QJsonObject{{"uiStyle", "modern"}}},
               {"accounts", QJsonObject{{"current", "fx_flexii"}}},
               {"sync", QJsonObject{{"base", "t1"}}},
               {"backup", QJsonObject{{"last", "x"}, {"days", 7}}}});
    writeJson(layout, tabs);
    const auto before = profilesync::fingerprint(root.path());

    // Another login, another sync state, another backup time, the window
    // elsewhere: the same setup
    writeJson(settings,
              {{"appearance", QJsonObject{{"uiStyle", "modern"}}},
               {"accounts", QJsonObject{{"current", "someone"}}},
               {"sync", QJsonObject{{"base", "t9"}}},
               {"backup", QJsonObject{{"last", "y"}, {"days", 7}}}});
    writeJson(layout,
              {{"windows",
                QJsonArray{QJsonObject{{"type", "main"},
                                       {"x", -3000},
                                       {"y", 5},
                                       {"width", 1200},
                                       {"tabs", QJsonArray{"a"}}}}}});
    EXPECT_EQ(profilesync::fingerprint(root.path()), before);

    // A setting, or a tab, changed: another setup
    writeJson(settings,
              {{"appearance", QJsonObject{{"uiStyle", "classic"}}},
               {"backup", QJsonObject{{"days", 7}}}});
    writeJson(layout, tabs);
    EXPECT_NE(profilesync::fingerprint(root.path()), before);
}
