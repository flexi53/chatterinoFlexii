// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QStringList>
#include <QWidget>

class QHBoxLayout;

namespace chatterino {

/// A line on the user card showing where someone moderates: the channels'
/// pictures - the mod highlights channels first - and a button with the whole
/// list, former channels included. Asks whosthemod.xyz the way /wtm in the
/// WhoseTheMod plugin does, and stays hidden while that plugin is off, the
/// line is switched off, or the user moderates nowhere known.
class ModChannelsRow : public QWidget
{
public:
    explicit ModChannelsRow(QWidget *parent = nullptr);

    /// Shows where @a login moderates, once whosthemod.xyz has answered
    void showFor(const QString &login);

private:
    void fill(const QStringList &channels, int total, const QStringList &former,
              int formerTotal);

    QHBoxLayout *layout_{};
    QString login_;
};

}  // namespace chatterino
