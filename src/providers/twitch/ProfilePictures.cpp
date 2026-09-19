// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/ProfilePictures.hpp"

#include "Application.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "messages/Image.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccountManager.hpp"
#include "singletons/Paths.hpp"
#include "singletons/WindowManager.hpp"
#include "util/PostToThread.hpp"
#include "util/Twitch.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <tuple>
#include <vector>

namespace {

using namespace chatterino;
using namespace std::chrono_literals;

/// Twitch answers for up to this many names at once
constexpr qsizetype LOOKUP_BATCH = 100;
/// How long names are gathered before they are asked about together - tabs,
/// captions and a chat's worth of mods come in a burst
constexpr auto GATHER_FOR = 60ms;
/// When a name that got no answer is asked again, by how often it went
/// unanswered - the last one over and over
constexpr std::array<std::chrono::milliseconds, 4> RETRY_AFTER{3s, 10s, 30s,
                                                               60s};
/// How old a profile kept on disk may get before it is looked up again - it
/// is still shown meanwhile
constexpr auto REFRESH_AFTER = std::chrono::hours(24 * 7);
/// How long a profile being looked up again waits before another try
constexpr auto REFRESH_RETRY = 10min;
/// How many profiles are kept on disk, the most recently answered
constexpr qsizetype KEEP_ON_DISK = 5000;
constexpr auto SAVE_AFTER = 5s;
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
    /// When Twitch last answered for it
    QDateTime answeredAt;
    std::chrono::steady_clock::time_point askedAt =
        std::chrono::steady_clock::now();
    /// A found one being looked up again
    bool refreshing = false;
    /// How often in a row it got no answer
    int unanswered = 0;
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

/// Names waiting to be asked about together. The lock guards it.
QStringList &gathered()
{
    static QStringList names;
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

QString diskFile()
{
    auto *app = tryGetApp();
    if (app == nullptr)
    {
        return {};
    }
    return app->getPaths().cacheFilePath(QStringLiteral("profiles.json"));
}

/// Takes in the profiles kept on disk, once. The caller holds the lock.
void loadFromDiskLocked()
{
    static bool loaded = false;
    if (loaded)
    {
        return;
    }
    loaded = true;

    QFile file(diskFile());
    if (file.fileName().isEmpty() || !file.open(QIODevice::ReadOnly))
    {
        return;
    }
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        const auto entry = it.value().toObject();
        if (known().contains(it.key()))
        {
            continue;
        }
        Known kept;
        kept.state = Known::State::Found;
        kept.profile = {
            .login = it.key(),
            .displayName = entry.value("displayName").toString(),
            .pictureUrl = entry.value("picture").toString(),
            .createdAt = QDateTime::fromString(
                entry.value("createdAt").toString(), Qt::ISODate),
        };
        kept.answeredAt = QDateTime::fromString(
            entry.value("answeredAt").toString(), Qt::ISODate);
        // One due to be looked up again may be, right away
        kept.askedAt = std::chrono::steady_clock::now() - REFRESH_RETRY;
        known().insert(it.key(), kept);
    }
}

/// Writes what Twitch answered to disk, a moment after the last answer.
/// GUI thread only.
void saveSoon()
{
    static QTimer *timer = [] {
        auto *timer = new QTimer(QCoreApplication::instance());
        timer->setSingleShot(true);
        timer->setInterval(SAVE_AFTER);
        QObject::connect(timer, &QTimer::timeout, [] {
            std::vector<std::pair<QString, Known>> found;
            {
                std::lock_guard lock(knownMutex());
                for (auto it = known().cbegin(); it != known().cend(); ++it)
                {
                    if (it->state == Known::State::Found &&
                        it->answeredAt.isValid())
                    {
                        found.emplace_back(it.key(), *it);
                    }
                }
            }
            std::sort(found.begin(), found.end(), [](auto &a, auto &b) {
                return a.second.answeredAt > b.second.answeredAt;
            });
            if (qsizetype(found.size()) > KEEP_ON_DISK)
            {
                found.resize(KEEP_ON_DISK);
            }

            QJsonObject root;
            for (const auto &[login, entry] : found)
            {
                root.insert(
                    login,
                    QJsonObject{
                        {"displayName", entry.profile.displayName},
                        {"picture", entry.profile.pictureUrl},
                        {"createdAt",
                         entry.profile.createdAt.toString(Qt::ISODate)},
                        {"answeredAt", entry.answeredAt.toString(Qt::ISODate)},
                    });
            }
            QSaveFile file(diskFile());
            if (!file.fileName().isEmpty() && file.open(QIODevice::WriteOnly))
            {
                file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
                file.commit();
            }
        });
        return timer;
    }();
    timer->start();
}

/// The names in @a logins that should be asked about now, marked as asked:
/// never asked, or found long ago and due to be looked up again. The caller
/// holds the lock.
QStringList claimLocked(const QStringList &logins)
{
    loadFromDiskLocked();

    QStringList claimed;
    const auto now = std::chrono::steady_clock::now();
    const auto nowUtc = QDateTime::currentDateTimeUtc();
    for (const auto &login : logins)
    {
        auto it = known().find(login);
        if (it == known().end())
        {
            known().insert(login, Known{});
            claimed.append(login);
        }
        else if (it->state == Known::State::Found && !it->refreshing &&
                 it->answeredAt.isValid() &&
                 std::chrono::seconds(it->answeredAt.secsTo(nowUtc)) >=
                     REFRESH_AFTER &&
                 now - it->askedAt >= REFRESH_RETRY)
        {
            // Shown as it is meanwhile
            it->refreshing = true;
            it->askedAt = now;
            claimed.append(login);
        }
    }
    return claimed;
}

/// Hands the profiles of @a logins to whatever was waiting for them. Those
/// waiting for a name that is no Twitch user are let go; those waiting for
/// one that got no answer keep waiting for the next try.
void settleWaiters(const QStringList &logins)
{
    runInGuiThread([logins] {
        for (const auto &login : logins)
        {
            std::optional<TwitchProfile> found;
            bool stillAsking = false;
            {
                std::lock_guard lock(knownMutex());
                const auto it = known().constFind(login);
                if (it != known().constEnd())
                {
                    if (it->state == Known::State::Found)
                    {
                        found = it->profile;
                    }
                    stillAsking = it->state == Known::State::NoAnswer ||
                                  it->state == Known::State::Asked;
                }
            }
            if (stillAsking)
            {
                continue;
            }

            auto waiting = waiters().take(login);
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

void askGathered();

/// Asks about @a logins together with whatever else comes in a moment
void lookUp(const QStringList &logins)
{
    bool first = false;
    {
        std::lock_guard lock(knownMutex());
        first = gathered().isEmpty();
        gathered().append(logins);
    }
    if (first)
    {
        // Queued, so names asked about while the app is still starting go
        // out once it is up
        postToThread([] {
            QTimer::singleShot(GATHER_FOR, [] {
                askGathered();
            });
        });
    }
}

/// Asks again about every name that got no answer - now, or @a after
void retryUnanswered(std::chrono::milliseconds after)
{
    postToThread([after] {
        QTimer::singleShot(after, [] {
            QStringList again;
            {
                std::lock_guard lock(knownMutex());
                for (auto it = known().begin(); it != known().end(); ++it)
                {
                    if (it->state == Known::State::NoAnswer)
                    {
                        it->state = Known::State::Asked;
                        it->askedAt = std::chrono::steady_clock::now();
                        again.append(it.key());
                    }
                }
            }
            if (!again.isEmpty())
            {
                lookUp(again);
            }
        });
    });
}

/// Once, on the GUI thread: asks again the moment a login is there, as
/// names asked about before it get no answer
void watchLogin()
{
    static bool watching = false;
    if (watching)
    {
        return;
    }
    watching = true;
    std::ignore =
        getApp()->getAccounts()->twitch.currentUserChanged.connect([] {
            retryUnanswered(0ms);
        });
}

void askGathered()
{
    watchLogin();

    QStringList logins;
    {
        std::lock_guard lock(knownMutex());
        for (const auto &login : gathered())
        {
            if (!logins.contains(login))
            {
                logins.append(login);
            }
        }
        gathered().clear();
    }

    for (qsizetype start = 0; start < logins.size(); start += LOOKUP_BATCH)
    {
        const auto batch = logins.mid(start, LOOKUP_BATCH);
        getHelix()->fetchUsers(
            {}, batch,
            [batch](const std::vector<HelixUser> &users) {
                {
                    std::lock_guard lock(knownMutex());
                    const auto now = QDateTime::currentDateTimeUtc();
                    for (const auto &user : users)
                    {
                        auto &entry = known()[user.login.toLower()];
                        entry.state = Known::State::Found;
                        entry.refreshing = false;
                        entry.unanswered = 0;
                        entry.answeredAt = now;
                        if (entry.profile.pictureUrl != user.profileImageUrl)
                        {
                            // A new picture - the chat one is made afresh
                            entry.image = nullptr;
                        }
                        entry.profile = {
                            .login = user.login.toLower(),
                            .displayName = user.displayName,
                            .pictureUrl = user.profileImageUrl,
                            .createdAt = QDateTime::fromString(user.createdAt,
                                                               Qt::ISODate),
                        };
                    }
                    for (const auto &login : batch)
                    {
                        auto &entry = known()[login];
                        entry.refreshing = false;
                        if (entry.state == Known::State::Asked)
                        {
                            entry.state = Known::State::NoSuchUser;
                        }
                    }
                }

                settleWaiters(batch);
                runInGuiThread([] {
                    saveSoon();
                    // Captions already in chat show the names as text - laid
                    // out again, they show the pictures
                    auto *app = tryGetApp();
                    if (app != nullptr && app->getWindows() != nullptr)
                    {
                        app->getWindows()->forceLayoutChannelViews();
                    }
                });
            },
            [batch] {
                int unanswered = 0;
                {
                    std::lock_guard lock(knownMutex());
                    const auto now = std::chrono::steady_clock::now();
                    for (const auto &login : batch)
                    {
                        auto &entry = known()[login];
                        entry.refreshing = false;
                        if (entry.state == Known::State::Asked)
                        {
                            entry.state = Known::State::NoAnswer;
                            entry.askedAt = now;
                            entry.unanswered++;
                            unanswered = std::max(unanswered, entry.unanswered);
                        }
                    }
                }
                if (unanswered > 0)
                {
                    const auto step = std::min<size_t>(unanswered - 1,
                                                       RETRY_AFTER.size() - 1);
                    retryUnanswered(RETRY_AFTER.at(step));
                }
            });
    }
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
        if (it != known().constEnd() && it->state == Known::State::NoSuchUser)
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
                it->image = Image::fromUrl(
                    Url{sizedPicture(it->profile.pictureUrl, 70)});
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
    loadFromDiskLocked();
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
