// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QSize>

/// Look -> Style: what the chosen look changes, asked in one place. Classic
/// is Chatterino as it always was, and every answer here falls back to it.
namespace chatterino::uistyle {

/// Rounded, with a little depth
bool modern();
/// Classic's shapes with less room around everything, for many chats at once
bool compact();
/// No boxes - a line and a colour say what is what
bool flat();

/// Modern and Flat draw the settings icons of ChattiFlexii's own set and let
/// the alert windows fade in; Classic and Compact keep Chatterino's
bool drawnIcons();

/// How high a tab is, in unscaled pixels
int tabHeight();
/// How high the split header is, and how wide its buttons, unscaled
int headerHeight();
/// ChattiFlexii: how high the second line of the split header is, unscaled -
/// 0 while the title bar stands in one line
int headerSecondRow();
/// Both lines of the split header together, unscaled
int headerTotalHeight();
/// How wide the scrollbar is, unscaled
int scrollbarWidth();
/// The small buttons beside the input, unscaled
QSize inputButton();
/// The room inside an alert window, unscaled
int alertMargin();

}  // namespace chatterino::uistyle
