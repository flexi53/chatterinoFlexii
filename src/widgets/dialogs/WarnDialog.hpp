// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <functional>

namespace chatterino {

/// The usercard's "Verwarnen": asks for the reason, as Twitch wants one -
/// the user is shown it, and writes again only once they have read it.
/// Offers the reason given last and the ones set under Buttons.
class WarnDialog : public QDialog
{
public:
    WarnDialog(const QString &userName, QWidget *parent);

    /// Told the reason once "Verwarnen" is pressed
    std::function<void(const QString &reason)> onWarn;

    /// What is offered: the reason given @a last first, then @a presets -
    /// one per line - each once, without the empty ones
    static QStringList choices(const QString &presets, const QString &last);

    /// The most Twitch takes for a reason
    static constexpr int MOST_CHARACTERS = 500;
};

}  // namespace chatterino
