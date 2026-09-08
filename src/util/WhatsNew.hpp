// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

namespace chatterino {

/// Shows what has changed since the user last saw this window, and remembers
/// that they have seen it. Does nothing when there is nothing new to tell.
///
/// Call once the main window is up, so it has something to sit on top of.
void showWhatsNew();

}  // namespace chatterino
