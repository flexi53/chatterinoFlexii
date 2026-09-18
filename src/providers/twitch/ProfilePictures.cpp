// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/ProfilePictures.hpp"

#include "Application.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "messages/Image.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "singletons/WindowManager.hpp"
#include "util/PostToThread.hpp"
#include "util/Twitch.hpp"

#include <QHash>
#include <QPointer>

#include <chrono>
#include <mutex>
#include <vector>

namespace {

using namespace chatterino;

/// Twitch answers for up to this many names at once
constexpr qsizetype LOOKUP_BATCH = 100;
/// How long a name that got no answer - no connection, not logged in yet -
/// waits before it is asked about again
constexpr auto RETRY_AFTER = std::chrono::minutes(1);
constexpr int PICTURE_TIMEOUT_MS = 20000;

struct Known {
    enum class State {
        Asked,
        Found,
        NoSuchUser,
        NoAnswer,
    };

    State state = State::Asked;
    TwitchProfile profile;
    /// The picture for chat, made the first time it is wanted
    ImagePtr image;
    std::chrono::steady_clock::time_point askedAt =
        std::chrono::steady_clock::now();
};

struct Waiter {
    QPointer<QObject> context;
    std::function<void(const TwitchProfile &)> done;
};

std::mutex &knownMutex()
{
    static std::mutex mutex;
    return mutex;
}

QHash<QString, Known> &known()
{
    static QHash<QString, Known> names;
    return names;
}

/// Callbacks waiting for a profile still being looked up. GUI thread only.
QHash<QString, std::vector<Waiter>> &waiters()
{
    static QHash<QString, std::vector<Waiter>> waiting;
    return waiting;
}

/// Pictures already loaded, by address. GUI thread only.
QHash<QString, QPixmap> &loadedPictures()
{
    static QHash<QString, QPixmap> pictures;
    return pictures;
}

/// Twitch links profile pictures at 300x300 and serves them at 70x70 as
/// well - plenty for anything shown smaller
QString sizedPicture(QString url, int side)
{
    if (side <= 70)
    {
        url.replace(QStringLiteral("300x300"), QStringLiteral("70x70"));
    }
    return url;
}

/// The names in @a logins that should be asked about now - never asked, or
/// asked a while ago without an answer - marked as asked. The caller holds
/// the lock.
QStringList claimLocked(const QStringList &logins)
{
    QStringList claimed;
    const auto now = std::chrono::steady_clock::now();
    for (const auto &login : logins)
    {
        auto it = known().find(login);
        if (it == known().end())
        {
            known().insert(login, Known{});
            claimed.append(login);
        }
        else if (it->state == Known::State::NoAnswer &&
                 now - it->askedAt >= RETRY_AFTER)
        {
            it->state = Known::State::Asked;
            it->askedAt = now;
            claimed.append(login);
        }
    }
    return claimed;
}

/// Hands the profiles of @a logins to whatever was waiting for them. Those
/// waiting for a name that got no profile are let go.
void settleWaiters(const QStringList &logins)
{
    runInGuiThread([logins] {
        for (const auto &login : logins)
        {
            auto waiting = waiters().take(login);
            if (waiting.empty())
            {
                continue;
            }

            std::optional<TwitchProfile> found;
            {
                std::lock_guard lock(knownMutex());
                const auto it = known().constFind(login);
                if (it != known().constEnd() &&
                    it->state == Known::State::Found)
                {
                    found = it->profile;
                }
            }
            if (!found)
            {
                continue;
            }
            for (const auto &waiter : waiting)
            {
                if (waiter.context)
                {
                    waiter.done(*found);
                }
            }
        }
    });
}

void lookUp(const QStringList &logins)
{
    // Queued, so names asked about while the app is still starting go out
    // once it is up and logged in
    postToThread([logins] {
        for (qsizetype start = 0; start < logins.size(); start += LOOKUP_BATCH)
        {
            const auto batch = logins.mid(start, LOOKUP_BATCH);
            getHelix()->fetchUsers(
                {}, batch,
                [batch](const std::vector<HelixUser> &users) {
                    {
                        std::lock_guard lock(knownMutex());
                        for (const auto &user : users)
                        {
                            auto &entry = known()[user.login.toLower()];
                            entry.state = Known::State::Found;
                            entry.profile = {
                                .login = user.login.toLower(),
                                .displayName = user.displayName,
                                .pictureUrl = user.profileImageUrl,
                                .createdAt = QDateTime::fromString(
                                    user.createdAt, Qt::ISODate),
                            };
                        }
                        for (const auto &login : batch)
                        {
                            auto &entry = known()[login];
                            if (entry.state == Known::State::Asked)
                            {
                                entry.state = Known::State::NoSuchUser;
                            }
                        }
                    }

                    settleWaiters(batch);
                    // Captions already in chat show the names as text - laid
                    // out again, they show the pictures
                    runInGuiThread([] {
                        getApp()->getWindows()->forceLayoutChannelViews();
                    });
                },
                [batch] {
                    {
                        std::lock_guard lock(knownMutex());
                        const auto now = std::chrono::steady_clock::now();
                        for (const auto &login : batch)
                        {
                            auto &entry = known()[login];
                            if (entry.state == Known::State::Asked)
                            {
                                entry.state = Known::State::NoAnswer;
                                entry.askedAt = now;
                            }
                        }
                    }
                    settleWaiters(batch);
                });
        }
    });
}

}  // namespace

