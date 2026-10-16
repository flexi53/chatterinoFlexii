// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/chattibadges/ChattiBadges.hpp"

#include "Application.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Settings.hpp"

#include <QBuffer>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QStringBuilder>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <chrono>

namespace chatterino {

namespace {

/// How often the list is fetched again - a badge just approved shows up
/// everywhere within this
constexpr auto RELOAD_EVERY = std::chrono::minutes(10);
constexpr int BADGE_SIDE = 72;
constexpr int TIMEOUT_MS = 20000;

/// What went wrong, as the server says it, or as the network does
QString errorOf(const NetworkResult &result)
{
    const auto said = result.parseJson().value("error").toString();
    if (!said.isEmpty())
    {
        return said;
    }
    return QStringLiteral("Der Badge-Server antwortet nicht (%1).")
        .arg(result.formatError());
}

/// A request to the server, with the login of the account here where
/// @a signedIn - or nothing, where there is no server or no login
std::optional<NetworkRequest> request(const QString &path,
                                      NetworkRequestType type, bool signedIn,
                                      const chattibadges::Failed &failed)
{
    const auto base = chattibadges::server();
    if (base.isEmpty())
    {
        failed(QStringLiteral("Es ist noch kein Badge-Server eingetragen."));
        return std::nullopt;
    }
    auto built = NetworkRequest(QUrl(base + path), type).timeout(TIMEOUT_MS);
    if (signedIn)
    {
        const auto account = getApp()->getAccounts()->twitch.getCurrent();
        if (account->isAnon())
        {
            failed(QStringLiteral("Dafür musst du bei Twitch eingeloggt "
                                  "sein."));
            return std::nullopt;
        }
        built = std::move(built).header("Authorization",
                                        "OAuth " + account->getOAuthToken());
    }
    return built;
}

/// Runs @a built, handing a success to @a done and anything else to
/// @a failed
void send(NetworkRequest built, QObject *caller,
          std::function<void(const QJsonObject &)> done,
          chattibadges::Failed failed)
{
    std::move(built)
        .caller(caller)
        .onSuccess([done](const NetworkResult &result) {
            done(result.parseJson());
        })
        .onError([failed](const NetworkResult &result) {
            failed(errorOf(result));
        })
        .execute();
}

}  // namespace

ChattiBadges::ChattiBadges()
{
    this->timer_ = new QTimer;
    this->timer_->setInterval(RELOAD_EVERY);
    QObject::connect(this->timer_, &QTimer::timeout, [this] {
        this->reload();
    });
    this->timer_->start();

    // A server put in or changed is asked at once
    getSettings()->chattiBadgesServer.connect(
        [this](const QString &, auto) {
            this->reload();
        },
        this->connections_, false);

    this->reload();
}

ChattiBadges::~ChattiBadges()
{
    delete this->timer_;
}

std::optional<EmotePtr> ChattiBadges::getBadge(const UserId &id)
{
    std::shared_lock lock(this->mutex_);
    auto it = this->badgeMap_.find(id.string);
    if (it != this->badgeMap_.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void ChattiBadges::reload()
{
    const auto base = chattibadges::server();
    if (base.isEmpty())
    {
        return;
    }

    NetworkRequest(QUrl(base + QStringLiteral("/badges")))
        .timeout(TIMEOUT_MS)
        .concurrent()
        .onSuccess([this](const NetworkResult &result) {
            std::unordered_map<QString, EmotePtr> badgeMap;
            const auto users = result.parseJson().value("users").toObject();
            for (auto it = users.begin(); it != users.end(); ++it)
            {
                const auto user = it.value().toObject();
                const auto image = user.value("image").toString();
                if (image.isEmpty())
                {
                    continue;
                }
                const auto login = user.value("login").toString();
                // One picture, as wide as the server says - a PNG made 72
                // px here, a GIF as it came - shown as high as a badge is
                const auto side =
                    std::max(1, user.value("size").toInt(BADGE_SIDE));
                auto emote = std::make_shared<const Emote>(Emote{
                    .name = EmoteName{u"chatti:" % login},
                    .images = ImageSet{Image::fromUrl(
                        Url{image}, 18.0 / side, QSize(side, side))},
                    // The program it was uploaded with - ChattiFlexii, or a
                    // friend's own that uses the same server
                    .tooltip = Tooltip{
                        user.value("client")
                            .toString(QStringLiteral("ChattiFlexii")) +
                        QStringLiteral("-Badge")},
                    .homePage = Url{},
                });
                badgeMap.emplace(it.key(), std::move(emote));
            }

            std::unique_lock lock(this->mutex_);
            this->badgeMap_ = std::move(badgeMap);
            qCDebug(chatterinoChattiBadges)
                << "Loaded" << this->badgeMap_.size() << "ChattiFlexii badges";
        })
        .onError([](const NetworkResult &result) {
            qCWarning(chatterinoChattiBadges)
                << "Failed to load ChattiFlexii badges:"
                << result.formatError();
        })
        .execute();
}

namespace chattibadges {

QString server()
{
    auto base = getSettings()->chattiBadgesServer.getValue().trimmed();
    while (base.endsWith(u'/'))
    {
        base.chop(1);
    }
    return base;
}

QByteArray badgePng(const QImage &picture)
{
    QImage badge(BADGE_SIDE, BADGE_SIDE, QImage::Format_ARGB32_Premultiplied);
    badge.fill(Qt::transparent);
    if (!picture.isNull())
    {
        const auto fitted =
            picture.scaled(BADGE_SIDE, BADGE_SIDE, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
        QPainter painter(&badge);
        painter.drawImage((BADGE_SIDE - fitted.width()) / 2,
                          (BADGE_SIDE - fitted.height()) / 2, fitted);
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    badge.save(&buffer, "PNG");
    return png;
}

void me(QObject *caller, std::function<void(const Me &)> done, Failed failed)
{
    auto built = request(QStringLiteral("/me"), NetworkRequestType::Get, true,
                         failed);
    if (!built)
    {
        return;
    }
    send(
        std::move(*built), caller,
        [done](const QJsonObject &answer) {
            done(Me{
                .userId = answer.value("userId").toString(),
                .login = answer.value("login").toString(),
                .approved = answer.value("approved").toString(),
                .pending = answer.value("pending").toString(),
                .isAdmin = answer.value("isAdmin").toBool(),
            });
        },
        failed);
}

void upload(const QByteArray &picture, bool animated, QObject *caller,
            std::function<void()> done, Failed failed)
{
    auto built = request(QStringLiteral("/upload"), NetworkRequestType::Post,
                         true, failed);
    if (!built)
    {
        return;
    }
    send(std::move(*built)
             .header("Content-Type", animated ? "image/gif" : "image/png")
             .header("X-Badge-Client", "ChattiFlexii")
             .payload(picture),
         caller,
         [done](const QJsonObject &) {
             if (auto *badges = getApp()->getChattiBadges())
             {
                 badges->reload();
             }
             done();
         },
         failed);
}

void removeMine(QObject *caller, std::function<void()> done, Failed failed)
{
    auto built = request(QStringLiteral("/mine"), NetworkRequestType::Delete,
                         true, failed);
    if (!built)
    {
        return;
    }
    send(
        std::move(*built), caller,
        [done](const QJsonObject &) {
            if (auto *badges = getApp()->getChattiBadges())
            {
                badges->reload();
            }
            done();
        },
        failed);
}

void waiting(QObject *caller,
             std::function<void(const std::vector<Waiting> &)> done,
             Failed failed)
{
    auto built = request(QStringLiteral("/pending"), NetworkRequestType::Get,
                         true, failed);
    if (!built)
    {
        return;
    }
    send(
        std::move(*built), caller,
        [done](const QJsonObject &answer) {
            std::vector<Waiting> found;
            for (const auto &value : answer.value("pending").toArray())
            {
                const auto entry = value.toObject();
                found.push_back(Waiting{
                    .userId = entry.value("userId").toString(),
                    .login = entry.value("login").toString(),
                    .image = entry.value("image").toString(),
                    .client = entry.value("client").toString(
                        QStringLiteral("ChattiFlexii")),
                });
            }
            done(found);
        },
        failed);
}

void approve(const QString &userId, QObject *caller, std::function<void()> done,
             Failed failed)
{
    auto built = request(QStringLiteral("/approve/") + userId,
                         NetworkRequestType::Post, true, failed);
    if (!built)
    {
        return;
    }
    send(
        std::move(*built), caller,
        [done](const QJsonObject &) {
            if (auto *badges = getApp()->getChattiBadges())
            {
                badges->reload();
            }
            done();
        },
        failed);
}

void reject(const QString &userId, QObject *caller, std::function<void()> done,
            Failed failed)
{
    auto built = request(QStringLiteral("/reject/") + userId,
                         NetworkRequestType::Post, true, failed);
    if (!built)
    {
        return;
    }
    send(
        std::move(*built), caller,
        [done](const QJsonObject &) {
            done();
        },
        failed);
}

}  // namespace chattibadges

}  // namespace chatterino
