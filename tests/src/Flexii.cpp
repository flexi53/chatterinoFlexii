// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for what ChattiFlexii adds on top of Chatterino. They run on every
// push before a download is published.

#include "controllers/highlights/HighlightBadge.hpp"
#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/WordAlertDetector.hpp"
#include "controllers/moderation/ModHighlights.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "controllers/moderation/StepEscalation.hpp"
#include "messages/layouts/AlternateBackground.hpp"
#include "messages/layouts/ChatEvent.hpp"
#include "messages/layouts/MessageLayout.hpp"
#include "messages/layouts/MessageRole.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "Test.hpp"
#include "util/ProfileSetup.hpp"
#include "util/ProfileSync.hpp"
#include "util/SelfUpdate.hpp"
#include "util/SettingsSnapshots.hpp"
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
#include <QProcess>
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
                 {"digest", "sha256:0963AD91E03A2ECE"},
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
    // What the download is checked against, before it replaces the app
    EXPECT_EQ(release->sha256, "0963ad91e03a2ece");
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

TEST(FlexiiSync, TakingASetupKeepsTheWindowsWhereTheyAreHere)
{
    QTemporaryDir local;
    QTemporaryDir staged;
    ASSERT_TRUE(local.isValid() && staged.isValid());

    // Here: the main window on the laptop screen, one popup, alerts placed
    writeJson(local.path() + "/Settings/window-layout.json",
              {{"windows",
                QJsonArray{
                    QJsonObject{{"type", "main"},
                                {"x", 0},
                                {"y", 25},
                                {"width", 1400},
                                {"height", 860},
                                {"focus", true},
                                {"tabs", QJsonArray{"here"}}},
                    QJsonObject{{"type", "popup"},
                                {"x", 100},
                                {"y", 100},
                                {"width", 500},
                                {"height", 400},
                                {"tabs", QJsonArray{"popup here"}}},
                }}});
    writeJson(local.path() + "/Settings/settings.json",
              {{"moderation",
                QJsonObject{{"alerts", QJsonObject{{"x", 40},
                                                   {"y", 50},
                                                   {"positionSaved", true},
                                                   {"sound", false}}}}},
               {"appearance",
                QJsonObject{{"uiStyle", "classic"}, {"focusMode", true}}}});

    // From the other computer: on its second screen, maximized, two popups,
    // other tabs and settings
    writeJson(staged.path() + "/Settings/window-layout.json",
              {{"windows",
                QJsonArray{
                    QJsonObject{{"type", "main"},
                                {"x", -3000},
                                {"y", -400},
                                {"width", 1080},
                                {"height", 1215},
                                {"state", "maximized"},
                                {"tabs", QJsonArray{"there"}}},
                    QJsonObject{{"type", "popup"},
                                {"x", -3000},
                                {"y", 997},
                                {"tabs", QJsonArray{"popup there"}}},
                    QJsonObject{{"type", "popup"},
                                {"x", -2000},
                                {"y", 10},
                                {"tabs", QJsonArray{"second popup"}}},
                }}});
    writeJson(staged.path() + "/Settings/settings.json",
              {{"moderation",
                QJsonObject{{"alerts", QJsonObject{{"x", -2770},
                                                   {"y", 63},
                                                   {"width", 603},
                                                   {"positionSaved", true},
                                                   {"sound", true}}}}},
               {"appearance", QJsonObject{{"uiStyle", "modern"}}}});

    const auto fingerprint = profilesync::fingerprint(staged.path());
    profilesync::keepWindowPlaces(local.path(), staged.path());
    // Still the same setup - or the two computers would hand it back and
    // forth for ever
    EXPECT_EQ(profilesync::fingerprint(staged.path()), fingerprint);

    const auto layout =
        readJson(staged.path() + "/Settings/window-layout.json");
    const auto windows = layout["windows"].toArray();
    ASSERT_EQ(windows.size(), 3);
    const auto main = windows[0].toObject();
    EXPECT_EQ(main["x"].toInt(), 0);
    EXPECT_EQ(main["width"].toInt(), 1400);
    EXPECT_FALSE(main.contains("state"));
    // Only the chats in this window here, whatever it is there
    EXPECT_TRUE(main["focus"].toBool());
    // Its tabs came along
    EXPECT_EQ(main["tabs"].toArray()[0].toString(), "there");
    EXPECT_EQ(windows[1].toObject()["x"].toInt(), 100);
    EXPECT_EQ(windows[1].toObject()["tabs"].toArray()[0].toString(),
              "popup there");
    // No popup here to take the place of: it keeps its own
    EXPECT_EQ(windows[2].toObject()["x"].toInt(), -2000);

    const auto settings = readJson(staged.path() + "/Settings/settings.json");
    const auto alerts = settings["moderation"].toObject()["alerts"].toObject();
    EXPECT_EQ(alerts["x"].toInt(), 40);
    EXPECT_EQ(alerts["y"].toInt(), 50);
    EXPECT_FALSE(alerts.contains("width"));
    // Everything else is the other computer's
    EXPECT_TRUE(alerts["sound"].toBool());
    EXPECT_EQ(settings["appearance"].toObject()["uiStyle"].toString(),
              "modern");
    // Only the chats here, everything there
    EXPECT_TRUE(settings["appearance"].toObject()["focusMode"].toBool());
}

