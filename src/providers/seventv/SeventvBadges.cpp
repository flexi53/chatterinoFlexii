// SPDX-FileCopyrightText: 2022 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/seventv/SeventvBadges.hpp"

#include "messages/Emote.hpp"
#include "messages/Image.hpp"  // IWYU pragma: keep
#include "providers/seventv/SeventvEmotes.hpp"
#include "singletons/Settings.hpp"
#include "util/PostToThread.hpp"

namespace chatterino {

QString SeventvBadges::idForBadge(const QJsonObject &badgeJson) const
{
    return badgeJson["id"].toString();
}

EmotePtr SeventvBadges::createBadge(const QString &id,
                                    const QJsonObject &badgeJson) const
{
    auto emote = Emote{
        // We utilize the "emote" "name" for filtering badges, and expect
        // the format to be "7tv:badge name" (e.g. "7tv:NNYS 2024")
        .name = EmoteName{u"7tv:" % badgeJson["name"].toString()},
        .images = SeventvEmotes::createImageSet(
            badgeJson, !getSettings()->animateSevenTVBadges),
        .tooltip = Tooltip{badgeJson["tooltip"].toString()},
        .homePage = Url{},
        .id = EmoteId{id},
    };

    if (emote.images.getImage1()->isEmpty())
    {
        return nullptr;  // Bad images
    }

    // ChattiFlexii: loaded as soon as 7TV names the badge rather than when it
    // is first shown, so it is there by the time a chatter who has it writes
    runInGuiThread([images = emote.images] {
        images.getImage1()->load();
        images.getImage2()->load();
    });

    return std::make_shared<const Emote>(std::move(emote));
}

}  // namespace chatterino
