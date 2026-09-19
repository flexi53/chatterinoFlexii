// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <optional>

/// Looks on GitHub whether there is a newer ChattiFlexii download than the
/// one running, and offers it. Only the downloads look: they are built by
/// GitHub from a known commit, while a build made at home has nothing to
/// compare with.
namespace chatterino::updatecheck {

/// The download GitHub has on offer
struct Release {
    /// The commit it was built from
    QString commit;
    /// The file for this computer - the DMG on macOS, the ZIP on Windows
    QString downloadUrl;
    QDateTime published;
    /// Its SHA-256 as GitHub gives it, in hex - empty if it gives none
    QString sha256;
};

/// Reads the release GitHub describes in @a json - the commit named in its
/// text and the download called @a assetName. Nothing if either is missing.
std::optional<Release> parseRelease(const QJsonObject &json,
                                    const QString &assetName);

/// Whether @a release is another build than the one from @a runningCommit -
/// which may be the short form of the commit
bool isNewer(const Release &release, const QString &runningCommit);

/// Whether this build looks at all
bool canCheck();

/// Looks soon after start and every few hours after. Called once the app is
/// up; later calls do nothing.
void start();

/// Looks now. With @a fromUser it also says when there is nothing newer, and
/// offers a download that was put off with "Später".
void checkNow(bool fromUser);

}  // namespace chatterino::updatecheck