TEST(FlexiiSync, SilencedAlertsStayOnTheComputerTheyWereSilencedOn)
{
    QTemporaryDir local;
    QTemporaryDir staged;
    ASSERT_TRUE(local.isValid() && staged.isValid());

    // Silenced here, not there
    writeJson(local.path() + "/Settings/settings.json",
              {{"moderation",
                QJsonObject{
                    {"alert", QJsonObject{{"mutedChannels", "trymacs"}}}}}});
    writeJson(staged.path() + "/Settings/settings.json",
              {{"moderation",
                QJsonObject{{"alert", QJsonObject{{"mutedChannels", ""},
                                                  {"colorWord", "#112233"}}}}}});

    profilesync::keepWindowPlaces(local.path(), staged.path());

    const auto settings = readJson(staged.path() + "/Settings/settings.json");
    const auto alert = settings["moderation"].toObject()["alert"].toObject();
    // Taken from here, so a bell pressed on one computer does not silence
    // the other
    EXPECT_EQ(alert["mutedChannels"].toString(), "trymacs");
    // What belongs to the alert itself still comes from there
    EXPECT_EQ(alert["colorWord"].toString(), "#112233");
}

namespace {

ChatRole roleWith(std::initializer_list<const char *> badges)
{
    Message message;
    for (const auto *badge : badges)
    {
        message.twitchBadges.emplace_back(badge, "1");
    }
    return roleOf(message);
}

}  // namespace

TEST(FlexiiLook, TheStripeShowsTheHighestRole)
{
    EXPECT_EQ(roleWith({}), ChatRole::None);
    EXPECT_EQ(roleWith({"premium"}), ChatRole::None);
    EXPECT_EQ(roleWith({"founder"}), ChatRole::Subscriber);
    EXPECT_EQ(roleWith({"subscriber", "vip"}), ChatRole::Vip);
    EXPECT_EQ(roleWith({"subscriber", "moderator"}), ChatRole::Moderator);
    EXPECT_EQ(roleWith({"lead_moderator"}), ChatRole::Moderator);
    EXPECT_EQ(roleWith({"broadcaster", "subscriber"}), ChatRole::Broadcaster);
}

namespace {

HighlightBadge badgeHighlight(const QString &badge, const QString &color)
{
    return {badge, badge, false, false, false, "", QColor(color)};
}

QColor stripeFor(std::initializer_list<const char *> badges,
                 const std::vector<HighlightBadge> &highlights)
{
    Message message;
    for (const auto *badge : badges)
    {
        message.twitchBadges.emplace_back(badge, "1");
    }
    RoleColors colors;
    colors.moderator = QColor("#00ad03");
    colors.vip = QColor("#e005b9");
    return stripeColor(message, highlights, colors);
}

}  // namespace

