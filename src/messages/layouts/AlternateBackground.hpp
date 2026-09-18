// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QString>

#include <array>

namespace chatterino {

class MessageLayout;
struct Message;

namespace alternatebg {

/// How far every other message is brought towards its colour, in percent,
/// for each strength on offer - 0 keeps the theme's own shade
inline constexpr std::array<int, 5> STRENGTHS{0, 5, 9, 14, 20};

/// The strength a colour is given when the strength is left to the theme,
/// which only has a shade of grey
inline constexpr int TINT_STRENGTH = STRENGTHS[1];

/// The background every other message gets: @a regular brought @a strength
/// percent towards @a tint - or, with no tint, towards white on a dark
/// background and black on a light one. With neither a strength nor a tint
/// it is the theme's own @a themeAlternate.
QColor background(const QColor &regular, const QColor &themeAlternate,
                  int strength, const QColor &tint);

/// Whether @a message gets the alternate background, next to @a neighbour -
/// the message before it, or the one after it when it is added at the top.
/// Every message takes the other background than its neighbour, except with
/// @a bySender when both come from the same sender: then it keeps the same
/// one, so what one chatter writes in a row reads as one block.
bool alternateNextTo(MessageLayout *neighbour, const Message &message,
                     bool bySender);

}  // namespace alternatebg

}  // namespace chatterino
