// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

/// The logo the program runs under: in the Dock, in the window switcher and
/// on its windows. The same shape in five colours - picked under Aussehen ->
/// Stil. The icon of the program file itself stays as it is; it belongs to
/// the signed bundle and is nothing to meddle with while it runs.
namespace chatterino::appicon {

/// Every colour there is, the one it starts with first
QStringList keys();

/// What the colour @a key is called
QString nameOf(const QString &key);

/// Where the logo of @a key lies - the one that fills the whole square,
/// which is what the program file carries
QString pathOf(const QString &key);

/// Where the logo lies in the form that is shown while the program runs:
/// free-standing when that was asked for, otherwise the same as pathOf()
QString shownPathOf(const QString &key);

/// Whether the logo is shown without a background behind it while the program
/// runs
bool bare();

/// The colour picked, or the one it starts with when none holds
QString picked();

/// Hands the picked logo to the program - called at the start and whenever
/// the choice changes
void apply();

}  // namespace chatterino::appicon
