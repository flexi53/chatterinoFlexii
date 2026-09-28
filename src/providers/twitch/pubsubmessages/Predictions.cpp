// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/pubsubmessages/Predictions.hpp"

#include <QJsonArray>
#include <QJsonValue>

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

PubSubPrediction::Status statusOf(const QString &text)
{
    const auto upper = text.toUpper();
    if (upper == "ACTIVE"_L1)
    {
        return PubSubPrediction::Status::Active;
    }
    if (upper == "LOCKED"_L1 || upper == "RESOLVE_PENDING"_L1)
    {
        return PubSubPrediction::Status::Locked;
    }
    if (upper == "RESOLVED"_L1)
    {
        return PubSubPrediction::Status::Resolved;
    }
    if (upper == "CANCELED"_L1 || upper == "CANCELLED"_L1)
    {
        return PubSubPrediction::Status::Canceled;
    }
    return PubSubPrediction::Status::Invalid;
}

}  // namespace

QColor PubSubPrediction::Outcome::asColor() const
{
    // The two Twitch itself uses
    if (this->color.compare("PINK"_L1, Qt::CaseInsensitive) == 0)
    {
        return {0xf5, 0x00, 0x9b};
    }
    return {0x38, 0x7a, 0xff};
}

bool PubSubPrediction::running() const
{
    return this->status == Status::Active || this->status == Status::Locked;
}

std::chrono::milliseconds PubSubPrediction::remaining() const
{
    if (this->status != Status::Active || !this->locksAt.isValid())
    {
        return std::chrono::milliseconds(0);
    }
    const auto left = QDateTime::currentDateTimeUtc().msecsTo(this->locksAt);
    return std::chrono::milliseconds(std::max<qint64>(0, left));
}

qint64 PubSubPrediction::totalPoints() const
{
    qint64 total = 0;
    for (const auto &outcome : this->outcomes)
    {
        total += outcome.points;
    }
    return total;
}

double PubSubPrediction::payoutOf(size_t outcome) const
{
    if (outcome >= this->outcomes.size())
    {
        return 0.0;
    }
    const auto own = this->outcomes.at(outcome).points;
    if (own <= 0)
    {
        return 0.0;
    }
    return static_cast<double>(this->totalPoints()) /
           static_cast<double>(own);
}

std::optional<PubSubPrediction> predictionFrom(const QJsonObject &root)
{
    // The prediction sits under data.event
    auto event = root.value("data"_L1).toObject().value("event"_L1).toObject();
    if (event.isEmpty())
    {
        event = root.value("event"_L1).toObject();
    }
    if (event.isEmpty())
    {
        event = root;
    }

    const auto title = event.value("title"_L1).toString();
    const auto outcomes = event.value("outcomes"_L1).toArray();
    if (title.isEmpty() || outcomes.isEmpty())
    {
        return std::nullopt;
    }

    PubSubPrediction out;
    out.id = firstOf(event, {"id"_L1, "event_id"_L1}).toString();
    out.title = title;
    out.status = statusOf(event.value("status"_L1).toString());
    out.winningOutcomeId = event.value("winning_outcome_id"_L1).toString();

    for (const auto &value : outcomes)
    {
        const auto outcome = value.toObject();
        out.outcomes.push_back({
            .id = outcome.value("id"_L1).toString(),
            .title = outcome.value("title"_L1).toString(),
            .color = outcome.value("color"_L1).toString(),
            .points = static_cast<qint64>(
                firstOf(outcome, {"total_points"_L1, "points"_L1}).toDouble()),
            .users = static_cast<int>(
                firstOf(outcome, {"total_users"_L1, "users"_L1}).toDouble()),
        });
    }

    // When betting closes: what it says, or from when it began
    const auto locked = QDateTime::fromString(
        firstOf(event, {"locked_at"_L1, "prediction_window_ends_at"_L1})
            .toString(),
        Qt::ISODate);
    if (locked.isValid())
    {
        out.locksAt = locked.toUTC();
    }
    else
    {
        const auto created = QDateTime::fromString(
            firstOf(event, {"created_at"_L1, "started_at"_L1}).toString(),
            Qt::ISODate);
        const auto window = static_cast<qint64>(
            firstOf(event, {"prediction_window_seconds"_L1}).toDouble());
        if (created.isValid() && window > 0)
        {
            out.locksAt = created.toUTC().addSecs(window);
        }
    }

    return out;
}

}  // namespace chatterino
