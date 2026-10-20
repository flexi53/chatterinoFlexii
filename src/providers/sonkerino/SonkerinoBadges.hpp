// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

#include <memory>
#include <optional>

namespace chatterino {

struct Emote;
using EmotePtr = std::shared_ptr<const Emote>;

/// The badges people uploaded on sonkerino.kuestenfunk.org - a friend's own
/// Chatterino, which in turn shows ChattiFlexii's. Only the public list is
/// read, with no login, and only pictures from that server are loaded.
namespace sonkerinobadges {

/// The sonkerino badge of the Twitch user @a userId, if they wear one. The
/// first call starts fetching the list, which is fetched again every few
/// minutes after.
std::optional<EmotePtr> badgeOf(const QString &userId);

}  // namespace sonkerinobadges

}  // namespace chatterino
