// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "messages/BadgeOrder.hpp"

#include "singletons/Settings.hpp"
#include "util/SavedOrder.hpp"

namespace chatterino::badgeorder {

QStringList defaults()
{
    return {FROM_TWITCH,  FROM_CHATTERINO, FROM_FFZ,       FROM_BTTV,
            FROM_SEVENTV, FROM_HOMIES,     FROM_MOLTORINO, FROM_SONKERINO,
            FROM_CHATTI,  CHATTI_STATUS};
}

QStringList order()
{
    const auto saved =
        getSettings()->badgeOrder.getValue().split(u';', Qt::SkipEmptyParts);
    return orderAsSaved(defaults(), saved);
}

void setOrder(const QStringList &keys)
{
    getSettings()->badgeOrder.setValue(
        orderAsSaved(defaults(), keys).join(u';'));
}

QString nameOf(const QString &key)
{
    if (key == FROM_TWITCH)
    {
        return QStringLiteral("Twitch (Rolle, Abo, Prime, Bits …)");
    }
    if (key == FROM_CHATTERINO)
    {
        return QStringLiteral("Chatterino");
    }
    if (key == FROM_FFZ)
    {
        return QStringLiteral("FrankerFaceZ");
    }
    if (key == FROM_BTTV)
    {
        return QStringLiteral("BetterTTV");
    }
    if (key == FROM_SEVENTV)
    {
        return QStringLiteral("7TV");
    }
    if (key == FROM_HOMIES)
    {
        return QStringLiteral("Chatterino Homies");
    }
    if (key == FROM_MOLTORINO)
    {
        return QStringLiteral("Moltorino");
    }
    if (key == FROM_SONKERINO)
    {
        return QStringLiteral("sonkerino");
    }
    if (key == FROM_CHATTI)
    {
        return QStringLiteral("Chatti-Badge (hochgeladen)");
    }
    if (key == CHATTI_STATUS)
    {
        return QStringLiteral("ChattiFlexii-Nutzer / -Supporter");
    }
    return key;
}

bool move(const QString &key, const bool up)
{
    auto keys = order();
    const auto at = keys.indexOf(key);
    if (at < 0)
    {
        return false;
    }
    const auto to = up ? at - 1 : at + 1;
    if (to < 0 || to >= keys.size())
    {
        return false;
    }

    keys.move(at, to);
    setOrder(keys);
    return true;
}

}  // namespace chatterino::badgeorder
