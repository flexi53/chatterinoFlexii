// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <functional>
#include <vector>

class QObject;

/// BadgeBase (badgebase.de) knows every Twitch badge there is to get: when it
/// can be had, how, and who is still missing it. Each user asks it with a key
/// of their own, kept in the system's keychain - never in the settings, an
/// export, a backup or the code.
namespace chatterino::badgebase {

struct Badge {
    QString id;
    QString setId;
    QString title;
    /// Its page on badgebase.de
    QString url;
    QString image;
    bool paid = false;
    /// Invalid while not known
    QDateTime start;
    QDateTime end;
    /// How many have it, -1 while not known
    int holders = -1;
    QString description;
};

/// One badge as BadgeBase sends it. What it sends differs from what its own
/// description says - "image", "start", "end", "holders" and "price" instead
/// of "image_url", "startDate", "endDate", "collectors" and "paid" - so both
/// are read.
Badge normalize(const QJsonObject &object);
/// The badges in an answer - its "data", or the answer itself when that is
/// the list
std::vector<Badge> badgesIn(const QJsonObject &answer);

/// Whether a key can be kept here - only where the system has a keychain
bool canStore();
/// Whether @a key looks like one of BadgeBase's
bool looksLikeKey(const QString &key);
/// Keeps @a key in the keychain; false when it does not look like one
bool storeKey(const QString &key);
void eraseKey();
/// Hands the kept key to @a done - an empty one when there is none
void loadKey(QObject *receiver, std::function<void(const QString &)> done);

/// Asks BadgeBase for @a path - "/badges?status=claimable" and the like.
/// @a done gets the answer, @a failed what went wrong, in words.
void get(const QString &key, const QString &path, QObject *caller,
         std::function<void(const QJsonObject &)> done,
         std::function<void(const QString &)> failed);

}  // namespace chatterino::badgebase
