// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/UiStyle.hpp"

#include "singletons/Settings.hpp"

namespace chatterino::uistyle {

namespace {

UiStyle current()
{
    return getSettings()->uiStyle.getEnum();
}

}  // namespace

bool modern()
{
    return current() == UiStyle::Modern;
}

bool compact()
{
    return current() == UiStyle::Compact;
}

bool flat()
{
    return current() == UiStyle::Flat;
}

bool drawnIcons()
{
    const auto style = current();
    return style == UiStyle::Modern || style == UiStyle::Flat;
}

int tabHeight()
{
    return compact() ? 22 : 28;
}

int headerHeight()
{
    return compact() ? 22 : 28;
}

int scrollbarWidth()
{
    return compact() ? 10 : 16;
}

QSize inputButton()
{
    return compact() ? QSize(20, 14) : QSize(24, 18);
}

int alertMargin()
{
    return compact() ? 7 : 12;
}

}  // namespace chatterino::uistyle
