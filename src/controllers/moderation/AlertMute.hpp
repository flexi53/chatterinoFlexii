// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

/// Which channels the alert windows are silenced in. The bell in the input
/// bar switches this for the channel it stands in, the way the shield next
/// to it sets what that channel watches for. What the alerts watch for goes
/// on either way; only the windows stay away.
namespace chatterino::alertmute {

bool isMuted(const QString &channel);
void setMuted(const QString &channel, bool muted);

}  // namespace chatterino::alertmute
