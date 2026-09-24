// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/userdata/UserNotes.hpp"

#include "Application.hpp"
#include "controllers/userdata/UserDataController.hpp"

namespace chatterino::usernotes {

QString noteFor(const QString &userId)
{
    if (userId.isEmpty())
    {
        return {};
    }

    const auto *data = getApp()->getUserData();
    if (data == nullptr)
    {
        return {};
    }

    const auto user = data->getUser(userId);
    if (!user)
    {
        return {};
    }
    return user->notes;
}

QString oneLine(const QString &note, const int most)
{
    auto line = note.trimmed();
    const auto breakAt = line.indexOf(u'\n');
    bool cut = false;
    if (breakAt >= 0)
    {
        line = line.left(breakAt).trimmed();
        cut = true;
    }
    if (most > 0 && line.size() > most)
    {
        line = line.left(most).trimmed();
        cut = true;
    }
    if (cut)
    {
        line += QStringLiteral("…");
    }
    return line;
}

std::vector<Entry> all()
{
    std::vector<Entry> entries;

    const auto *data = getApp()->getUserData();
    if (data == nullptr)
    {
        return entries;
    }

    for (const auto &[userId, user] : data->getUsers())
    {
        if (user.notes.trimmed().isEmpty())
        {
            continue;
        }
        entries.push_back({userId, user.notes});
    }
    return entries;
}

}  // namespace chatterino::usernotes
