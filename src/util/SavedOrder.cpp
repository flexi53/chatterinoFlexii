// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/SavedOrder.hpp"

namespace chatterino {

QStringList orderAsSaved(const QStringList &defaults, const QStringList &saved)
{
    QStringList order;
    for (const auto &key : saved)
    {
        if (defaults.contains(key) && !order.contains(key))
        {
            order.append(key);
        }
    }

    for (qsizetype i = 0; i < defaults.size(); i++)
    {
        const auto &key = defaults.at(i);
        if (order.contains(key))
        {
            continue;
        }
        qsizetype at = 0;
        for (auto before = i; before > 0; before--)
        {
            const auto found = order.indexOf(defaults.at(before - 1));
            if (found >= 0)
            {
                at = found + 1;
                break;
            }
        }
        order.insert(at, key);
    }
    return order;
}

}  // namespace chatterino
