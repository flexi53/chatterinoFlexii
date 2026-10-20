// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/sonkerino/SonkerinoBadges.hpp"

#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "util/PostToThread.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringBuilder>
#include <QTimer>
#include <QUrl>

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace chatterino::sonkerinobadges {

namespace {

const QUrl LIST{QStringLiteral("https://sonkerino.kuestenfunk.org/v2/badges")};
constexpr auto RELOAD_EVERY = std::chrono::minutes(10);
constexpr int TIMEOUT_MS = 20000;

std::shared_mutex &mutex()
{
    static std::shared_mutex instance;
    return instance;
}

/// Twitch user id to badge. Guarded by mutex()
std::unordered_map<QString, EmotePtr> &byUser()
{
    static std::unordered_map<QString, EmotePtr> instance;
    return instance;
}

/// A picture from the sonkerino server itself - anything else in the list
/// is left out, so the list cannot point the chat elsewhere
bool fromThere(const QString &url)
{
    const QUrl parsed(url);
    return parsed.scheme() == u"https" && parsed.host() == LIST.host();
}

void load()
{
    NetworkRequest(LIST)
        .timeout(TIMEOUT_MS)
        .concurrent()
        .onSuccess([](const NetworkResult &result) {
            const auto root = result.parseJson();

            // "badges": [{id, tooltip, images: {1x, 2x, 3x}}]
            std::unordered_map<QString, EmotePtr> byBadge;
            for (const auto &value : root.value("badges").toArray())
            {
                const auto badge = value.toObject();
                const auto id = badge.value("id").toString();
                const auto images = badge.value("images").toObject();
                const auto x1 = images.value("1x").toString();
                const auto x2 = images.value("2x").toString();
                const auto x3 = images.value("3x").toString();
                if (id.isEmpty() || !fromThere(x1) || !fromThere(x2) ||
                    !fromThere(x3))
                {
                    continue;
                }
                constexpr QSize baseSize(18, 18);
                byBadge.emplace(
                    id, std::make_shared<const Emote>(Emote{
                            .name = EmoteName{u"sonkerino:" % id},
                            .images =
                                ImageSet{
                                    Image::fromUrl(Url{x1}, 1.0, baseSize),
                                    Image::fromUrl(Url{x2}, 0.5, baseSize * 2),
                                    Image::fromUrl(Url{x3}, 0.25,
                                                   baseSize * 4),
                                },
                            .tooltip = Tooltip{QStringLiteral(
                                "sonkerino-Badge")},
                            .homePage = Url{QStringLiteral(
                                "https://sonkerino.kuestenfunk.org")},
                        }));
            }

            // "users": {"<twitch id>": {activeBadge, badges: [...]}} - the
            // one they picked, or else their first
            std::unordered_map<QString, EmotePtr> found;
            const auto users = root.value("users").toObject();
            for (auto it = users.begin(); it != users.end(); ++it)
            {
                const auto user = it.value().toObject();
                auto id = user.value("activeBadge").toString();
                if (id.isEmpty())
                {
                    id = user.value("badges").toArray().first().toString();
                }
                const auto badge = byBadge.find(id);
                if (badge != byBadge.end())
                {
                    found.emplace(it.key(), badge->second);
                }
            }

            std::unique_lock lock(mutex());
            byUser() = std::move(found);
        })
        .onError([](const NetworkResult &result) {
            qCWarning(chatterinoApp)
                << "Failed to load sonkerino badges:" << result.formatError();
        })
        .execute();
}

/// Fetches the list once now and every few minutes after - started by the
/// first message that asks for a badge
void startOnce()
{
    static std::once_flag started;
    std::call_once(started, [] {
        postToThread([] {
            auto *timer = new QTimer(QCoreApplication::instance());
            timer->setInterval(RELOAD_EVERY);
            QObject::connect(timer, &QTimer::timeout, [] {
                load();
            });
            timer->start();
            load();
        });
    });
}

}  // namespace

std::optional<EmotePtr> badgeOf(const QString &userId)
{
    startOnce();

    std::shared_lock lock(mutex());
    const auto it = byUser().find(userId);
    if (it == byUser().end())
    {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace chatterino::sonkerinobadges
