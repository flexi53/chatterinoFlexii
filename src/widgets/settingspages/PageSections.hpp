// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <pajlada/settings/setting.hpp>
#include <pajlada/signals/signalholder.hpp>
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

/// A row to pick a sound with: the built-in ones, a file of your own, and a
/// button to hear it. An empty setting stands for the ping Chatterino plays
/// for highlights. @a holder keeps the row in step when the setting changes
/// elsewhere.
QWidget *soundChooser(QWidget *parent,
                      pajlada::Settings::Setting<QString> &setting,
                      pajlada::Signals::SignalHolder &holder);

/// Whether a search in the settings for @a query should show a page these
/// @a keywords describe - always for an empty search
bool matchesKeywords(const QString &query, const QStringList &keywords);

/// Whether anything written on @a page - a label, a checkbox, a button, a
/// tab, a list entry or a tooltip - contains @a query. So a page is found by
/// what it actually says, without keeping a list of words next to it.
bool matchesPageText(const QWidget *page, const QString &query);

}  // namespace chatterino::pagesections
