// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

namespace chatterino {

class Paths;

/// Copies the plugins shipped with the app into the user's plugin directory.
/// A plugin that is already there is left alone, so local changes survive an
/// update. Installed plugins still have to be switched on by hand.
///
/// Must run before the plugin controller scans the directory.
void installBundledPlugins(const Paths &paths);

/// On a fresh profile, offers to copy an existing Chatterino installation's
/// settings over so the fork starts out configured rather than empty. The
/// other installation is only read from, never changed.
///
/// Must run before the settings are read.
void importExistingProfile(const Paths &paths);

}  // namespace chatterino
