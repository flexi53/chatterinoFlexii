// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace chatterino {

class Paths;

/// Copies the plugins shipped with the app into the user's plugin directory.
/// A plugin that is already there is left alone, so local changes survive an
/// update. Installed plugins still have to be switched on by hand.
///
/// Must run before the plugin controller scans the directory.
void installBundledPlugins(const Paths &paths);

/// On a fresh profile, offers to start out configured rather than empty: by
/// copying an existing Chatterino installation's settings over, or by
/// importing an export made on another computer. The other installation is
/// only read from, never changed.
///
/// Must run before the settings are read.
void importExistingProfile(const Paths &paths);

/// Writes this profile to a new folder on the desktop, to be carried to
/// another computer: settings, tabs, highlights, commands, the moderation
/// assistant's cases, themes and plugins - not logs or caches. The Twitch
/// login stays behind unless @a includeLogin.
///
/// The settings on disk are copied as they are, so they should be saved
/// first. Returns the folder, or an empty string and the reason in @a error.
QString exportProfile(const Paths &paths, bool includeLogin, QString &error);

/// exportProfile for the profile in @a rootDirectory, into a folder in
/// @a parentFolder named @a namePrefix and the time - what backups and the
/// tests use
QString exportProfileTo(
    const QString &rootDirectory, const QString &parentFolder,
    bool includeLogin, QString &error,
    const QString &namePrefix = QStringLiteral("ChattiFlexii-Export"));

/// Whether @a folder holds an export written by exportProfile
bool isProfileExport(const QString &folder);

/// Sets the export in @a folder aside to replace this profile the next time
/// the app starts - the running app would write its own settings back over
/// it on the way out. Returns false and the reason in @a error if it could
/// not be copied.
bool stageProfileImport(const Paths &paths, const QString &folder,
                        QString &error);
/// stageProfileImport for the profile in @a rootDirectory
bool stageProfileImportAt(const QString &rootDirectory, const QString &folder,
                          QString &error);

/// Carries out an import set aside by stageProfileImport: this profile's
/// settings move to a backup folder and the export takes their place. A login
/// this computer already has is kept when the export brings none.
///
/// Must run before the settings are read.
void applyPendingImport(const Paths &paths);
/// applyPendingImport for the profile in @a rootDirectory
void applyPendingImportAt(const QString &rootDirectory);

/// Starts the app again once this process has exited, so an import set aside
/// is applied without the user having to reopen it. Returns false if the
/// restart could not be arranged.
bool relaunchAfterExit();

}  // namespace chatterino
