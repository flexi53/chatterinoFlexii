// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Aliases.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QByteArray>
#include <QString>

#include <chrono>
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
/// Besides, everyone who uses ChattiFlexii wears its user badge - the
/// program tells the server so once a day, unless switched off - and those
/// an admin makes supporters wear the supporter badge instead.
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

    /// The user or the supporter badge of the given Twitch user, if they
    /// wear one
    std::optional<EmotePtr> getStatusBadge(const UserId &id);

    /// Fetches the list again now - after an upload or an approval
    void reload();

    /// Tells the server whether this account uses ChattiFlexii, as
    /// Chatti-Badge says - at most once a day unless @a now
    void sayHello(bool now = false);

private:
    std::shared_mutex mutex_;
    /// Maps Twitch user IDs to their badge. Guarded by mutex_
    std::unordered_map<QString, EmotePtr> badgeMap_;
    /// Maps Twitch user IDs to the user or the supporter badge. Guarded by
    /// mutex_
    std::unordered_map<QString, EmotePtr> statusMap_;
    /// Who was last said hello for, and when - so a day of reloads asks
    /// the server once
    QString helloLogin_;
    std::chrono::steady_clock::time_point helloAt_;
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
    bool supporter = false;
};

/// Someone an admin made a supporter
struct Supporter {
    QString userId;
    QString login;
};

/// A badge waiting for an admin
struct Waiting {
    QString userId;
    QString login;
    QString image;
    /// The program it was uploaded with
    QString client;
};

/// @a picture made what the server takes: a PNG of 72 × 72, the picture
/// fitted in and the rest left see-through
QByteArray badgePng(const QImage &picture);

void me(QObject *caller, std::function<void(const Me &)> done, Failed failed);
/// The most an animated GIF may be, as the server takes it - it is sent as
/// it is, since making it smaller here would stop it moving
constexpr int MAX_GIF_SIDE = 240;
constexpr qsizetype MAX_GIF_BYTES = 1024 * 1024;

/// Uploads @a picture for the account logged in here - a PNG from
/// badgePng(), or an @a animated GIF as it is. It waits for an admin -
/// unless an admin uploads it.
void upload(const QByteArray &picture, bool animated, QObject *caller,
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

void supporters(QObject *caller,
                std::function<void(const std::vector<Supporter> &)> done,
                Failed failed);
/// Makes the Twitch user @a login a supporter
void addSupporter(const QString &login, QObject *caller,
                  std::function<void()> done, Failed failed);
void removeSupporter(const QString &userId, QObject *caller,
                     std::function<void()> done, Failed failed);

}  // namespace chattibadges

}  // namespace chatterino
