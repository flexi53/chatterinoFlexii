// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

class QWidget;

namespace chatterino::updatecheck {
struct Release;
}  // namespace chatterino::updatecheck

/// Puts a newer ChattiFlexii in place of the running one with one click, on
/// macOS: the disk image is downloaded, checked against the checksum GitHub
/// gives, and the app copied out of it next to the running one - all while
/// the old one still runs, so nothing is lost if any of it fails. Then the
/// app quits, a small script swaps the two and opens the new one. Settings
/// are kept, as they live apart from the app. As the download never passes
/// through a browser, macOS does not ask whether to open it.
namespace chatterino::selfupdate {

/// Whether this app can replace itself: macOS, a build GitHub made, and an
/// app bundle in a folder it may write to
bool canInstall();

/// Downloads @a release and restarts into it, showing how far it got in a
/// window over @a parent - or what went wrong, with the app left as it was
void install(const updatecheck::Release &release, QWidget *parent);

/// The app bundle this runs from - /Applications/ChattiFlexii.app - or an
/// empty string when it runs from something else
QString bundlePath();

/// Copies ChattiFlexii.app out of the disk image @a diskImage to
/// @a staging, which is replaced if it is there. Returns false and the
/// reason in @a error.
bool stageFromDiskImage(const QString &diskImage, const QString &staging,
                        QString &error);

/// The shell script that, once the process with the id given as $1 has
/// ended, puts the app at $2 in place of the one at $3 - keeping the old one
/// if the swap fails - and opens it, unless $4 is "noopen"
QString swapScript();

}  // namespace chatterino::selfupdate
