// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/ActiveBorder.hpp"

#include "singletons/Settings.hpp"

namespace chatterino::activeborder {

QColor fallback()
{
    return {0xe9, 0x19, 0x16};
}

QColor forSplit()
{
    const QColor picked(getSettings()->activeSplitBorderColor.getValue());
    return picked.isValid() ? picked : fallback();
}

QColor forTab()
{
    const QColor own(getSettings()->activeTabBorderColor.getValue());
    return own.isValid() ? own : forSplit();
}

}  // namespace chatterino::activeborder
