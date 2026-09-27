// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/activity/ActivityMarks.hpp"

#include <QHash>

#include <deque>

namespace chatterino::activitymarks {

namespace {

/// Made once and never taken down - see ProfilePictures for why that
/// matters at the end of the program
QHash<QString, std::deque<Mark>> &store()
{
    static auto *marks = new QHash<QString, std::deque<Mark>>();
    return *marks;
}

}  // namespace

void note(const QString &channel, Kind kind, const QString &who,
          QDateTime when)
{
    if (channel.isEmpty() || !when.isValid())
    {
        return;
    }

    auto &kept = store()[channel.toLower()];
    kept.push_back({.when = when, .kind = kind, .who = who});
    while (kept.size() > MOST_PER_CHANNEL)
    {
        kept.pop_front();
    }
}

std::vector<Mark> marks(const QString &channel, const QDateTime &from,
                        const QDateTime &to)
{
    std::vector<Mark> found;
    const auto kept = store().find(channel.toLower());
    if (kept == store().end())
    {
        return found;
    }

    for (const auto &mark : *kept)
    {
        if (mark.when >= from && mark.when <= to)
        {
            found.push_back(mark);
        }
    }
    return found;
}

QString nameOf(Kind kind)
{
    switch (kind)
    {
        case Kind::Mention:
            return QStringLiteral("Erwähnung");
        case Kind::Alert:
            return QStringLiteral("Alarm");
        case Kind::ModAction:
            return QStringLiteral("Moderiert");
    }
    return {};
}

void forget()
{
    store().clear();
}

}  // namespace chatterino::activitymarks
