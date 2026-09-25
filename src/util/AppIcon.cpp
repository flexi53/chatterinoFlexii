// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/AppIcon.hpp"

#include "singletons/Settings.hpp"

#include <QApplication>
#include <QHash>
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
    return {
        // Die Kachel bleibt hell, Bogen und Körper wechseln
        "violett", "blau", "gruen", "orange", "rosa",
        // Ganz durchgemustert
        "camouflage", "mitternacht", "sonnenuntergang", "neon", "regenbogen",
        // Was gerade überall zu sehen ist
        "chrom", "holo", "feuer", "eis", "aurora",
    };
}

QString nameOf(const QString &key)
{
    static const QHash<QString, QString> NAMEN{
        {"violett", QStringLiteral("Violett")},
        {"blau", QStringLiteral("Blau")},
        {"gruen", QStringLiteral("Grün")},
        {"orange", QStringLiteral("Orange")},
        {"rosa", QStringLiteral("Rosa")},
        {"camouflage", QStringLiteral("Camouflage")},
        {"mitternacht", QStringLiteral("Mitternacht")},
        {"sonnenuntergang", QStringLiteral("Sonnenuntergang")},
        {"neon", QStringLiteral("Neon")},
        {"regenbogen", QStringLiteral("Regenbogen")},
        {"chrom", QStringLiteral("Chrom")},
        {"holo", QStringLiteral("Holo")},
        {"feuer", QStringLiteral("Feuer")},
        {"eis", QStringLiteral("Eis")},
        {"aurora", QStringLiteral("Aurora")},
    };
    return NAMEN.value(key, key);
}

QString pathOf(const QString &key)
{
    const auto wanted = keys().contains(key) ? key : keys().front();
    return QStringLiteral(":/icons/chattiflexii-%1.png").arg(wanted);
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
