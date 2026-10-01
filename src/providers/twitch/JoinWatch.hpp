// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <chrono>
#include <map>

/// ChattiFlexii: Twitch answers a JOIN with a JOIN of its own, and where it
/// does not - too many at once, a connection just coming back - the channel
/// stays empty for good: no messages, nothing to see, and nothing says so.
/// This keeps track of what was asked for and what came back.
namespace chatterino::joinwatch {

/// A channel we asked to join and have not heard back about
struct Pending {
    QDateTime asked;
    int tries = 1;
};

/// The channels of @a pending that have waited longer than @a after without
/// an answer and may be asked again - a channel that was asked @a mostTries
/// times is given up on, so a name that no longer exists is not asked for
/// ever after. In the order they were asked for.
QStringList overdue(const std::map<QString, Pending> &pending,
                    const QDateTime &now, std::chrono::seconds after,
                    int mostTries);

}  // namespace chatterino::joinwatch
