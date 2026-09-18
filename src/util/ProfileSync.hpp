// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QString>

#include <optional>

/// Keeps the setup alike on the user's computers - the Mac and the MacBook -
/// through the backup folder, which iCloud Drive carries between them. Where
/// the windows sit stays with each computer. Each
/// computer leaves its setup there as "ChattiFlexii-Abgleich" when it
/// changed, and offers to take the other one's when that is newer. Nothing is
/// ever taken without asking, and a setup from another computer that has not
/// been answered is never written over. The Twitch login stays on each
/// computer.
namespace chatterino::profilesync {

/// The setup waiting in the folder, as the computer that left it describes it
struct Shared {
    /// Which computer left it - see thisComputer()
    QString computer;
    /// Its name, to show
    QString computerName;
    /// When it was left there, ISO 8601 in UTC - which one it is
    QString written;
    /// fingerprint() of what it holds, to tell it has fully arrived
    QString fingerprint;
    /// Which way the fingerprint was worked out; 1 for the first version
    int format = 1;
};

enum class Step {
    Nothing,
    /// Leave this computer's setup in the folder
    Write,
    /// Another computer's newer setup is waiting - ask whether to take it
    Offer,
    /// Another computer's setup is waiting, the same as this one: note it as
    /// seen, there is nothing to take
    Settle,
};

/// What to do, given what waits in the folder, which computer this is, which
/// setup it last wrote or took (@a base), what its setup looks like now and
/// what it looked like when it last wrote it
Step decide(const std::optional<Shared> &shared, const QString &computer,
            const QString &base, const QString &fingerprintNow,
            const QString &fingerprintWritten);

/// What the setup in @a rootDirectory looks like, for comparing: its settings
/// and tabs, without the login and without what changes by itself - when the
/// last backup was made, what was last written here
QString fingerprint(const QString &rootDirectory);

/// Which computer this is - stays the same across restarts
QString thisComputer();
/// This computer's name, to show
QString thisComputerName();

/// Where the shared setup is kept: "ChattiFlexii-Abgleich" in the backup
/// folder
QString sharedFolder();

/// What waits in @a folder, if a setup does
std::optional<Shared> readShared(const QString &folder);

/// Puts where this computer's windows sit, and how big they are, into the
/// setup staged in @a stagedRoot, from the one in @a localRoot - so taking
/// another computer's setup brings its settings and tabs but leaves the
/// windows arranged as they were here, on this computer's screens
void keepWindowPlaces(const QString &localRoot, const QString &stagedRoot);

/// Looks soon after start and every half hour after, and leaves this
/// computer's setup in the folder on the way out. Called once the app is up;
/// later calls do nothing.
void start();

/// Looks now: offers another computer's newer setup, or leaves this one's
/// when it changed. Returns what it did, for the settings page.
QString syncNow(bool fromUser);

}  // namespace chatterino::profilesync
