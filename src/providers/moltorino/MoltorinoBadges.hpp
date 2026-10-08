// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Aliases.hpp"

#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace chatterino {

struct Emote;
using EmotePtr = std::shared_ptr<const Emote>;

/// ChattiFlexii: the badges Moltorino (https://moltorino.com) gives its
/// developers, founders, donors and supporters.
///
/// The list is fetched once at startup. Someone may have several of them;
/// shown is the one they picked, or else the one Moltorino puts first.
class MoltorinoBadges
{
public:
    MoltorinoBadges();

    MoltorinoBadges(const MoltorinoBadges &) = delete;
    MoltorinoBadges(MoltorinoBadges &&) = delete;
    MoltorinoBadges &operator=(const MoltorinoBadges &) = delete;
    MoltorinoBadges &operator=(MoltorinoBadges &&) = delete;
    ~MoltorinoBadges() = default;

    /// The badge for the given Twitch user, if they have one
    std::optional<EmotePtr> getBadge(const UserId &id);

private:
    void loadBadges();

    std::shared_mutex mutex_;

    /// Maps Twitch user IDs to their badge. Guarded by mutex_
    std::unordered_map<QString, EmotePtr> badgeMap_;
};

}  // namespace chatterino
