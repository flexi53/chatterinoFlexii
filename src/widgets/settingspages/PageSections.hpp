// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QStringList>

class QLabel;
class QString;
class QTabWidget;
class QVBoxLayout;
class QWidget;

/// Building blocks for settings pages laid out in tabs, as the Mod-Assistent
/// and Mod-Highlights pages are
namespace chatterino::pagesections {

/// A tab of @a tabs that scrolls on its own, so a long one never squeezes its
/// rows together. Gives the layout to fill.
QVBoxLayout *addPageTab(QTabWidget *tabs, const QString &title);

/// A bold heading, set a little apart from what comes before it
void addHeading(QVBoxLayout *layout, const QString &text);

/// A wrapped paragraph, greyed out with @a dimmed for side notes
QLabel *addText(QVBoxLayout *layout, const QString &text, bool dimmed = false);

/// @a widget on a row of its own, kept to its natural width
void addButtonRow(QVBoxLayout *layout, QWidget *widget);

/// Whether a search in the settings for @a query should show a page these
/// @a keywords describe - always for an empty search
bool matchesKeywords(const QString &query, const QStringList &keywords);

}  // namespace chatterino::pagesections
