// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

#include <memory>

namespace chatterino {

class Channel;

/// Shows @a channel in a tab of its own in the main window - the one it is
/// in already when there is one. For the channels ChattiFlexii fills itself,
/// "Neue Badges" and "Mod-Änderungen", which need no Twitch channel asked for.
void openOwnTab(const std::shared_ptr<Channel> &channel);

/// Brings the tab of the Twitch channel @a name to the front, without
/// pulling the window forward - for following along with the browser. Opens
/// the channel in a tab of its own when @a openWhenMissing and it is nowhere
/// to be seen; with @a closeOpened such a tab goes again as soon as the
/// browser moves on, while a tab that was already there stays. Says whether
/// a tab was found or opened.
bool showChannelTab(const QString &name, bool openWhenMissing,
                    bool closeOpened = false);

}  // namespace chatterino
