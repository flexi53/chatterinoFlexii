// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/InputButtons.hpp"

#include "singletons/Settings.hpp"
#include "util/SavedOrder.hpp"

namespace chatterino::inputbuttons {

QStringList defaults()
{
    return {MOD_ASSIST, ALERT_MUTE, CLEAR, FOCUS, FOLLOW, BADGE, CLIP, EMOTE};
}

QStringList order()
{
    const auto saved = getSettings()->inputButtonOrder.getValue().split(
        u';', Qt::SkipEmptyParts);
    return orderAsSaved(defaults(), saved);
}

void setOrder(const QStringList &keys)
{
    getSettings()->inputButtonOrder.setValue(
        orderAsSaved(defaults(), keys).join(u';'));
}

QString nameOf(const QString &key)
{
    if (key == MOD_ASSIST)
    {
        return QStringLiteral("Mod-Assistent (Schild)");
    }
    if (key == ALERT_MUTE)
    {
        return QStringLiteral("Alarme stumm (Glocke)");
    }
    if (key == CLEAR)
    {
        return QStringLiteral("Chat leeren");
    }
    if (key == FOCUS)
    {
        return QStringLiteral("Fokus-Ansicht");
    }
    if (key == FOLLOW)
    {
        return QStringLiteral("Dem Browser folgen (Kette)");
    }
    if (key == BADGE)
    {
        return QStringLiteral("Badge wechseln");
    }
    if (key == CLIP)
    {
        return QStringLiteral("Clip erstellen");
    }
    if (key == EMOTE)
    {
        return QStringLiteral("Emotes");
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

}  // namespace chatterino::inputbuttons
