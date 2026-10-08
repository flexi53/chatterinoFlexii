// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/homies/HomiesBadges.hpp"

#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>

namespace chatterino {

HomiesBadges::HomiesBadges()
{
    this->loadHomiesBadges();
    this->loadSupporterBadges();
}

std::optional<EmotePtr> HomiesBadges::getBadge(const UserId &id)
{
    std::shared_lock lock(this->mutex_);

    auto it = this->badgeMap_.find(id.string);
    if (it != this->badgeMap_.end())
    {
        return this->emotes_[it->second];
    }

    return std::nullopt;
}

std::optional<EmotePtr> HomiesBadges::getSupporterBadge(const UserId &id)
{
    std::shared_lock lock(this->mutex_);

    auto it = this->supporterMap_.find(id.string);
    if (it != this->supporterMap_.end())
    {
        return it->second;
    }

    return std::nullopt;
}

void HomiesBadges::loadHomiesBadges()
{
    static QUrl url("https://chatterinohomies.com/api/badges/list");

    NetworkRequest(url)
        .concurrent()
        .onSuccess([this](auto result) {
            auto jsonRoot = result.parseJson();

            std::unique_lock lock(this->mutex_);

            // Unlike Chatterino's own badge list, every entry here belongs to
            // exactly one user, so there is no "users" array to walk.
            for (const auto &jsonBadgeValue :
                 jsonRoot.value("badges").toArray())
            {
                auto jsonBadge = jsonBadgeValue.toObject();

                auto userId = jsonBadge.value("userId").toString();
                if (userId.isEmpty())
                {
                    continue;
                }

                constexpr QSize baseSize(18, 18);
                auto tooltip = jsonBadge.value("tooltip").toString();

                auto emote = Emote{
                    .name = EmoteName{u"homies:" % tooltip},
                    .images =
                        ImageSet{
                            Image::fromUrl(
                                Url{jsonBadge.value("image1").toString()}, 1.0,
                                baseSize),
                            Image::fromUrl(
                                Url{jsonBadge.value("image2").toString()}, 0.5,
                                baseSize * 2),
                            Image::fromUrl(
                                Url{jsonBadge.value("image3").toString()}, 0.25,
                                baseSize * 4),
                        },
                    .tooltip = Tooltip{tooltip},
                    .homePage = Url{"https://chatterinohomies.com/badges"},
                };

                this->emotes_.push_back(
                    std::make_shared<const Emote>(std::move(emote)));
                this->badgeMap_[userId] =
                    static_cast<int>(this->emotes_.size()) - 1;
            }

            qCDebug(chatterinoHomies)
                << "Loaded" << this->badgeMap_.size() << "Homies badges";
        })
        .onError([](const auto &result) {
            qCWarning(chatterinoHomies)
                << "Failed to load Homies badges:" << result.formatError();
        })
        .execute();
}

void HomiesBadges::loadSupporterBadges()
{
    // The badges Homies gives its developers and supporters are not in the
    // list above - they are kept by itzAlex, who runs Homies, one entry per
    // badge with everyone who has it
    static QUrl url("https://itzalex.github.io/badges");

    NetworkRequest(url)
        .concurrent()
        .onSuccess([this](auto result) {
            auto jsonRoot = result.parseJson();

            std::unique_lock lock(this->mutex_);

            for (const auto &jsonBadgeValue :
                 jsonRoot.value("badges").toArray())
            {
                auto jsonBadge = jsonBadgeValue.toObject();

                constexpr QSize baseSize(18, 18);
                auto tooltip = jsonBadge.value("tooltip").toString();

                auto emote = std::make_shared<const Emote>(Emote{
                    .name = EmoteName{u"homies:" % tooltip},
                    .images =
                        ImageSet{
                            Image::fromUrl(
                                Url{jsonBadge.value("image1").toString()}, 1.0,
                                baseSize),
                            Image::fromUrl(
                                Url{jsonBadge.value("image2").toString()}, 0.5,
                                baseSize * 2),
                            Image::fromUrl(
                                Url{jsonBadge.value("image3").toString()}, 0.25,
                                baseSize * 4),
                        },
                    .tooltip = Tooltip{tooltip},
                    .homePage = Url{"https://chatterinohomies.com"},
                });

                // Someone listed twice keeps the first one - the list goes
                // from developer down
                for (const auto &user : jsonBadge.value("users").toArray())
                {
                    auto userId = user.toString();
                    if (!userId.isEmpty())
                    {
                        this->supporterMap_.try_emplace(userId, emote);
                    }
                }
            }

            qCDebug(chatterinoHomies)
                << "Loaded" << this->supporterMap_.size()
                << "Homies supporter badges";
        })
        .onError([](const auto &result) {
            qCWarning(chatterinoHomies)
                << "Failed to load Homies supporter badges:"
                << result.formatError();
        })
        .execute();
}

}  // namespace chatterino
