// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/AppIcon.hpp"

#include "singletons/Settings.hpp"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QPixmap>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifdef Q_OS_MACOS
#    include "util/MacOsHelpers.h"
#endif

namespace chatterino::appicon {

QStringList keys()
{
    return {"blau", "violett", "gruen", "orange", "rosa"};
}

QString nameOf(const QString &key)
{
    if (key == "blau")
    {
        return QStringLiteral("Blau");
    }
    if (key == "violett")
    {
        return QStringLiteral("Violett");
    }
    if (key == "gruen")
    {
        return QStringLiteral("Grün");
    }
    if (key == "orange")
    {
        return QStringLiteral("Orange");
    }
    if (key == "rosa")
    {
        return QStringLiteral("Rosa");
    }
    return key;
}

QString pathOf(const QString &key)
{
    const auto wanted = keys().contains(key) ? key : keys().front();
    return QStringLiteral(":/icons/chattiflexii-%1.svg").arg(wanted);
}

QString picked()
{
    const auto kept = getSettings()->appIcon.getValue();
    return keys().contains(kept) ? kept : keys().front();
}

void apply()
{
    const auto key = picked();
    QApplication::setWindowIcon(QIcon(pathOf(key)));

#ifdef Q_OS_MACOS
    // The Dock shows the icon of the bundle, which is signed and not to be
    // touched. A picture handed to the running program does the job for as
    // long as it runs - and for the colour it was built with there is
    // nothing to hand over.
    if (key == keys().front())
    {
        chatterinoSetMacOsDockIcon("");
        return;
    }

    // NSImage reads a file, not a resource, so the picture is drawn once
    // into the program's own folder
    static QString drawn;
    const auto folder =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const auto file = folder + QStringLiteral("/chattiflexii-dock-%1.png").arg(key);
    if (drawn != file)
    {
        const QPixmap picture = QIcon(pathOf(key)).pixmap(1024, 1024);
        if (!picture.isNull() && picture.save(file, "PNG"))
        {
            drawn = file;
        }
    }
    if (!drawn.isEmpty())
    {
        chatterinoSetMacOsDockIcon(drawn.toUtf8().constData());
    }
#endif
}

}  // namespace chatterino::appicon
