// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>

namespace chatterino {

class Channel;

/// Shows @a channel in a tab of its own in the main window - the one it is
/// in already when there is one. For the channels ChattiFlexii fills itself,
/// "Neue Badges" and "Mod-Änderungen", which need no Twitch channel asked for.
void openOwnTab(const std::shared_ptr<Channel> &channel);

}  // namespace chatterino
