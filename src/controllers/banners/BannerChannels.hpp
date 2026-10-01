// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

/// ChattiFlexii: which channels a banner is left out of. Twitch tells every
/// viewer about a hype train, and in a channel where one rides every other
/// minute the banner is in the way rather than news. Set per channel in the
/// split's menu, kept in the settings.
namespace chatterino::banners {

/// Whether the hype train banner is wanted in @a channel - true unless it
/// was switched off there
bool hypeShownIn(const QString &channel);
void setHypeShownIn(const QString &channel, bool shown);

}  // namespace chatterino::banners
