// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Aliases.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

class QImage;
class QObject;
class QTimer;

namespace chatterino {

struct Emote;
using EmotePtr = std::shared_ptr<const Emote>;

/// ChattiFlexii's own badges: everyone with ChattiFlexii can upload one for
/// their Twitch name under Chatti-Badge, and once it is approved every
/// ChattiFlexii shows it next to that name. They live on a small server of
/// ChattiFlexii's own - see server/chatti-badges.
///
/// The list is fetched at startup and again every few minutes.
class ChattiBadges
{
public:
    ChattiBadges();
    ~ChattiBadges();

    ChattiBadges(const ChattiBadges &) = delete;
    ChattiBadges(ChattiBadges &&) = delete;
    ChattiBadges &operator=(const ChattiBadges &) = delete;
    ChattiBadges &operator=(ChattiBadges &&) = delete;

    /// The badge for the given Twitch user, if they have one
    std::optional<EmotePtr> getBadge(const UserId &id);

    /// Fetches the list again now - after an upload or an approval
    void reload();

private:
    std::shared_mutex mutex_;
    /// Maps Twitch user IDs to their badge. Guarded by mutex_
    std::unordered_map<QString, EmotePtr> badgeMap_;
    QTimer *timer_ = nullptr;
    pajlada::Signals::SignalHolder connections_;
};

/// Talking to the badge server, for the settings page
namespace chattibadges {

using Failed = std::function<void(const QString &)>;

/// The server's address, without a slash at the end - empty while none is
/// set
QString server();

/// What the server knows of the account logged in here
struct Me {
    QString userId;
    QString login;
    /// The pictures of the badge shown now and of the one waiting, if any
    QString approved;
    QString pending;
    bool isAdmin = false;
};

/// A badge waiting for an admin
struct Waiting {
    QString userId;
    QString login;
    QString image;
};

/// @a picture made what the server takes: a PNG of 72 × 72, the picture
/// fitted in and the rest left see-through
QByteArray badgePng(const QImage &picture);

void me(QObject *caller, std::function<void(const Me &)> done, Failed failed);
/// Uploads @a png for the account logged in here. It waits for an admin -
/// unless an admin uploads it.
void upload(const QByteArray &png, QObject *caller,
            std::function<void()> done, Failed failed);
/// Takes the badge of the account logged in here away, shown and waiting
void removeMine(QObject *caller, std::function<void()> done, Failed failed);

void waiting(QObject *caller,
             std::function<void(const std::vector<Waiting> &)> done,
             Failed failed);
void approve(const QString &userId, QObject *caller,
             std::function<void()> done, Failed failed);
void reject(const QString &userId, QObject *caller,
            std::function<void()> done, Failed failed);

}  // namespace chattibadges

}  // namespace chatterino
