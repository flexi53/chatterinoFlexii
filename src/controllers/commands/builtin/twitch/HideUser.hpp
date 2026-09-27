// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace chatterino {

struct CommandContext;

}  // namespace chatterino

namespace chatterino::commands {

/// /hide - their messages stop arriving, for you alone
QString hideUser(const CommandContext &ctx);
/// /unhide
QString unhideUser(const CommandContext &ctx);
/// /hidden - who is hidden
QString listHiddenUsers(const CommandContext &ctx);

}  // namespace chatterino::commands
