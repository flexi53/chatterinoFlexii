// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/pubsubmessages/Polls.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace chatterino {

using namespace Qt::StringLiterals;

namespace {

/// The first of @a keys @a object has something under
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

/// A count that is written either as a number or as an object with the
/// pieces it is made of
int countOf(const QJsonValue &value)
{
    if (value.isDouble())
    {
        return static_cast<int>(value.toDouble());
    }
    if (value.isObject())
    {
        return static_cast<int>(value.toObject().value("total"_L1).toDouble());
    }
    return 0;
}

PubSubPoll::Status statusOf(const QString &text)
{
    const auto upper = text.toUpper();
    if (upper == "ACTIVE"_L1)
    {
        return PubSubPoll::Status::Active;
    }
    if (upper == "COMPLETED"_L1)
    {
        return PubSubPoll::Status::Completed;
    }
    if (upper == "TERMINATED"_L1 || upper == "MODERATED"_L1)
    {
        return PubSubPoll::Status::Terminated;
    }
    if (upper == "ARCHIVED"_L1)
    {
        return PubSubPoll::Status::Archived;
    }
    return PubSubPoll::Status::Invalid;
}

/// How long the poll still has, from whichever of the two ways it says so
std::chrono::milliseconds remainingOf(const QJsonObject &poll)
{
    const auto written =
        firstOf(poll, {"remaining_duration_milliseconds"_L1,
                       "remaining_duration"_L1, "duration_milliseconds"_L1});
    if (written.isDouble())
    {
        return std::chrono::milliseconds(
            std::max<qint64>(0, static_cast<qint64>(written.toDouble())));
    }

    // Otherwise from when it started and how long it runs
    const auto started = QDateTime::fromString(
        firstOf(poll, {"started_at"_L1, "created_at"_L1}).toString(),
        Qt::ISODate);
    const auto seconds = static_cast<qint64>(
        firstOf(poll, {"duration_seconds"_L1}).toDouble());
    if (!started.isValid() || seconds <= 0)
    {
        return std::chrono::milliseconds(0);
    }

    const auto left = QDateTime::currentDateTimeUtc().msecsTo(
        started.toUTC().addSecs(seconds));
    return std::chrono::milliseconds(std::max<qint64>(0, left));
}

}  // namespace

bool PubSubPoll::running() const
{
    return this->status == Status::Active;
}

std::chrono::milliseconds PubSubPoll::remaining() const
{
    if (!this->running() || !this->endsAt.isValid())
    {
        return std::chrono::milliseconds(0);
    }
    const auto left = QDateTime::currentDateTimeUtc().msecsTo(this->endsAt);
    return std::chrono::milliseconds(std::max<qint64>(0, left));
}

std::optional<size_t> PubSubPoll::leader() const
{
    if (this->totalVotes <= 0 || this->choices.empty())
    {
        return std::nullopt;
    }

    const auto best = std::max_element(this->choices.begin(),
                                       this->choices.end(),
                                       [](const auto &a, const auto &b) {
                                           return a.votes < b.votes;
                                       });
    const auto ties = std::count_if(this->choices.begin(), this->choices.end(),
                                    [&best](const auto &c) {
                                        return c.votes == best->votes;
                                    });
    if (ties > 1)
    {
        return std::nullopt;
    }
    return static_cast<size_t>(std::distance(this->choices.begin(), best));
}

std::optional<PubSubPoll> pollFrom(const QJsonObject &root)
{
    // The poll sits under data.poll; where it does not, the message may be
    // the poll itself
    auto poll = root.value("data"_L1).toObject().value("poll"_L1).toObject();
    if (poll.isEmpty())
    {
        poll = root.value("poll"_L1).toObject();
    }
    if (poll.isEmpty())
    {
        poll = root;
    }

    const auto title = poll.value("title"_L1).toString();
    const auto choices = poll.value("choices"_L1).toArray();
    if (title.isEmpty() || choices.isEmpty())
    {
        return std::nullopt;
    }

    PubSubPoll out;
    out.id = firstOf(poll, {"poll_id"_L1, "id"_L1}).toString();
    out.title = title;
    out.status = statusOf(poll.value("status"_L1).toString());
    out.endsAt = QDateTime::currentDateTimeUtc().addMSecs(
        remainingOf(poll).count());

    for (const auto &value : choices)
    {
        const auto choice = value.toObject();
        out.choices.push_back({
            .title = choice.value("title"_L1).toString(),
            .votes = countOf(firstOf(choice, {"votes"_L1, "total_voters"_L1})),
        });
    }

    out.totalVotes =
        countOf(firstOf(poll, {"votes"_L1, "total_votes"_L1, "total_voters"_L1}));
    if (out.totalVotes <= 0)
    {
        // Add them up ourselves where the poll does not say
        for (const auto &choice : out.choices)
        {
            out.totalVotes += choice.votes;
        }
    }

    const auto points = poll.value("settings"_L1)
                            .toObject()
                            .value("channel_points_votes"_L1)
                            .toObject();
    if (points.value("is_enabled"_L1).toBool())
    {
        out.pointsPerVote = static_cast<int>(points.value("cost"_L1).toDouble());
    }

    return out;
}

}  // namespace chatterino
