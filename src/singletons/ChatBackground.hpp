// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

class QPainter;
class QWidget;

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

/// ChattiFlexii: where a piece of the picture lies. @a whole is the size
/// the picture is fitted to, @a at where the piece being painted sits
/// inside that. A chat carrying the picture by itself passes its own size
/// and the origin; everything drawn from one picture spanning the window
/// passes the window's size and where it stands in the window.
struct Place {
    QSize whole;
    QPoint at;
};

/// Whether a picture is set at all
bool isSet();

/// ChattiFlexii: whether the picture is wanted in @a key - true unless it
/// was switched off there. @a key is what pinnedsplits::nameOf gives, so
/// /mentions and the rest can be told apart from channels.
bool shownIn(const QString &key);
void setShownIn(const QString &key, bool shown);

/// ChattiFlexii: the picture for @a key - the one chosen for it, otherwise
/// the general one. Empty where none applies or it was switched off there.
QString fileFor(const QString &key);
/// Gives @a key a picture of its own; an empty @a file puts it back on the
/// general one
void setFileFor(const QString &key, const QString &file);

/// Where @a widget stands, following "spread over the whole window". @a
/// inside is the corner of the piece within @a widget.
Place placeOf(const QWidget *widget, QPoint inside = {});

/// Draws @a file over @a area as @a place says, and lays the veil of @a
/// veilColor over it. Without that veil the writing would stand on a
/// bright picture and nobody could read it, so it is what makes the
/// picture usable rather than decoration. Does nothing for an empty file.
void paint(QPainter &painter, const QRect &area, const Place &place,
           const QColor &veilColor, const QString &file);

/// Throws away what is kept, after a file or the way it is fitted changed
void forget();

/// Takes @a file into the profile, so the picture stays where Chatti finds
/// it even when the original is moved or the stick it came from is pulled.
/// Returns the path it now lies at, or an empty string if it could not be
/// read as a picture.
QString adopt(const QString &file);

/// Removes pictures in the profile that nothing points at any more
void tidy();

}  // namespace chatterino::chatbackground
