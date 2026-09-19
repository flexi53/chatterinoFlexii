// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>
#include <QStringList>

class QWidget;

/// The German texts for Chatterino's own windows. Chatterino writes its
/// labels in English, right in the code; translating them there would make
/// every merge with a new Chatterino release a fight over the same lines.
/// So the code keeps its English, and the text is looked up here on its way
/// to the screen. A text with no entry stays English, which is why this can
/// grow a piece at a time.
///
/// Set CHATTIFLEXII_GERMAN_MISSING to a file path to have every text that
/// has no entry written there - that is how the list below is kept honest.
namespace chatterino::german {

/// The German text for @a english, or @a english when there is none yet
QString say(const QString &english);

/// @a english and its German text, for the search in the settings - so a
/// setting is found by either
QStringList bothWords(const QString &english);

/// Puts the German text on every label, button, group, tab and tooltip under
/// @a root that has one. For the pages that build their widgets themselves,
/// instead of going through the page view. What a dropdown offers is left
/// alone: those texts are the values of the settings, not labels.
void translateWidgets(QWidget *root);

}  // namespace chatterino::german
