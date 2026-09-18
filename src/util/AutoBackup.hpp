// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QString>

/// Makes an export of the profile every few days by itself, into a folder the
/// user can choose, and keeps the newest few - for when something breaks or
/// a device is set up again. The Twitch login is never in a backup.
namespace chatterino::autobackup {

/// Starts looking every few hours whether a backup is due. Called once the
/// app is up; later calls do nothing.
void start();

/// Makes a backup now. Returns its folder, or an empty string and the reason
/// in @a error.
QString backUpNow(QString &error);

/// Where backups go when no folder is chosen: "ChattiFlexii-Sicherungen" in
/// iCloud Drive where there is one, otherwise in Documents
QString defaultFolder();

/// Where backups go
QString folder();

/// When the last backup was made; invalid if never
QDateTime lastBackup();

}  // namespace chatterino::autobackup
