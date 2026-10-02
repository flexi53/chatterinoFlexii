// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/ChatBackground.hpp"

#include "Application.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPixmap>

#include <algorithm>

using namespace Qt::Literals;

namespace chatterino::chatbackground {

namespace {

/// The picture as it was last drawn - fitting it anew for every chat and
/// every repaint would cost more than the chat itself. The chat only ever
/// paints in the GUI thread, so one of these is enough.
struct Kept {
    QString from;
    int fit = -1;
    QSize over;
    QPixmap picture;
};

/// Only ever asked for while something paints - a QPixmap may not exist
/// before Qt itself does, so this one comes into being on first use
Kept &kept()
{
    static Kept it;
    return it;
}

/// Where the pictures taken into the profile lie
QString ourDirectory()
{
    return QDir(getApp()->getPaths().rootAppDataDirectory)
        .filePath(u"Backgrounds"_s);
}

QPixmap fitted(const QString &file, Fit fit, QSize over)
{
    QPixmap source(file);
    if (source.isNull() || over.isEmpty())
    {
        return {};
    }

    switch (fit)
    {
        case Fit::Fill:
            return source.scaled(over, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
        case Fit::Whole:
            return source.scaled(over, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
        case Fit::Tile:
        case Fit::Middle:
            break;
    }
    return source;
}

}  // namespace

bool isSet()
{
    return !getSettings()->chatBackground.getValue().isEmpty();
}

void forget()
{
    kept() = {};
}

void paint(QPainter &painter, const QRect &area, const QColor &chatColor)
{
    const auto file = getSettings()->chatBackground.getValue();
    if (file.isEmpty() || area.isEmpty())
    {
        return;
    }

    const int wanted =
        std::clamp(getSettings()->chatBackgroundFit.getValue(), 0, 3);
    const auto fit = static_cast<Fit>(wanted);

    auto &held = kept();
    if (held.from != file || held.fit != wanted || held.over != area.size())
    {
        held.from = file;
        held.fit = wanted;
        held.over = area.size();
        held.picture = fitted(file, fit, area.size());
    }

    if (held.picture.isNull())
    {
        return;
    }

    painter.save();
    painter.setClipRect(area);

    if (fit == Fit::Tile)
    {
        painter.drawTiledPixmap(area, held.picture);
    }
    else
    {
        // Filled, the picture is bigger than the chat - it is the middle of
        // it that shows
        QRect where(QPoint(), held.picture.size());
        where.moveCenter(area.center());
        painter.drawPixmap(where, held.picture);
    }

    // ChattiFlexii: the chat's own colour laid over it, as far as asked for.
    // A picture at full strength leaves the writing unreadable, so this is
    // what makes it a background instead of a wall.
    const int veil =
        std::clamp(getSettings()->chatBackgroundVeil.getValue(), 0, 100);
    if (veil > 0)
    {
        QColor over = chatColor;
        over.setAlpha(std::clamp(veil * 255 / 100, 0, 255));
        painter.fillRect(area, over);
    }

    painter.restore();
}

QString adopt(const QString &file)
{
    if (QPixmap(file).isNull())
    {
        return {};
    }

    QDir where(ourDirectory());
    if (!where.mkpath(u"."_s))
    {
        return {};
    }

    // The one before it goes - only one picture is ever used, and the name
    // carries the moment so a new file is seen as a new one
    for (const auto &old : where.entryList({u"chat-*"_s}, QDir::Files))
    {
        where.remove(old);
    }

    const QFileInfo what(file);
    const QString suffix =
        what.suffix().isEmpty() ? u"png"_s : what.suffix().toLower();
    const QString to = where.filePath(
        u"chat-%1.%2"_s.arg(QDateTime::currentMSecsSinceEpoch()).arg(suffix));

    if (!QFile::copy(file, to))
    {
        return {};
    }
    return to;
}

}  // namespace chatterino::chatbackground
