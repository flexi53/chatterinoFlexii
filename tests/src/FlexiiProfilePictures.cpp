// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Tests for how ChattiFlexii looks up Twitch profiles for pictures - in
// tabs, captions and alert windows. Kept apart from Flexii.cpp as they need
// an application with accounts, a window manager and a stand-in for Twitch.

#include "controllers/accounts/AccountController.hpp"
#include "mocks/BaseApplication.hpp"
#include "mocks/Helix.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "messages/Image.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>

#include <chrono>
#include <functional>
#include <optional>

using namespace chatterino;
using namespace std::chrono_literals;

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

    AccountController *getAccounts() override
    {
        return &this->accounts;
    }

    WindowManager windowManager;
    AccountController accounts;
};

/// Runs the event loop until @a done, or @a limit has passed
bool waitFor(const std::function<bool()> &done, std::chrono::milliseconds limit)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < limit.count())
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    return done();
}

/// A name no earlier test or run has asked about - profiles are kept for
/// the whole run
QString freshLogin(const QString &prefix)
{
    static int count = 0;
    return QStringLiteral("%1%2x%3")
        .arg(prefix)
        .arg(QDateTime::currentMSecsSinceEpoch() % 100000000)
        .arg(++count);
}

class FlexiiProfilePictures : public ::testing::Test
{
protected:
    FlexiiProfilePictures()
    {
        // Nothing kept on disk from, or for, anything else
        QStringSetting("/cache/path").setValue(this->cache.path());
        this->helix = new mock::Helix;
        initializeHelix(this->helix);
    }

    ~FlexiiProfilePictures() override
    {
        // Checks what it was expected to be asked
        delete this->helix;
    }

    QTemporaryDir cache;
    MockApplication app;
    mock::Helix *helix{};
};

}  // namespace

TEST_F(FlexiiProfilePictures, NamesAskedTogetherGoOutInOneRequest)
{
    const auto first = freshLogin("a");
    const auto second = freshLogin("b");

    QStringList asked;
    EXPECT_CALL(*this->helix, fetchUsers)
        .WillOnce([&](auto, QStringList logins, auto ok, auto) {
            asked = logins;
            ok({});
        });

    profilepictures::prefetch({first});
    profilepictures::prefetch({second});

    ASSERT_TRUE(waitFor(
        [&] {
            return !asked.isEmpty();
        },
        2s));
    EXPECT_EQ(asked, (QStringList{first, second}));
    // Let what the answer set off finish while this app is still there
    waitFor(
        [] {
            return false;
        },
        300ms);
}

TEST_F(FlexiiProfilePictures, NoAnswerAtFirstIsAskedAgainAndTheWaitingGetIt)
{
    const auto login = freshLogin("c");
    const HelixUser user(QJsonObject{
        {"login", login},
        {"display_name", "Tester"},
        {"profile_image_url", "https://example.com/300x300.png"},
    });

    // Not logged in yet the first time, the second time it answers
    EXPECT_CALL(*this->helix, fetchUsers)
        .WillOnce([](auto, auto, auto, auto fail) {
            fail();
        })
        .WillOnce([user](auto, auto, auto ok, auto) {
            ok({user});
        });

    QObject context;
    std::optional<TwitchProfile> got;
    profilepictures::whenKnown(login, &context,
                               [&](const TwitchProfile &profile) {
                                   got = profile;
                               });

    ASSERT_TRUE(waitFor(
        [&] {
            return got.has_value();
        },
        8s));
    EXPECT_EQ(got->displayName, "Tester");
    EXPECT_EQ(profilepictures::displayName(login), "Tester");
    waitFor(
        [] {
            return false;
        },
        300ms);
}

TEST_F(FlexiiProfilePictures, ASharedMessageBadgeStaysSmall)
{
    // Chatterino keeps one picture per address: the badge of a shared
    // message is built from the same 70x70 address our avatars use, and
    // whoever asks first says how big it is drawn. Ours therefore asks for
    // it the way the badge needs it - 18 px - or the badge came out as a
    // 70 px block in the middle of the chat.
    const auto login = freshLogin("d");
    const HelixUser user(QJsonObject{
        {"login", login},
        {"display_name", "Tester"},
        {"profile_image_url",
         "https://static-cdn.jtvnw.net/jtv_user_pictures/" + login +
             "-profile_image-300x300.png"},
    });

    EXPECT_CALL(*this->helix, fetchUsers)
        .WillOnce([user](auto, auto, auto ok, auto) {
            ok({user});
        });

    QObject context;
    bool known = false;
    profilepictures::whenKnown(login, &context, [&](const auto &) {
        known = true;
    });
    ASSERT_TRUE(waitFor(
        [&] {
            return known;
        },
        8s));

    auto ours = profilepictures::image(login);
    ASSERT_TRUE(ours);
    EXPECT_EQ(ours->size(), QSizeF(18, 18));

    // The address the badge would take - the very same picture
    const auto badgeUrl =
        Url{"https://static-cdn.jtvnw.net/jtv_user_pictures/" + login +
            "-profile_image-70x70.png"};
    EXPECT_EQ(Image::fromUrl(badgeUrl).get(), ours.get());
    EXPECT_EQ(Image::fromUrl(badgeUrl)->size(), QSizeF(18, 18));

    waitFor(
        [] {
            return false;
        },
        300ms);
}
