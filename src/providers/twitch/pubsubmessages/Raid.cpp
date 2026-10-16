// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/pubsubmessages/Raid.hpp"

#include <QJsonValue>
#include <QTimeZone>

#include <algorithm>

namespace chatterino {

using namespace Qt::StringLiterals;

namespace {

QJsonValue firstOf(const QJsonObject &object,
                   std::initializer_list<QLatin1String> keys)
{
    for (const auto key : keys)
    {
        const auto value = object.value(key);
        if (!value.isUndefined() && !value.isNull())
        {
            return value;
        }
    }
    return {};
}

qint64 numberOf(const QJsonValue &value)
{
    if (value.isDouble())
    {
        return static_cast<qint64>(value.toDouble());
    }
    return value.toString().toLongLong();
}

PubSubRaid::Kind kindOf(const QString &type)
{
    const auto lower = type.toLower();
    if (lower.startsWith("raid_update"_L1))
    {
        return PubSubRaid::Kind::Update;
    }
    if (lower.startsWith("raid_go"_L1))
    {
        return PubSubRaid::Kind::Went;
    }
    if (lower.startsWith("raid_cancel"_L1))
    {
        return PubSubRaid::Kind::Canceled;
    }
    return PubSubRaid::Kind::Other;
}

}  // namespace

std::chrono::milliseconds PubSubRaid::remaining() const
{
    if (!this->goesAt.isValid())
    {
        return std::chrono::milliseconds{0};
    }
    return std::chrono::milliseconds{std::max<qint64>(
        0, QDateTime::currentDateTimeUtc().msecsTo(this->goesAt))};
}

std::optional<PubSubRaid> raidFrom(const QJsonObject &root)
{
    // The raid sits under raid, or under data.raid
    auto raid = root.value("raid"_L1).toObject();
    if (raid.isEmpty())
    {
        raid = root.value("data"_L1).toObject().value("raid"_L1).toObject();
    }
    if (raid.isEmpty())
    {
        return std::nullopt;
    }

    PubSubRaid out;
    out.kind = kindOf(root.value("type"_L1).toString());
    out.id = firstOf(raid, {"id"_L1}).toString();
    out.targetId = firstOf(raid, {"target_id"_L1, "targetID"_L1}).toString();
    out.targetLogin =
        firstOf(raid, {"target_login"_L1, "targetLogin"_L1}).toString();
    out.targetDisplayName =
        firstOf(raid, {"target_display_name"_L1, "targetDisplayName"_L1})
            .toString();
    if (out.targetDisplayName.isEmpty())
    {
        out.targetDisplayName = out.targetLogin;
    }
    out.viewerCount = static_cast<int>(std::max<qint64>(
        0, numberOf(firstOf(raid, {"viewer_count"_L1, "viewerCount"_L1}))));

    const auto wait = numberOf(firstOf(
        raid, {"force_raid_now_seconds"_L1, "forceRaidNowSeconds"_L1}));
    if (wait > 0)
    {
        out.waitSeconds = static_cast<int>(wait);
    }

    // It goes by itself once the wait is over, counted from when it was set
    // up - or from now, where the message does not say
    const auto created = numberOf(
        firstOf(raid, {"raid_created_at"_L1, "raidCreatedAt"_L1}));
    const auto start =
        created > 0
            ? QDateTime::fromSecsSinceEpoch(created, QTimeZone::UTC)
            : QDateTime::currentDateTimeUtc();
    out.goesAt = start.addSecs(out.waitSeconds);
    out.startKnown = created > 0;

    // That it went or was called off needs no more than which raid it was
    if (out.id.isEmpty() ||
        (out.kind == PubSubRaid::Kind::Update && out.targetLogin.isEmpty()))
    {
        return std::nullopt;
    }
    return out;
}

}  // namespace chatterino