TEST(FlexiiLook, StripesTakeTheBadgeHighlightsColourMadeSolid)
{
    const std::vector<HighlightBadge> highlights{
        badgeHighlight("lead_moderator", "#6a32e01b"),
        badgeHighlight("moderator", "#6a1060ff"),
    };
    EXPECT_EQ(stripeFor({"lead_moderator"}, highlights), QColor("#32e01b"));
    EXPECT_EQ(stripeFor({"moderator", "subscriber"}, highlights),
              QColor("#1060ff"));

    // A lead moderator without a highlight of their own takes the
    // moderators' one
    EXPECT_EQ(stripeFor({"lead_moderator"},
                        {badgeHighlight("moderator", "#6a1060ff")}),
              QColor("#1060ff"));
}

TEST(FlexiiLook, WithoutABadgeHighlightTheRolesOwnColour)
{
    EXPECT_EQ(stripeFor({"vip"}, {}), QColor("#e005b9"));
    EXPECT_EQ(stripeFor({"moderator"}, {badgeHighlight("vip", "#ff0000")}),
              QColor("#00ad03"));
    // Subscribers have no colour to begin with: no stripe
    EXPECT_FALSE(stripeFor({"subscriber"}, {}).isValid());
    EXPECT_FALSE(stripeFor({"premium"}, {}).isValid());
}

TEST(FlexiiLook, TheSettingsPageKnowsWhichRolesAHighlightColours)
{
    const std::vector<HighlightBadge> highlights{
        badgeHighlight("moderator", "#6a1060ff"),
        badgeHighlight("broadcaster", "#93ffa600"),
    };
    EXPECT_EQ(badgeStripeColor(ChatRole::Moderator, highlights),
              QColor("#1060ff"));
    EXPECT_EQ(badgeStripeColor(ChatRole::Broadcaster, highlights),
              QColor("#ffa600"));
    EXPECT_FALSE(badgeStripeColor(ChatRole::Vip, highlights).has_value());
}

namespace {

ChatEvent eventFor(MessageFlag flag, const QString &text)
{
    Message message;
    message.flags.set(flag);
    message.messageText = text;
    return eventOf(message);
}

}  // namespace

TEST(FlexiiLook, EventsAreToldApart)
{
    EXPECT_EQ(eventFor(MessageFlag::Timeout, "x has been timed out for 10m."),
              ChatEvent::Timeout);
    EXPECT_EQ(eventFor(MessageFlag::Timeout, "x has been permanently banned."),
              ChatEvent::Ban);
    EXPECT_EQ(eventFor(MessageFlag::Subscription,
                       "15 raiders from zarbex have joined!"),
              ChatEvent::Raid);
    EXPECT_EQ(eventFor(MessageFlag::Subscription, "Announcement"),
              ChatEvent::Announcement);
    EXPECT_EQ(eventFor(MessageFlag::Subscription,
                       "anna gifted a Tier 1 sub to ben!"),
              ChatEvent::Gift);
    EXPECT_EQ(eventFor(MessageFlag::Subscription,
                       "anna subscribed at Tier 1. They've subscribed for 3 "
                       "months!"),
              ChatEvent::Sub);
    EXPECT_EQ(eventFor(MessageFlag::CheerMessage, "cheer100 gg"),
              ChatEvent::Bits);
    EXPECT_EQ(eventFor(MessageFlag::RedeemedChannelPointReward, "Hydrate"),
              ChatEvent::Redeem);
    EXPECT_EQ(eventFor(MessageFlag::WatchStreak, "watched 5 streams"),
              ChatEvent::Streak);
    EXPECT_EQ(eventFor(MessageFlag::Untimeout, "x has been untimedout."),
              ChatEvent::None);
    EXPECT_EQ(eventOf(Message{}), ChatEvent::None);
}

TEST(FlexiiLook, EveryEventHasASymbolAndAColour)
{
    for (auto event :
         {ChatEvent::Sub, ChatEvent::Gift, ChatEvent::Raid,
          ChatEvent::Announcement, ChatEvent::Timeout, ChatEvent::Ban,
          ChatEvent::Bits, ChatEvent::Redeem, ChatEvent::Streak})
    {
        EXPECT_FALSE(eventSymbol(event).isEmpty());
        EXPECT_TRUE(eventColor(event).isValid());
    }
    EXPECT_TRUE(eventSymbol(ChatEvent::None).isEmpty());
}

