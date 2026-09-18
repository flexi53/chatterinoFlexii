// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "messages/layouts/AlternateBackground.hpp"

#include "messages/layouts/MessageLayout.hpp"
#include "messages/Message.hpp"

#include <algorithm>

namespace chatterino::alternatebg {

QColor background(const QColor &regular, const QColor &themeAlternate,
                  int strength, const QColor &tint)
{
    if (strength <= 0)
    {
        if (!tint.isValid())
        {
            return themeAlternate;
        }
        strength = TINT_STRENGTH;
    }

    QColor towards = tint;
    if (!towards.isValid())
    {
        towards =
            regular.lightnessF() < 0.5 ? QColor(Qt::white) : QColor(Qt::black);
    }

    const double amount = std::clamp(strength, 0, 100) / 100.0;
    const auto mix = [amount](int from, int to) {
        return int(from + ((to - from) * amount) + 0.5);
    };
    return {mix(regular.red(), towards.red()),
            mix(regular.green(), towards.green()),
            mix(regular.blue(), towards.blue()), regular.alpha()};
}

bool alternateNextTo(MessageLayout *neighbour, const Message &message,
                     bool bySender)
{
    if (neighbour == nullptr)
    {
        return false;
    }

    const bool neighbourAlternate =
        neighbour->flags.has(MessageLayoutFlag::AlternateBackground);
    if (bySender && neighbour->getMessage()->loginName == message.loginName)
    {
        return neighbourAlternate;
    }
    return !neighbourAlternate;
}

}  // namespace chatterino::alternatebg
