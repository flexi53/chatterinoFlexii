// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace chatterino {

class MessageBuilder;
struct CommandContext;

/// The badges from ChattiFlexii's badge server, for any Chatterino: shows
/// them next to the names, and uploads one's own with /chattibadge.
/// Everything to set is at the top of ChattiBadgesClient.cpp.
namespace chattibadgesclient {

/// Puts the badge of @a userID into @a builder, if they have one. Call it
/// where the other badges are added to a chat message.
void appendTo(MessageBuilder &builder, const QString &userID);

/// /chattibadge <file> - uploads a picture as one's own badge. A still
/// picture is made 72 × 72, an animated GIF goes as it is.
QString uploadCommand(const CommandContext &ctx);

}  // namespace chattibadgesclient

}  // namespace chatterino
