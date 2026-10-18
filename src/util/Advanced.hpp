// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

/// The parts of ChattiFlexii that belong to the one account it was built
/// for: the suggestions the moderation assistant learns, the people kept in
/// view and the automatic answers. Everyone else gets the program without
/// them - the settings page "Automatik" is not shown, and neither are the
/// shield and the speech bubble beside the input. The alert windows, their
/// page Mod-Alarme and the bell are everyone's.
///
/// This is tidying up, not a lock: the code is public, and anyone who
/// builds it themselves can put their own name in. It keeps the download
/// clean for people who have no use for those parts.
namespace chatterino::advanced {

/// The account those parts belong to
QString owner();

/// Whether the account logged in now is that one
bool unlocked();

/// For the tests, which have no account: what unlocked() should say.
/// Handing in nothing gives the account back the say.
void pretendUnlocked(bool unlocked);
void stopPretending();

}  // namespace chatterino::advanced
