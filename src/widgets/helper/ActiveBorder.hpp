// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>

/// What the split you type in and the tab you are on are outlined in - see
/// Look -> Tabs. Both ask here, so the two borders keep the same colour
/// unless one is given its own.
namespace chatterino::activeborder {

/// The red both fall back to when nothing is set
QColor fallback();

/// The border around the split being typed in
QColor forSplit();

/// The border around the tab you are on: its own colour where one is set,
/// otherwise the split's
QColor forTab();

}  // namespace chatterino::activeborder
