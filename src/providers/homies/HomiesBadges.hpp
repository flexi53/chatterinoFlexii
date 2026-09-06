// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Aliases.hpp"

#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace chatterino {

struct Emote;
using EmotePtr = std::shared_ptr<const Emote>;

/// Badges from Chatterino Homies (https://chatterinohomies.com), where users
/// can upload a badge of their own that is then shown next to their name.
///
/// The list is fetched once at startup, the same way Chatterino's own badges
/// are loaded. Each user has at most one badge.
class HomiesBadges
{
public:
    HomiesBadges();

    HomiesBadges(const HomiesBadges &) = delete;
    HomiesBadges(HomiesBadges &&) = delete;
    HomiesBadges &operator=(const HomiesBadges &) = delete;
    HomiesBadges &operator=(HomiesBadges &&) = delete;
    ~HomiesBadges() = default;

    /// The badge for the given Twitch user, if they have one
    std::optional<EmotePtr> getBadge(const UserId &id);

private:
    void loadHomiesBadges();

    std::shared_mutex mutex_;

    /// Maps Twitch user IDs to an index into emotes_. Guarded by mutex_
    std::unordered_map<QString, int> badgeMap_;

    /// The badges themselves, referred to by badgeMap_
    std::vector<EmotePtr> emotes_;
};

}  // namespace chatterino