TEST(FlexiiViews, SavedRenamedAndRemoved)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeJson(dir.path() + "/settings.json",
              {{"appearance", QJsonObject{{"uiStyle", "modern"}}}});

    QString error;
    ASSERT_TRUE(snapshots::save(dir.path(), "Moderieren", error));
    ASSERT_TRUE(snapshots::save(dir.path(), "Entspannt", error));
    // Saving under a name that is there replaces it
    ASSERT_TRUE(snapshots::save(dir.path(), "Moderieren", error));

    auto views = snapshots::list(dir.path());
    ASSERT_EQ(views.size(), 2);
    EXPECT_EQ(views[0].name, "Entspannt");
    EXPECT_EQ(views[1].name, "Moderieren");

    ASSERT_TRUE(snapshots::rename(dir.path(), "Entspannt", "Chill", error));
    ASSERT_TRUE(snapshots::remove(dir.path(), "Moderieren"));
    views = snapshots::list(dir.path());
    ASSERT_EQ(views.size(), 1);
    EXPECT_EQ(views[0].name, "Chill");
    EXPECT_FALSE(snapshots::save(dir.path(), "   ", error));
}

TEST(FlexiiViews, SwitchingKeepsLoginSyncAndAWayBack)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const auto settingsFile = dir.path() + "/settings.json";

    // A view saved on another day, with another login in it
    writeJson(settingsFile,
              {{"appearance", QJsonObject{{"uiStyle", "classic"}}},
               {"accounts", QJsonObject{{"current", "someone"}}}});
    QString error;
    ASSERT_TRUE(snapshots::save(dir.path(), "Schlicht", error));

    // What is set up now
    writeJson(settingsFile,
              {{"appearance", QJsonObject{{"uiStyle", "modern"}}},
               {"accounts", QJsonObject{{"current", "fx_flexii"}}},
               {"sync", QJsonObject{{"base", "t1"}}}});
    ASSERT_TRUE(snapshots::stage(dir.path(), "Schlicht", error))
        << error.toStdString();
    snapshots::applyPending(dir.path());

    const auto settings = readJson(settingsFile);
    EXPECT_EQ(settings["appearance"].toObject()["uiStyle"].toString(),
              "classic");
    EXPECT_EQ(settings["accounts"].toObject()["current"].toString(),
              "fx_flexii");
    EXPECT_EQ(settings["sync"].toObject()["base"].toString(), "t1");
    EXPECT_EQ(settings["snapshots"].toObject()["current"].toString(),
              "Schlicht");

    // What was set up before is kept to go back to
    bool kept = false;
    for (const auto &view : snapshots::list(dir.path()))
    {
        kept = kept || view.name == snapshots::BEFORE_SWITCH;
    }
    EXPECT_TRUE(kept);

    // Applied once only
    writeJson(settingsFile, {{"appearance", QJsonObject{{"uiStyle", "x"}}}});
    snapshots::applyPending(dir.path());
    EXPECT_EQ(readJson(settingsFile)["appearance"]
                  .toObject()["uiStyle"]
                  .toString(),
              "x");
}

#ifdef Q_OS_MACOS

namespace {

bool runQuietly(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    return process.waitForFinished(60000) && process.exitCode() == 0;
}

void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(content);
}

}  // namespace

TEST(FlexiiSelfUpdate, TheAppIsCopiedOutOfTheDiskImage)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeFile(dir.path() + "/image/ChattiFlexii.app/Contents/Info.plist",
              "new version");
    ASSERT_TRUE(runQuietly("/usr/bin/hdiutil",
                           {"create", "-quiet", "-srcfolder",
                            dir.path() + "/image", "-format", "UDZO",
                            dir.path() + "/update.dmg"}));

    const auto staging = dir.path() + "/.ChattiFlexii-update.app";
    QString error;
    ASSERT_TRUE(selfupdate::stageFromDiskImage(dir.path() + "/update.dmg",
                                               staging, error))
        << error.toStdString();
    QFile info(staging + "/Contents/Info.plist");
    ASSERT_TRUE(info.open(QIODevice::ReadOnly));
    EXPECT_EQ(info.readAll(), "new version");
}

