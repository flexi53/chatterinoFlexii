// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/HiddenUsers.hpp"

#include "singletons/Settings.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace chatterino::hiddenusers {

QStringList read(const QString &text)
{
    QStringList names;
    for (const auto &piece : text.split(QRegularExpression("[,\\s]+"),
                                        Qt::SkipEmptyParts))
    {
        auto name = piece.trimmed().toLower();
        while (name.startsWith('@'))
        {
            name = name.mid(1);
        }
        if (!name.isEmpty() && !names.contains(name))
        {
            names.append(name);
        }
    }
    return names;
}

QStringList all()
{
    return read(getSettings()->hiddenUsers.getValue());
}

bool hides(const QString &login)
{
    if (login.isEmpty())
    {
        return false;
    }
    return all().contains(login.toLower());
}

bool add(const QString &login)
{
    auto names = all();
    const auto name = login.trimmed().toLower();
    if (name.isEmpty() || names.contains(name))
    {
        return false;
    }
    names.append(name);
    names.sort();
    getSettings()->hiddenUsers.setValue(names.join(','));
    return true;
}

bool remove(const QString &login)
{
    auto names = all();
    const auto name = login.trimmed().toLower();
    if (!names.removeOne(name))
    {
        return false;
    }
    getSettings()->hiddenUsers.setValue(names.join(','));
    return true;
}

}  // namespace chatterino::hiddenusers
