// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QRect>
#include <QString>

class QPainter;

namespace chatterino::chatbackground {

/// ChattiFlexii: how the picture fills the chat - see Aussehen -> Chat
enum class Fit {
    /// Covers the whole chat; what does not fit is cut off at the edges
    Fill = 0,
    /// Shows the whole picture, the chat's colour stays at the edges
    Whole = 1,
    /// Laid side by side at its own size
    Tile = 2,
    /// In the middle at its own size
    Middle = 3,
};

/// Whether a picture is set at all
bool isSet();

/// Draws the picture over @a area and lays the veil of @a chatColor over
/// it. Without that veil the writing would stand on a bright picture and
/// nobody could read it, so it is what makes the picture usable rather than
/// decoration. Does nothing while no picture is set.
void paint(QPainter &painter, const QRect &area, const QColor &chatColor);

/// Throws away what is kept, after the file or the way it is fitted changed
void forget();

/// Takes @a file into the profile, so the picture stays where Chatti finds
/// it even when the original is moved or the stick it came from is pulled.
/// Returns the path it now lies at, or an empty string if it could not be
/// read as a picture.
QString adopt(const QString &file);

}  // namespace chatterino::chatbackground