TEST(FlexiiSelfUpdate, AnImageWithoutTheAppIsTurnedDown)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeFile(dir.path() + "/image/Something.txt", "not an app");
    ASSERT_TRUE(runQuietly("/usr/bin/hdiutil",
                           {"create", "-quiet", "-srcfolder",
                            dir.path() + "/image", "-format", "UDZO",
                            dir.path() + "/other.dmg"}));

    QString error;
    EXPECT_FALSE(selfupdate::stageFromDiskImage(
        dir.path() + "/other.dmg", dir.path() + "/staged.app", error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(QFileInfo::exists(dir.path() + "/staged.app"));
}

TEST(FlexiiSelfUpdate, TheSwapPutsTheNewAppInPlaceOfTheOld)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const auto app = dir.path() + "/ChattiFlexii.app";
    const auto staging = dir.path() + "/.ChattiFlexii-update.app";
    writeFile(app + "/Contents/old.txt", "old");
    writeFile(staging + "/Contents/new.txt", "new");

    // A process id that is long gone, so it does not wait
    ASSERT_TRUE(runQuietly("/bin/sh", {"-c", selfupdate::swapScript(), "swap",
                                       "999999", staging, app, "noopen"}));

    EXPECT_TRUE(QFileInfo::exists(app + "/Contents/new.txt"));
    EXPECT_FALSE(QFileInfo::exists(app + "/Contents/old.txt"));
    EXPECT_FALSE(QFileInfo::exists(staging));
    EXPECT_FALSE(QFileInfo::exists(dir.path() + "/.ChattiFlexii-old.app"));
}

#endif

namespace {

using Reason = EmoteSpamDetector::Reason;
using Thresholds = EmoteSpamDetector::Thresholds;

/// The numbers as they come out of the box: only the window rule
constexpr Thresholds ONLY_WINDOW{.window = 8, .single = 0, .streak = 0};

}  // namespace

TEST(FlexiiEmoteSpam, WithoutEnoughEmotesNothingHappens)
{
    EXPECT_EQ(EmoteSpamDetector::reasonFor(ONLY_WINDOW, 7, 7, 3), Reason::None);
    EXPECT_EQ(EmoteSpamDetector::reasonFor(ONLY_WINDOW, 8, 3, 0),
              Reason::Window);
}

TEST(FlexiiEmoteSpam, OneMessageCanBeEnoughOnItsOwn)
{
    constexpr Thresholds withSingle{.window = 20, .single = 6, .streak = 0};

    // Six in one message, far short of the twenty over the window
    EXPECT_EQ(EmoteSpamDetector::reasonFor(withSingle, 6, 6, 0),
              Reason::SingleMessage);
    EXPECT_EQ(EmoteSpamDetector::reasonFor(withSingle, 5, 5, 0), Reason::None);

    // Switched off, only the window counts
    constexpr Thresholds off{.window = 20, .single = 0, .streak = 0};
    EXPECT_EQ(EmoteSpamDetector::reasonFor(off, 6, 6, 0), Reason::None);
}

TEST(FlexiiEmoteSpam, MessagesInARowOfNothingButEmotesAreEnough)
{
    constexpr Thresholds withStreak{.window = 20, .single = 0, .streak = 5};

    // Five in a row, though they hold two emotes each
    EXPECT_EQ(EmoteSpamDetector::reasonFor(withStreak, 10, 2, 5),
              Reason::Streak);
    EXPECT_EQ(EmoteSpamDetector::reasonFor(withStreak, 8, 2, 4), Reason::None);

    constexpr Thresholds off{.window = 20, .single = 0, .streak = 0};
    EXPECT_EQ(EmoteSpamDetector::reasonFor(off, 8, 2, 9), Reason::None);
}

TEST(FlexiiEmoteSpam, TheOneMessageRuleIsNamedFirst)
{
    // All three would do; the alert says what is easiest to see
    constexpr Thresholds all{.window = 8, .single = 6, .streak = 3};
    EXPECT_EQ(EmoteSpamDetector::reasonFor(all, 12, 6, 4),
              Reason::SingleMessage);
    EXPECT_EQ(EmoteSpamDetector::reasonFor(all, 12, 2, 4), Reason::Streak);
    EXPECT_EQ(EmoteSpamDetector::reasonFor(all, 12, 2, 1), Reason::Window);
}

