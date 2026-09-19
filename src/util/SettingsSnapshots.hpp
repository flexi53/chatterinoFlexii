// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <vector>

class QWidget;

/// Views: the settings saved under a name - "Moderieren", "Entspannt" - to
/// switch between with a click. A view holds every setting, but not the
/// tabs, not the Twitch login and not what belongs to this computer: where
/// windows sit, the sync state. Views live in Settings/Snapshots, so they
/// come along with an export and the sync. Switching restarts the app, as
/// not every setting can change while it runs; the settings in use are kept
/// as "Vor dem Wechsel" first, so there is always a way back.
namespace chatterino::snapshots {

/// What the settings in use are kept as before another view is loaded
inline const QString BEFORE_SWITCH = QStringLiteral("Vor dem Wechsel");

struct View {
    QString name;
    QDateTime saved;
};

/// The views saved in @a settingsDirectory, by name
std::vector<View> list(const QString &settingsDirectory);

/// Saves the settings on disk in @a settingsDirectory as the view @a name,
/// replacing one of that name. Returns false and the reason in @a error.
bool save(const QString &settingsDirectory, const QString &name,
          QString &error);

bool remove(const QString &settingsDirectory, const QString &name);

bool rename(const QString &settingsDirectory, const QString &from,
            const QString &to, QString &error);

/// Sets the view @a name aside to replace the settings at the next start,
/// after keeping the settings on disk as BEFORE_SWITCH
bool stage(const QString &settingsDirectory, const QString &name,
           QString &error);

/// Before the settings are read: puts a view set aside by stage in place of
/// the settings, keeping what belongs to this computer
void applyPending(const QString &settingsDirectory);

/// The settings of @a view, with what belongs to this computer taken from
/// @a current - the login, the sync state, where windows sit
QJsonObject merged(const QJsonObject &view, const QJsonObject &current);

/// Saves the settings in use and switches to the view @a name, restarting
/// the app after asking - over @a parent
void switchTo(const QString &name, QWidget *parent);

}  // namespace chatterino::snapshots
