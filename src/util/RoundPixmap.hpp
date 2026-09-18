// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QPixmap>

class QPainter;
class QRectF;

namespace chatterino {

/// Fills the circle in @a rect with @a pixmap - painted as a textured circle
/// rather than through a clip, which Qt does not smooth, so the edge comes
/// out clean. Profile pictures everywhere are drawn this way.
void paintRound(QPainter &painter, const QRectF &rect, const QPixmap &pixmap);

/// @a pixmap cut to a circle @a side pixels across
QPixmap roundPixmap(const QPixmap &pixmap, int side);

}  // namespace chatterino