TEST(FlexiiEmoteSpam, WithBothNewRulesOffNothingChanges)
{
    // Which is how they start out - only the window rule then decides
    constexpr Thresholds asBefore{.window = 8, .single = 0, .streak = 0};
    EXPECT_EQ(EmoteSpamDetector::reasonFor(asBefore, 20, 20, 12),
              Reason::Window);
    EXPECT_EQ(EmoteSpamDetector::reasonFor(asBefore, 7, 7, 12), Reason::None);
}

namespace {

/// The word of @a list that @a text holds, empty when none does
QString wordIn(const QString &text,
               const std::vector<WordAlertDetector::Watched> &list)
{
    const auto match = WordAlertDetector::find(text, list);
    return match ? match->word : QString{};
}

}  // namespace

TEST(FlexiiWordAlert, AWordIsFoundHoweverItIsWritten)
{
    const auto list = WordAlertDetector::parseWords("sybau");
    ASSERT_EQ(list.size(), 1);
    EXPECT_EQ(list.front().word, "sybau");

    // Plainly, dressed up, and in another case
    EXPECT_EQ(wordIn("sybau", list), "sybau");
    EXPECT_EQ(wordIn("SYBAU lol", list), "sybau");
    EXPECT_EQ(wordIn("syb4u", list), "sybau");
    EXPECT_EQ(wordIn("s.y.b.a.u", list), "sybau");

    // And not where it is not
    EXPECT_TRUE(wordIn("hallo zusammen", list).isEmpty());
}

TEST(FlexiiWordAlert, TheAlertLearnsHowItWasWritten)
{
    const auto list = WordAlertDetector::parseWords("felix");

    // So the window can say which word was meant, next to what stood there
    const auto plain = WordAlertDetector::find("hallo felix", list);
    ASSERT_TRUE(plain.has_value());
    EXPECT_EQ(plain->word, "felix");
    EXPECT_EQ(plain->asWritten, "felix");

    const auto dressed = WordAlertDetector::find("hallo f3l1x", list);
    ASSERT_TRUE(dressed.has_value());
    EXPECT_EQ(dressed->word, "felix");
    EXPECT_EQ(dressed->asWritten, "f3l1x");
}

TEST(FlexiiWordAlert, OnlyAsAWholeWordUnlessSaidOtherwise)
{
    const auto whole = WordAlertDetector::parseWords("ass");
    EXPECT_TRUE(wordIn("eine Klasse für sich", whole).isEmpty());
    EXPECT_EQ(wordIn("ass", whole), "ass");

    const auto anywhere = WordAlertDetector::parseWords("ass", true, false);
    EXPECT_EQ(wordIn("eine Klasse für sich", anywhere), "ass");
}

TEST(FlexiiWordAlert, WithoutVariantsOnlyTheWordItself)
{
    const auto exact = WordAlertDetector::parseWords("sybau", false);
    EXPECT_EQ(wordIn("SYBAU", exact), "sybau");
    EXPECT_TRUE(wordIn("syb4u", exact).isEmpty());
}

TEST(FlexiiWordAlert, TheListTakesOneWordPerLineAndNotes)
{
    const auto list = WordAlertDetector::parseWords(
        "sybau\n# das hier ist nur eine Notiz\n\n  kys  ");
    ASSERT_EQ(list.size(), 2);
    EXPECT_EQ(list.front().word, "sybau");
    EXPECT_EQ(list.back().word, "kys");

    EXPECT_TRUE(WordAlertDetector::parseWords("").empty());
    EXPECT_TRUE(WordAlertDetector::parseWords("# nur Notizen").empty());
}

TEST(FlexiiWordAlert, AWordCanBringItsOwnSteps)
{
    const auto list = WordAlertDetector::parseWords(
        "sybau\nkys = 1d, bann\nspam = löschen, 5m");
    ASSERT_EQ(list.size(), 3);

    // Nothing of its own: it follows the steps set for all of them
    EXPECT_EQ(list[0].word, "sybau");
    EXPECT_TRUE(list[0].steps.empty());

    EXPECT_EQ(list[1].word, "kys");
    EXPECT_EQ(list[1].steps, (std::vector<int>{86400, 0}));

    EXPECT_EQ(list[2].word, "spam");
    EXPECT_EQ(list[2].steps,
              (std::vector<int>{WordAlertDetector::DELETE, 300}));

    // The word is still found, the steps are not part of it
    EXPECT_EQ(wordIn("kys", list), "kys");
}

