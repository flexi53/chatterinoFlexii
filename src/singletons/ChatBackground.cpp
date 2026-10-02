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
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QStringList>
#include <QWidget>

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

/// Which places the picture was switched off in
QSet<QString> switchedOff()
{
    const auto value = getSettings()->chatBackgroundOffChannels.getValue();
    QSet<QString> places;
    for (const auto &place : value.split(',', Qt::SkipEmptyParts))
    {
        places.insert(place);
    }
    return places;
}

/// What each place picked for itself, by key
QJsonObject ownPictures()
{
    const auto value = getSettings()->chatBackgroundPerChannel.getValue();
    if (value.isEmpty())
    {
        return {};
    }
    return QJsonDocument::fromJson(value.toUtf8()).object();
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
    return !getSettings()->chatBackground.getValue().isEmpty() ||
           !ownPictures().isEmpty();
}

bool shownIn(const QString &key)
{
    if (key.isEmpty())
    {
        return true;
    }
    return !switchedOff().contains(key.toLower());
}

void setShownIn(const QString &key, bool shown)
{
    if (key.isEmpty())
    {
        return;
    }

    auto places = switchedOff();
    if (shown)
    {
        places.remove(key.toLower());
    }
    else
    {
        places.insert(key.toLower());
    }

    auto names = QStringList(places.begin(), places.end());
    names.sort();
    getSettings()->chatBackgroundOffChannels.setValue(names.join(','));
}

QString fileFor(const QString &key)
{
    if (!shownIn(key))
    {
        return {};
    }

    const auto own = ownPictures().value(key.toLower()).toString();
    if (!own.isEmpty())
    {
        return own;
    }
    return getSettings()->chatBackground.getValue();
}

void setFileFor(const QString &key, const QString &file)
{
    if (key.isEmpty())
    {
        return;
    }

    auto own = ownPictures();
    if (file.isEmpty())
    {
        own.remove(key.toLower());
    }
    else
    {
        own[key.toLower()] = file;
    }

    getSettings()->chatBackgroundPerChannel.setValue(
        own.isEmpty() ? QString()
                      : QString::fromUtf8(
                            QJsonDocument(own).toJson(QJsonDocument::Compact)));
    tidy();
}

Place placeOf(const QWidget *widget, QPoint inside)
{
    if (widget == nullptr)
    {
        return {};
    }

    // ChattiFlexii: one picture over the whole window means every chat
    // shows its own cut-out of it, and the room between them carries the
    // piece that belongs there - the window reads as one picture with the
    // chats laid on it
    if (getSettings()->chatBackgroundSpan)
    {
        const auto *window = widget->window();
        if (window != nullptr)
        {
            return {window->size(), widget->mapTo(window, inside)};
        }
    }
    return {widget->size(), inside};
}

void paint(QPainter &painter, const QRect &area, const Place &place,
           const QColor &veilColor, const QString &file)
{
    if (file.isEmpty() || area.isEmpty() || place.whole.isEmpty())
    {
        return;
    }

    const int wanted =
        std::clamp(getSettings()->chatBackgroundFit.getValue(), 0, 3);
    const auto fit = static_cast<Fit>(wanted);

    auto &held = kept();
    if (held.from != file || held.fit != wanted || held.over != place.whole)
    {
        held.from = file;
        held.fit = wanted;
        held.over = place.whole;
        held.picture = fitted(file, fit, place.whole);
    }

    if (held.picture.isNull())
    {
        return;
    }

    painter.save();
    painter.setClipRect(area);

    // What shows here is the piece of the picture this spot stands on
    const QRect over(area.topLeft() - place.at, place.whole);

    if (fit == Fit::Tile)
    {
        painter.drawTiledPixmap(over, held.picture);
    }
    else
    {
        // Filled, the picture is bigger than the room - it is the middle of
        // it that shows
        QRect where(QPoint(), held.picture.size());
        where.moveCenter(over.center());
        painter.drawPixmap(where, held.picture);
    }

    // ChattiFlexii: the chat's own colour laid over it, as far as asked for.
    // A picture at full strength leaves the writing unreadable, so this is
    // what makes it a background instead of a wall.
    const int veil =
        std::clamp(getSettings()->chatBackgroundVeil.getValue(), 0, 100);
    if (veil > 0)
    {
        QColor veiled = veilColor;
        veiled.setAlpha(std::clamp(veil * 255 / 100, 0, 255));
        painter.fillRect(area, veiled);
    }

    painter.restore();
}

void forget()
{
    kept() = {};
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

    const QFileInfo what(file);
    const QString suffix =
        what.suffix().isEmpty() ? u"png"_s : what.suffix().toLower();
    // The name carries the moment, so a picture chosen anew counts as a new
    // one even where the file it came from was called the same
    const QString to = where.filePath(
        u"chat-%1.%2"_s.arg(QDateTime::currentMSecsSinceEpoch()).arg(suffix));

    if (!QFile::copy(file, to))
    {
        return {};
    }
    return to;
}

void tidy()
{
    QSet<QString> inUse;
    const auto general = getSettings()->chatBackground.getValue();
    if (!general.isEmpty())
    {
        inUse.insert(QFileInfo(general).fileName());
    }
    const auto own = ownPictures();
    for (auto it = own.begin(); it != own.end(); ++it)
    {
        inUse.insert(QFileInfo(it.value().toString()).fileName());
    }

    QDir where(ourDirectory());
    for (const auto &there : where.entryList({u"chat-*"_s}, QDir::Files))
    {
        if (!inUse.contains(there))
        {
            where.remove(there);
        }
    }
}

}  // namespace chatterino::chatbackground
