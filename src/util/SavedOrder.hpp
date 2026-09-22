// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QStringList>

namespace chatterino {

/// @a defaults in the order @a saved has them. What @a saved does not know
/// goes behind the one it follows by default - or first when it is the
/// first - and what it names that is not among @a defaults is left out.
QStringList orderAsSaved(const QStringList &defaults, const QStringList &saved);

}  // namespace chatterino
