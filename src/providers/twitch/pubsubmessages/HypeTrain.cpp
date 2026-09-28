// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/pubsubmessages/HypeTrain.hpp"

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

PubSubHypeTrain::Kind kindOf(const QString &type)
{
    const auto lower = type.toLower();
    if (lower == "hype-train-start"_L1)
    {
        return PubSubHypeTrain::Kind::Started;
    }
    if (lower == "hype-train-progression"_L1)
    {
        return PubSubHypeTrain::Kind::Progress;
    }
    if (lower == "hype-train-level-up"_L1)
    {
        return PubSubHypeTrain::Kind::LevelUp;
    }
    if (lower == "hype-train-end"_L1)
    {
        return PubSubHypeTrain::Kind::Ended;
    }
    return PubSubHypeTrain::Kind::Other;
}

/// A time the topic writes either as a date or as milliseconds since the
/// epoch
QDateTime timeOf(const QJsonValue &value)
{
    if (value.isString())
    {
        return QDateTime::fromString(value.toString(), Qt::ISODate).toUTC();
    }
    if (value.isDouble())
    {
        auto number = static_cast<qint64>(value.toDouble());
        if (number <= 0)
        {
            return {};
        }
        constexpr qint64 SECONDS_UNTIL_THE_YEAR_2300 = 10'000'000'000LL;
        if (number < SECONDS_UNTIL_THE_YEAR_2300)
        {
            number *= 1000;
        }
        return QDateTime::fromMSecsSinceEpoch(number, QTimeZone::UTC);
    }
    return {};
}

}  // namespace

double PubSubHypeTrain::share() const
{
    if (this->goal <= 0)
    {
        return 0.0;
    }
    return std::clamp(static_cast<double>(this->value) /
                          static_cast<double>(this->goal),
                      0.0, 1.0);
}

std::chrono::milliseconds PubSubHypeTrain::remaining() const
{
    if (this->over || !this->expiresAt.isValid())
    {
        return std::chrono::milliseconds(0);
    }
    const auto left = QDateTime::currentDateTimeUtc().msecsTo(this->expiresAt);
    return std::chrono::milliseconds(std::max<qint64>(0, left));
}

std::optional<PubSubHypeTrain> hypeTrainFrom(const QJsonObject &root)
{
    const auto data = root.value("data"_L1).toObject();

    PubSubHypeTrain out;
    out.kind = kindOf(root.value("type"_L1).toString());
    if (out.kind == PubSubHypeTrain::Kind::Other)
    {
        return std::nullopt;
    }

    // Where it stands sits under "progress", or under the train itself when
    // one just started
    auto progress = data.value("progress"_L1).toObject();
    const auto train = data.value("event"_L1).isObject()
                           ? data.value("event"_L1).toObject()
                           : data;
    if (progress.isEmpty())
    {
        progress = train.value("progress"_L1).toObject();
    }

    out.id = firstOf(train, {"id"_L1, "hype_train_id"_L1}).toString();
    if (out.id.isEmpty())
    {
        out.id = firstOf(data, {"id"_L1, "hype_train_id"_L1}).toString();
    }

    if (out.kind == PubSubHypeTrain::Kind::Ended)
    {
        out.over = true;
        const auto reason = firstOf(data, {"ending_reason"_L1, "reason"_L1})
                                .toString()
                                .toUpper();
        out.completed = reason == "COMPLETED"_L1;
        return out;
    }

    if (progress.isEmpty())
    {
        // Nothing about where it stands - a conductor change, say
        return out;
    }

    out.hasProgress = true;
    const auto level = progress.value("level"_L1).toObject();
    out.level = std::max(1, static_cast<int>(
                                firstOf(level, {"value"_L1, "level"_L1})
                                    .toDouble()));
    out.value = static_cast<qint64>(
        firstOf(progress, {"value"_L1, "total"_L1}).toDouble());
    out.goal = static_cast<qint64>(
        firstOf(progress, {"goal"_L1}).isDouble()
            ? progress.value("goal"_L1).toDouble()
            : level.value("goal"_L1).toDouble());

    // When it runs out: said outright, or as the seconds it has left
    out.expiresAt = timeOf(firstOf(train, {"expires_at"_L1}));
    if (!out.expiresAt.isValid())
    {
        out.expiresAt = timeOf(firstOf(data, {"expires_at"_L1}));
    }
    if (!out.expiresAt.isValid())
    {
        const auto seconds = static_cast<qint64>(
            firstOf(progress, {"remaining_seconds"_L1}).toDouble());
        const auto millis = static_cast<qint64>(
            firstOf(data, {"time_to_expire"_L1}).toDouble());
        if (seconds > 0)
        {
            out.expiresAt = QDateTime::currentDateTimeUtc().addSecs(seconds);
        }
        else if (millis > 0)
        {
            out.expiresAt = timeOf(QJsonValue(static_cast<double>(millis)));
        }
    }

    return out;
}

}  // namespace chatterino