TEST(FlexiiWordAlert, TheListIsWrittenBackTheWayItIsRead)
{
    const auto text = QStringLiteral("sybau\nkys = 1d, bann");
    const auto written =
        WordAlertDetector::writeWords(WordAlertDetector::parseWords(text));
    EXPECT_EQ(written, text);

    // And reading that again gives the same list
    const auto again = WordAlertDetector::parseWords(written);
    ASSERT_EQ(again.size(), 2);
    EXPECT_EQ(again[1].steps, (std::vector<int>{86400, 0}));
}

TEST(FlexiiWordAlert, TheDeleteButtonTakesAsManyAsAsked)
{
    EXPECT_EQ(WordAlertDetector::deleteLimit(0),
              WordAlertDetector::MOST_DELETED);
    EXPECT_EQ(WordAlertDetector::deleteLimit(1), 1);
    EXPECT_EQ(WordAlertDetector::deleteLimit(99),
              WordAlertDetector::MOST_DELETED);
}

TEST(FlexiiWordAlert, TheStepsReadLikeTheOtherAlerts)
{
    const auto del = WordAlertDetector::DELETE;
    EXPECT_EQ(WordAlertDetector::parseSteps("löschen, 5m, 1d"),
              (std::vector<int>{del, 300, 86400}));
    EXPECT_EQ(WordAlertDetector::parseSteps("5m, 10m, 30m, 1h, 1d"),
              (std::vector<int>{300, 600, 1800, 3600, 86400}));
    EXPECT_TRUE(WordAlertDetector::parseSteps("5m, bald").empty());
}

TEST(FlexiiSteps, ABanIsWrittenOutRatherThanTimed)
{
    // Twitch times nobody out for longer than two weeks, so a permanent one
    // has no length - it reads as zero seconds
    EXPECT_EQ(RepeatSpamDetector::parseSteps("30s, 1h, bann"),
              (std::vector<int>{30, 3600, 0}));
    EXPECT_EQ(RepeatSpamDetector::parseSteps("ban"), (std::vector<int>{0}));
    EXPECT_EQ(RepeatSpamDetector::parseSteps("Perma"), (std::vector<int>{0}));
    EXPECT_EQ(RepeatSpamDetector::parseSteps("dauerhaft"),
              (std::vector<int>{0}));

    // The other alerts read their steps through the same parser
    EXPECT_EQ(EmoteSpamDetector::parseSteps("löschen, 30s, bann"),
              (std::vector<int>{EmoteSpamDetector::DELETE, 30, 0}));
    EXPECT_EQ(WordAlertDetector::parseSteps("5m, 1d, bann"),
              (std::vector<int>{300, 86400, 0}));

    // A duration of nothing is still a typo, not a ban
    EXPECT_TRUE(RepeatSpamDetector::parseSteps("0s").empty());
    EXPECT_TRUE(RepeatSpamDetector::parseSteps("30s, bannen").empty());
}

TEST(FlexiiEmoteSpam, TheDeleteButtonTakesAsManyAsAsked)
{
    // Nothing set: all of them, up to what one alert ever deletes
    EXPECT_EQ(EmoteSpamDetector::deleteLimit(0),
              EmoteSpamDetector::MOST_DELETED);
    EXPECT_EQ(EmoteSpamDetector::deleteLimit(-3),
              EmoteSpamDetector::MOST_DELETED);

    // Often one is all it takes
    EXPECT_EQ(EmoteSpamDetector::deleteLimit(1), 1);
    EXPECT_EQ(EmoteSpamDetector::deleteLimit(5), 5);

    // And never more than that, whatever is asked for
    EXPECT_EQ(EmoteSpamDetector::deleteLimit(500),
              EmoteSpamDetector::MOST_DELETED);
}
