// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

#include <memory>

namespace chatterino {

/// Notizen -> Leute im Blick: the people you pick are kept with Chatterino's
/// own machinery - each of them becomes a quiet entry under Highlights that
/// only says "show in mentions", and the tab is a mentions split with a
/// filter of their names on it. So what lands there is what Chatterino
/// itself collects, and the filter is one you can read and change.
class WatchedPeople
{
public:
    /// The logins picked, as they are kept: one per line, lowercase, without
    /// a leading @
    static QStringList people();
    /// Reads a written list - line by line or separated by commas
    static QStringList read(const QString &written);
    /// Writes @a people back the way the setting keeps them
    static QString write(const QStringList &people);

    /// Whether @a login is one of @a people
    static bool watches(const QString &login, const QStringList &people);
    /// Whether @a login is watched right now
    static bool watches(const QString &login);

    /// Adds @a login to the list, or takes it out again. Gives what holds
    /// afterwards.
    static bool toggle(const QString &login);

    /// The filter that says the same as @a people, in the language of the
    /// filters page: (author.name == "a") || (author.name == "b")
    static QString expressionFor(const QStringList &people);

    /// What is wrong with @a expression, empty when it is fine. An empty
    /// expression is fine as well - then the list counts.
    static QString problemWith(const QString &expression);

    /// Brings Chatterino's own settings in line with the list: the filter
    /// the tab uses, and a quiet highlight per person so their messages
    /// reach the mentions channel at all. Called whenever something here
    /// changes; a start of its own is not needed.
    static void sync();

    /// Opens the tab - a mentions split with the filter on it - or shows the
    /// one that is there
    static void openTab();

    /// Keeps the settings in step from the moment the app is up
    static void start();
};

}  // namespace chatterino
