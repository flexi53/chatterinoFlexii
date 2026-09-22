// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/badgebase/BadgeBase.hpp"

#include "common/Credentials.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"

#include <QJsonArray>
#include <QRegularExpression>

namespace chatterino::badgebase {

namespace {

const QString PROVIDER = QStringLiteral("badgebase");
const QString NAME = QStringLiteral("key");
const QString BASE = QStringLiteral("https://badgebase.de/api/v1");

QString textOf(const QJsonObject &object, const char *first,
               const char *second = nullptr)
{
    auto value = object.value(first);
    if ((value.isUndefined() || value.isNull()) && second != nullptr)
    {
        value = object.value(second);
    }
    if (value.isDouble())
    {
        return QString::number(value.toInteger());
    }
    return value.toString();
}

QDateTime timeOf(const QJsonObject &object, const char *first,
                 const char *second)
{
    auto value = object.value(first);
    if (value.isUndefined() || value.isNull())
    {
        value = object.value(second);
    }
    if (value.isDouble())
    {
        return QDateTime::fromSecsSinceEpoch(value.toInteger()).toUTC();
    }
    auto time = QDateTime::fromString(value.toString(), Qt::ISODate);
    if (!time.isValid())
    {
        time = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    }
    return time;
}

}  // namespace

Badge normalize(const QJsonObject &object)
{
    Badge badge;
    badge.id = textOf(object, "id");
    badge.setId = textOf(object, "set_id", "setId");
    badge.title = textOf(object, "title", "name");
    badge.url = textOf(object, "url");
    badge.image = textOf(object, "image", "image_url");
    badge.description = textOf(object, "description");
    badge.start = timeOf(object, "start", "startDate");
    badge.end = timeOf(object, "end", "endDate");

    const auto price = object.value("price");
    badge.paid = price.isString() ? price.toString() == QStringLiteral("paid")
                                  : object.value("paid").toBool();

    auto holders = object.value("holders");
    if (!holders.isDouble())
    {
        holders = object.value("collectors");
    }
    badge.holders = holders.isDouble() ? holders.toInt() : -1;
    return badge;
}

std::vector<Badge> badgesIn(const QJsonObject &answer)
{
    std::vector<Badge> badges;
    for (const auto &entry : answer.value("data").toArray())
    {
        const auto badge = normalize(entry.toObject());
        if (!badge.id.isEmpty() || !badge.title.isEmpty())
        {
            badges.push_back(badge);
        }
    }
    return badges;
}

bool canStore()
{
    return Credentials::isSecure();
}

bool looksLikeKey(const QString &key)
{
    static const QRegularExpression shape(
        QStringLiteral(R"(^[A-Za-z0-9_\-\.]{16,200}$)"));
    return shape.match(key).hasMatch();
}

bool storeKey(const QString &key)
{
    if (!canStore() || !looksLikeKey(key))
    {
        return false;
    }
    Credentials::instance().set(PROVIDER, NAME, key);
    return true;
}

void eraseKey()
{
    Credentials::instance().erase(PROVIDER, NAME);
}

void loadKey(QObject *receiver, std::function<void(const QString &)> done)
{
    if (!canStore())
    {
        done({});
        return;
    }
    Credentials::instance().get(PROVIDER, NAME, receiver, std::move(done));
}

void get(const QString &key, const QString &path, QObject *caller,
         std::function<void(const QJsonObject &)> done,
         std::function<void(const QString &)> failed)
{
    // The key goes in a header, never in the address - that would end up in
    // every log on the way
    NetworkRequest(QUrl(BASE + path), NetworkRequestType::Get)
        .header("Authorization", QStringLiteral("Bearer ") + key)
        .header("Accept", "application/json")
        .timeout(20000)
        .caller(caller)
        .onSuccess([done](const NetworkResult &result) {
            done(result.parseJson());
        })
        .onError([failed](const NetworkResult &result) {
            const auto status = result.status().value_or(0);
            if (status == 401)
            {
                failed(QStringLiteral("BadgeBase nimmt den Schlüssel nicht "
                                      "an."));
            }
            else if (status == 429)
            {
                failed(QStringLiteral("BadgeBase will gerade nicht so oft "
                                      "gefragt werden - gleich wieder."));
            }
            else
            {
                failed(QStringLiteral("BadgeBase war nicht zu erreichen: ") +
                       result.formatError());
            }
        })
        .execute();
}

}  // namespace chatterino::badgebase
