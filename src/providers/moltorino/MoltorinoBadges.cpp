// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/moltorino/MoltorinoBadges.hpp"

#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>

#include <limits>

namespace chatterino {

MoltorinoBadges::MoltorinoBadges()
{
    this->loadBadges();
}

std::optional<EmotePtr> MoltorinoBadges::getBadge(const UserId &id)
{
    std::shared_lock lock(this->mutex_);

    auto it = this->badgeMap_.find(id.string);
    if (it != this->badgeMap_.end())
    {
        return it->second;
    }

    return std::nullopt;
}

void MoltorinoBadges::loadBadges()
{
    static QUrl url("https://api.moltorino.com/v2/badges");

    NetworkRequest(url)
        .concurrent()
        .onSuccess([this](auto result) {
            auto jsonRoot = result.parseJson();

            struct Badge {
                EmotePtr emote;
                double priority;
            };
            std::unordered_map<QString, Badge> badges;

            for (const auto &jsonBadgeValue :
                 jsonRoot.value("badges").toArray())
            {
                auto jsonBadge = jsonBadgeValue.toObject();
                auto id = jsonBadge.value("id").toString();
                auto images = jsonBadge.value("images").toObject();
                if (id.isEmpty() || images.value("1x").toString().isEmpty())
                {
                    continue;
                }

                constexpr QSize baseSize(18, 18);
                auto tooltip = jsonBadge.value("tooltip").toString();

                auto emote = std::make_shared<const Emote>(Emote{
                    .name = EmoteName{u"moltorino:" % tooltip},
                    .images =
                        ImageSet{
                            Image::fromUrl(Url{images.value("1x").toString()},
                                           1.0, baseSize),
                            Image::fromUrl(Url{images.value("2x").toString()},
                                           0.5, baseSize * 2),
                            Image::fromUrl(Url{images.value("4x").toString()},
                                           0.25, baseSize * 4),
                        },
                    .tooltip = Tooltip{tooltip},
                    .homePage = Url{"https://moltorino.com"},
                });
                badges.emplace(
                    id, Badge{
                            .emote = std::move(emote),
                            .priority = jsonBadge.value("priority").toDouble(
                                std::numeric_limits<double>::max()),
                        });
            }

            std::unordered_map<QString, EmotePtr> badgeMap;
            auto users = jsonRoot.value("users").toObject();
            for (auto it = users.begin(); it != users.end(); ++it)
            {
                auto user = it.value().toObject();

                // The one they picked themselves - none at all if they
                // picked to show none
                if (user.contains("activeBadge"))
                {
                    auto active = user.value("activeBadge");
                    if (active.isNull())
                    {
                        continue;
                    }
                    auto found = badges.find(active.toString());
                    if (found != badges.end())
                    {
                        badgeMap.emplace(it.key(), found->second.emote);
                        continue;
                    }
                }

                // Otherwise the one Moltorino puts first: the lowest priority
                const Badge *best = nullptr;
                for (const auto &badgeId : user.value("badges").toArray())
                {
                    auto found = badges.find(badgeId.toString());
                    if (found != badges.end() &&
                        (best == nullptr ||
                         found->second.priority < best->priority))
                    {
                        best = &found->second;
                    }
                }
                if (best != nullptr)
                {
                    badgeMap.emplace(it.key(), best->emote);
                }
            }

            std::unique_lock lock(this->mutex_);
            this->badgeMap_ = std::move(badgeMap);

            qCDebug(chatterinoMoltorino)
                << "Loaded" << this->badgeMap_.size() << "Moltorino badges";
        })
        .onError([](const auto &result) {
            qCWarning(chatterinoMoltorino)
                << "Failed to load Moltorino badges:" << result.formatError();
        })
        .execute();
}

}  // namespace chatterino
