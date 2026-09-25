// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/MentionFlash.hpp"

#include <cmath>

namespace chatterino::mentionflash {

double shapeAt(double progress)
{
    constexpr double PI = 3.14159265358979323846;

    const auto wave = std::abs(std::sin(progress * FLASHES * PI));
    return 1.0 - (wave * (1.0 - (progress * 0.35)));
}

bool namedIn(const QString &text, const QString &name)
{
    if (name.isEmpty() || text.isEmpty())
    {
        return false;
    }
    return text.contains(name, Qt::CaseInsensitive);
}

}  // namespace chatterino::mentionflash
