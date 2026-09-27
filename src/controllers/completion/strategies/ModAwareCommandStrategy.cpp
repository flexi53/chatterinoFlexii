// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/completion/strategies/ModAwareCommandStrategy.hpp"

#include <algorithm>

namespace chatterino::completion {

namespace {

/// What Twitch only lets a moderator do. Named without their slash, as the
/// suggestions keep them.
const QStringList MOD_ONLY{
    "ban",        "unban",       "timeout",   "untimeout",  "delete",
    "clear",      "slow",        "slowoff",   "followers",  "followersoff",
    "subscribers", "subscribersoff", "emoteonly", "emoteonlyoff",
    "uniquechat", "uniquechatoff", "r9kbeta",  "r9kbetaoff", "vip",
    "unvip",      "vips",        "mods",      "mod",        "unmod",
    "announce",   "announceblue", "announcegreen", "announceorange",
    "announcepurple", "warn",    "raid",      "unraid",     "shoutout",
    "marker",     "automod",     "monitor",   "unmonitor",  "restrict",
    "unrestrict", "shieldmode",  "shieldmodeoff", "chatters",
    "blockterm",  "unblockterm", "blockterms", "pin",       "unpin",
    "nuke",
};

}  // namespace

ModAwareCommandStrategy::ModAwareCommandStrategy(bool startsWithOnly,
                                                 bool moderates)
    : CommandStrategy(startsWithOnly)
    , moderates_(moderates)
{
}

bool ModAwareCommandStrategy::onlyForMods(const QString &name)
{
    return MOD_ONLY.contains(name, Qt::CaseInsensitive);
}

void ModAwareCommandStrategy::apply(const std::vector<CommandItem> &items,
                                    std::vector<CommandItem> &output,
                                    const QString &query) const
{
    CommandStrategy::apply(items, output, query);

    if (this->moderates_)
    {
        return;
    }

    std::erase_if(output, [](const CommandItem &item) {
        return onlyForMods(item.name);
    });
}

}  // namespace chatterino::completion