namespace chatterino::profilepictures {

QString loginOf(const QString &word)
{
    if (word.size() < 2 || !word.startsWith(u'@'))
    {
        return {};
    }
    const auto login = word.mid(1).toLower();
    return isValidTwitchLogin(login) ? login : QString();
}

std::optional<TwitchProfile> profile(const QString &login)
{
    const auto key = login.toLower();
    QStringList toAsk;
    std::optional<TwitchProfile> result;
    {
        std::lock_guard lock(knownMutex());
        toAsk = claimLocked({key});
        const auto it = known().constFind(key);
        if (it != known().constEnd() && it->state == Known::State::Found)
        {
            result = it->profile;
        }
    }
    if (!toAsk.isEmpty())
    {
        lookUp(toAsk);
    }
    return result;
}

void whenKnown(const QString &login, QObject *context,
               std::function<void(const TwitchProfile &)> done)
{
    const auto key = login.toLower();
    if (auto found = profile(key))
    {
        done(*found);
        return;
    }

    {
        // No point waiting for a name that is known to be no Twitch user
        std::lock_guard lock(knownMutex());
        const auto it = known().constFind(key);
        if (it != known().constEnd() &&
            it->state == Known::State::NoSuchUser)
        {
            return;
        }
    }
    waiters()[key].push_back({context, std::move(done)});
}

std::shared_ptr<Image> image(const QString &login)
{
    const auto key = login.toLower();
    QStringList toAsk;
    ImagePtr result;
    {
        std::lock_guard lock(knownMutex());
        toAsk = claimLocked({key});

        auto it = known().find(key);
        if (it != known().end() && it->state == Known::State::Found)
        {
            if (!it->image && !it->profile.pictureUrl.isEmpty())
            {
                it->image =
                    Image::fromUrl(Url{sizedPicture(it->profile.pictureUrl, 70)});
            }
            result = it->image;
        }
    }

    if (!toAsk.isEmpty())
    {
        lookUp(toAsk);
    }
    return result;
}

void pixmap(const QString &login, int side, QObject *context,
            std::function<void(const QPixmap &)> done)
{
    QPointer<QObject> guard(context);
    whenKnown(
        login, context,
        [side, guard, done = std::move(done)](const TwitchProfile &profile) {
            if (profile.pictureUrl.isEmpty() || !guard)
            {
                return;
            }

            const auto url = sizedPicture(profile.pictureUrl, side);
            const auto loaded = loadedPictures().constFind(url);
            if (loaded != loadedPictures().constEnd())
            {
                done(*loaded);
                return;
            }

            NetworkRequest(url, NetworkRequestType::Get)
                .timeout(PICTURE_TIMEOUT_MS)
                .cache()
                .caller(guard.data())
                .onSuccess([url, guard, done](const NetworkResult &result) {
                    const auto data = result.getData();
                    runInGuiThread([url, data, guard, done] {
                        QPixmap picture;
                        if (!picture.loadFromData(data))
                        {
                            return;
                        }
                        loadedPictures().insert(url, picture);
                        if (guard)
                        {
                            done(picture);
                        }
                    });
                })
                .execute();
        });
}

QString displayName(const QString &login)
{
    std::lock_guard lock(knownMutex());
    const auto it = known().constFind(login.toLower());
    if (it != known().constEnd() && !it->profile.displayName.isEmpty())
    {
        return it->profile.displayName;
    }
    return login;
}

void prefetch(const QStringList &logins)
{
    QStringList keys;
    for (const auto &login : logins)
    {
        const auto key = login.toLower();
        if (!key.isEmpty() && !keys.contains(key))
        {
            keys.append(key);
        }
    }

    QStringList toAsk;
    {
        std::lock_guard lock(knownMutex());
        toAsk = claimLocked(keys);
    }
    if (!toAsk.isEmpty())
    {
        lookUp(toAsk);
    }
}

void prefetchCaptions(const QStringList &captions)
{
    QStringList logins;
    for (const auto &caption : captions)
    {
        for (const auto &word : caption.split(u' ', Qt::SkipEmptyParts))
        {
            const auto login = loginOf(word);
            if (!login.isEmpty())
            {
                logins.append(login);
            }
        }
    }
    prefetch(logins);
}

void remember(const TwitchProfile &profile, std::shared_ptr<Image> image)
{
    std::lock_guard lock(knownMutex());
    auto &entry = known()[profile.login.toLower()];
    entry.state = Known::State::Found;
    entry.profile = profile;
    entry.image = std::move(image);
}

}  // namespace chatterino::profilepictures
