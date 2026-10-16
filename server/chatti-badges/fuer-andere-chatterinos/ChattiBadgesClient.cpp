// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "ChattiBadgesClient.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "util/PostToThread.hpp"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonObject>
#include <QPainter>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace chatterino::chattibadgesclient {

namespace {

// ----- To set ---------------------------------------------------------

/// The badge server
const QString SERVER = QStringLiteral(
    "https://chatti-badges.chattiflexii.workers.dev");

/// This program's name - hovering a badge uploaded from here shows
/// "<name>-Badge"
const QString CLIENT_NAME = QStringLiteral("Sonkerino");

// ----------------------------------------------------------------------

constexpr auto RELOAD_EVERY = std::chrono::minutes(10);
constexpr int BADGE_SIDE = 72;
constexpr int MAX_GIF_SIDE = 240;
constexpr qsizetype MAX_GIF_BYTES = 1024 * 1024;

std::shared_mutex &mutex()
{
    static std::shared_mutex instance;
    return instance;
}

/// Twitch user id to badge. Guarded by mutex()
std::unordered_map<QString, EmotePtr> &badges()
{
    static std::unordered_map<QString, EmotePtr> instance;
    return instance;
}

void reload()
{
    NetworkRequest(QUrl(SERVER + QStringLiteral("/badges")))
        .timeout(20000)
        .concurrent()
        .onSuccess([](const NetworkResult &result) {
            std::unordered_map<QString, EmotePtr> found;
            const auto users = result.parseJson().value("users").toObject();
            for (auto it = users.begin(); it != users.end(); ++it)
            {
                const auto user = it.value().toObject();
                const auto image = user.value("image").toString();
                if (image.isEmpty())
                {
                    continue;
                }
                const auto side =
                    std::max(1, user.value("size").toInt(BADGE_SIDE));
                const auto client =
                    user.value("client").toString(QStringLiteral("ChattiFlexii"));
                found.emplace(
                    it.key(),
                    std::make_shared<const Emote>(Emote{
                        .name = EmoteName{QStringLiteral("chatti:") +
                                          user.value("login").toString()},
                        .images = ImageSet{Image::fromUrl(
                            Url{image}, 18.0 / side, QSize(side, side))},
                        .tooltip = Tooltip{client + QStringLiteral("-Badge")},
                        .homePage = Url{},
                    }));
            }
            std::unique_lock lock(mutex());
            badges() = std::move(found);
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
                reload();
            });
            timer->start();
            reload();
        });
    });
}

/// @a picture made what the server takes for a still one: a PNG of 72 × 72
QByteArray badgePng(const QImage &picture)
{
    QImage badge(BADGE_SIDE, BADGE_SIDE, QImage::Format_ARGB32_Premultiplied);
    badge.fill(Qt::transparent);
    const auto fitted = picture.scaled(BADGE_SIDE, BADGE_SIDE,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
    QPainter painter(&badge);
    painter.drawImage((BADGE_SIDE - fitted.width()) / 2,
                      (BADGE_SIDE - fitted.height()) / 2, fitted);
    painter.end();
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    badge.save(&buffer, "PNG");
    return png;
}

}  // namespace

void appendTo(MessageBuilder &builder, const QString &userID)
{
    startOnce();

    EmotePtr badge;
    {
        std::shared_lock lock(mutex());
        const auto it = badges().find(userID);
        if (it == badges().end())
        {
            return;
        }
        badge = it->second;
    }
    // Shown and hidden along with the Chatterino badges
    builder.emplace<BadgeElement>(badge, MessageElementFlag::BadgeChatterino);
}

QString uploadCommand(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return {};
    }
    const auto channel = ctx.channel;

    auto path = ctx.words.mid(1).join(u' ').trimmed();
    if (path.startsWith(u'~'))
    {
        path = QDir::homePath() + path.mid(1);
    }
    if (path.isEmpty())
    {
        channel->addSystemMessage(QStringLiteral(
            "Benutzung: /chattibadge /pfad/zum/bild.png - ein Bild wird 72 × "
            "72, ein animiertes GIF geht so hoch, wie es ist (höchstens 240 × "
            "240 und 1 MB)."));
        return {};
    }

    const auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (account->isAnon())
    {
        channel->addSystemMessage(
            QStringLiteral("Dafür musst du bei Twitch eingeloggt sein."));
        return {};
    }

    // An animated GIF goes as it is - made smaller here it would stop moving
    QByteArray picture;
    bool animated = false;
    QImageReader reader(path);
    if (reader.format() == "gif" && reader.supportsAnimation() &&
        reader.imageCount() > 1)
    {
        const auto size = reader.size();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            channel->addSystemMessage(
                QStringLiteral("Das GIF ließ sich nicht öffnen."));
            return {};
        }
        picture = file.readAll();
        if (size.width() > MAX_GIF_SIDE || size.height() > MAX_GIF_SIDE ||
            picture.size() > MAX_GIF_BYTES)
        {
            channel->addSystemMessage(
                QStringLiteral("Das GIF ist zu groß - höchstens 240 × 240 und "
                               "1 MB."));
            return {};
        }
        animated = true;
    }
    else
    {
        const QImage image(path);
        if (image.isNull())
        {
            channel->addSystemMessage(
                QStringLiteral("Das Bild ließ sich nicht öffnen."));
            return {};
        }
        picture = badgePng(image);
    }

    channel->addSystemMessage(QStringLiteral("Lade dein Badge hoch …"));
    NetworkRequest(QUrl(SERVER + QStringLiteral("/upload")),
                   NetworkRequestType::Post)
        .timeout(20000)
        .header("Authorization", "OAuth " + account->getOAuthToken())
        .header("X-Badge-Client", CLIENT_NAME)
        .header("Content-Type", animated ? "image/gif" : "image/png")
        .payload(picture)
        .onSuccess([channel](const NetworkResult &result) {
            const auto answer = result.parseJson();
            channel->addSystemMessage(
                answer.contains("approved")
                    ? QStringLiteral("Dein Badge ist hochgeladen und "
                                     "freigegeben.")
                    : QStringLiteral("Dein Badge ist hochgeladen und wartet "
                                     "auf Freigabe."));
            reload();
        })
        .onError([channel](const NetworkResult &result) {
            const auto said = result.parseJson().value("error").toString();
            channel->addSystemMessage(
                said.isEmpty() ? QStringLiteral("Der Badge-Server antwortet "
                                                "nicht (%1).")
                                     .arg(result.formatError())
                               : said);
        })
        .execute();
    return {};
}

}  // namespace chatterino::chattibadgesclient
