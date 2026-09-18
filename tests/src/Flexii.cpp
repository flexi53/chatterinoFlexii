// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for what ChattiFlexii adds on top of Chatterino. They run on every
// push before a download is published.

#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/ModHighlights.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "Test.hpp"
#include "util/ProfileSetup.hpp"
#include "util/Twitch.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUrl>

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
