// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/CaptionAvatars.hpp"

#include "Application.hpp"
#include "messages/Image.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "singletons/WindowManager.hpp"
#include "util/PostToThread.hpp"

#include <QHash>

#include <chrono>
#include <mutex>

namespace {

using namespace chatterino;

/// Twitch answers for up to this many names at once
constexpr qsizetype LOOKUP_BATCH = 100;
/// How long a name that got no answer - no connection, not logged in yet -
/// waits before it is asked about again
constexpr auto RETRY_AFTER = std::chrono::minutes(1);

struct Known {
    enum class State {
        Asked,
        Found,
        NoSuchUser,
        NoAnswer,
    };

    State state = State::Asked;
    QString displayName;
    QString pictureUrl;
    /// Made from pictureUrl the first time it is wanted
    ImagePtr image;
    std::chrono::steady_clock::time_point askedAt =
        std::chrono::steady_clock::now();
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

/// Profile pictures are linked at 300x300. Twitch serves them at 70x70 too,
/// plenty for a caption.
QString smallPicture(QString url)
{
    return url.replace(QStringLiteral("300x300"), QStringLiteral("70x70"));
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
                            entry.displayName = user.displayName;
                            entry.pictureUrl =
                                smallPicture(user.profileImageUrl);
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

                    // Captions already in chat show the names as text - laid
                    // out again, they show the pictures
                    runInGuiThread([] {
                        getApp()->getWindows()->forceLayoutChannelViews();
                    });
                },
                [batch] {
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
                });
        }
    });
}

}  // namespace

namespace chatterino::captionavatars {

QString loginOf(const QString &word)
{
    // Twitch logins are up to 25 of a-z, 0-9 and _
    if (word.size() < 2 || word.size() > 26 || !word.startsWith(u'@'))
    {
        return {};
    }

    auto login = word.mid(1).toLower();
    for (const auto c : login)
    {
        const bool allowed = (c >= u'a' && c <= u'z') ||
                             (c >= u'0' && c <= u'9') || c == u'_';
        if (!allowed)
        {
            return {};
        }
    }
    return login;
}

std::shared_ptr<Image> image(const QString &login)
{
    QStringList toAsk;
    ImagePtr result;
    {
        std::lock_guard lock(knownMutex());
        toAsk = claimLocked({login});

        auto it = known().find(login);
        if (it != known().end() && it->state == Known::State::Found)
        {
            if (!it->image && !it->pictureUrl.isEmpty())
            {
                it->image = Image::fromUrl(Url{it->pictureUrl});
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

QString displayName(const QString &login)
{
    std::lock_guard lock(knownMutex());
    const auto it = known().constFind(login);
    if (it != known().constEnd() && !it->displayName.isEmpty())
    {
        return it->displayName;
    }
    return login;
}

void prefetch(const QStringList &captions)
{
    QStringList logins;
    for (const auto &caption : captions)
    {
        for (const auto &word : caption.split(u' ', Qt::SkipEmptyParts))
        {
            const auto login = loginOf(word);
            if (!login.isEmpty() && !logins.contains(login))
            {
                logins.append(login);
            }
        }
    }

    QStringList toAsk;
    {
        std::lock_guard lock(knownMutex());
        toAsk = claimLocked(logins);
    }
    if (!toAsk.isEmpty())
    {
        lookUp(toAsk);
    }
}

void remember(const QString &login, const QString &displayName,
              std::shared_ptr<Image> image)
{
    std::lock_guard lock(knownMutex());
    auto &entry = known()[login.toLower()];
    entry.state = Known::State::Found;
    entry.displayName = displayName;
    entry.image = std::move(image);
}

}  // namespace chatterino::captionavatars
