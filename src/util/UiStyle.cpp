// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/UiStyle.hpp"

#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"

#include <QColor>

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

int headerSecondRow()
{
    if (!getSettings()->headerTwoRows)
    {
        return 0;
    }
    return compact() ? 15 : 18;
}

int headerTotalHeight()
{
    return headerHeight() + headerSecondRow();
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

QString quietButtonStyle()
{
    const auto *theme = getTheme();
    const auto light = theme->isLightTheme();
    const auto text = theme->messages.textColors.regular;
    const auto line = light ? QColor(0, 0, 0, 60) : QColor(255, 255, 255, 45);
    const auto hover = light ? QColor(0, 0, 0, 18) : QColor(255, 255, 255, 22);
    const auto pressed =
        light ? QColor(0, 0, 0, 34) : QColor(255, 255, 255, 40);

    const auto rgba = [](const QColor &color) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.alpha());
    };

    return QStringLiteral(
               "QPushButton { border: 1px solid %1; border-radius: 5px; "
               "padding: 3px 10px; background: transparent; color: %2; }"
               "QPushButton:hover { background: %3; }"
               "QPushButton:pressed { background: %4; }"
               "QPushButton:checked { background: %4; border-color: %2; }"
               "QPushButton:disabled { color: %1; }")
        .arg(rgba(line), text.name(), rgba(hover), rgba(pressed));
}

QString accentButtonStyle(const QColor &color)
{
    return QStringLiteral(
               "QPushButton { border: 2px solid %1; "
               "border-radius: 5px; padding: 3px 10px; "
               "background: rgba(%2, %3, %4, 45); color: %5; }"
               "QPushButton:hover { background: rgba(%2, %3, %4, 75); }"
               "QPushButton:pressed { background: rgba(%2, %3, %4, 110); }"
               "QPushButton:disabled { border-color: rgba(%2, %3, %4, 80); "
               "background: transparent; }")
        .arg(color.name())
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(getTheme()->messages.textColors.regular.name());
}

}  // namespace chatterino::uistyle
