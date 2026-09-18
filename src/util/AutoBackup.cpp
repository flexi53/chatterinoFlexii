// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/AutoBackup.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/ProfileSetup.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <chrono>

namespace chatterino::autobackup {

namespace {

/// Long enough after start that the app has settled and logged in
constexpr auto FIRST_CHECK = std::chrono::minutes(2);
constexpr auto CHECK_EVERY = std::chrono::hours(6);

/// The one backup there is, replaced by each new one
const QString BACKUP_NAME = QStringLiteral("ChattiFlexii-Sicherung");
/// What a new backup is called until it is complete and takes the old one's
/// place
const QString FRESH_PREFIX = QStringLiteral("ChattiFlexii-Sicherung (neu)");

bool due()
{
    if (!getSettings()->autoBackupEnabled.getValue())
    {
        return false;
    }
    const auto last = lastBackup();
    const auto days = std::max(1, getSettings()->autoBackupDays.getValue());
    return !last.isValid() || last.secsTo(QDateTime::currentDateTimeUtc()) >=
                                  qint64(days) * 24 * 60 * 60;
}

/// Backups other than the one kept: those an earlier version dated, and a new
/// one left unfinished. Only folders this wrote are touched - marked as an
/// export, or named as a new backup.
void removeOthers(const QDir &dir)
{
    for (const auto &entry : dir.entryInfoList(
             {BACKUP_NAME + QStringLiteral("*")},
             QDir::Dirs | QDir::NoDotAndDotDot))
    {
        if (entry.fileName() == BACKUP_NAME)
        {
            continue;
        }
        if (entry.fileName().startsWith(FRESH_PREFIX) ||
            isProfileExport(entry.absoluteFilePath()))
        {
            QDir(entry.absoluteFilePath()).removeRecursively();
        }
    }
}

void checkNow()
{
    if (!due())
    {
        return;
    }
    QString error;
    if (backUpNow(error).isEmpty())
    {
        qCWarning(chatterinoApp) << "Automatic backup failed:" << error;
    }
}

}  // namespace

void start()
{
    static bool started = false;
    if (started)
    {
        return;
    }
    started = true;

    auto *context = new QObject(QCoreApplication::instance());
    QTimer::singleShot(FIRST_CHECK, context, [] {
        checkNow();
    });

    auto *timer = new QTimer(context);
    timer->setInterval(CHECK_EVERY);
    QObject::connect(timer, &QTimer::timeout, context, [] {
        checkNow();
    });
    timer->start();

    // Switching it on makes the first backup in a moment, not days later.
    // Never destroyed, so nothing is torn down after the settings at exit.
    auto *connections = new pajlada::Signals::SignalHolder;
    getSettings()->autoBackupEnabled.connect(
        [context](const bool &on, auto) {
            if (on)
            {
                QTimer::singleShot(std::chrono::seconds(5), context, [] {
                    checkNow();
                });
            }
        },
        *connections, false);
}

QString backUpNow(QString &error)
{
    // Settings and tabs are otherwise only written when the app closes, and
    // the backup copies what is on disk
    getSettings()->requestSave();
    getApp()->getWindows()->save();

    const auto target = folder();
    if (!QDir().mkpath(target))
    {
        error = QStringLiteral("Der Ordner %1 konnte nicht angelegt werden.")
                    .arg(QDir::toNativeSeparators(target));
        return {};
    }
    const QDir dir(target);
    const auto kept = dir.absoluteFilePath(BACKUP_NAME);
    if (QFileInfo::exists(kept) && !isProfileExport(kept))
    {
        error = QStringLiteral("Im Ordner liegt schon „%1“, aber keine "
                               "Sicherung - er wird nicht überschrieben.")
                    .arg(BACKUP_NAME);
        return {};
    }

    // Written next to the old one first, so a backup that fails never costs
    // the last good one
    const auto fresh =
        exportProfileTo(getApp()->getPaths().rootAppDataDirectory, target,
                        false, error, FRESH_PREFIX);
    if (fresh.isEmpty())
    {
        return {};
    }

    if (QFileInfo::exists(kept) && !QDir(kept).removeRecursively())
    {
        QDir(fresh).removeRecursively();
        error = QStringLiteral("Die vorige Sicherung ließ sich nicht ersetzen.");
        return {};
    }

    auto made = kept;
    if (!QDir().rename(fresh, kept))
    {
        // Still a complete backup, just under its interim name
        qCWarning(chatterinoApp) << "Could not rename the backup" << fresh;
        made = fresh;
    }
    else
    {
        removeOthers(dir);
    }

    getSettings()->autoBackupLast.setValue(
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return made;
}

QString defaultFolder()
{
#ifdef Q_OS_MACOS
    const auto iCloud = QDir::home().absoluteFilePath(
        QStringLiteral("Library/Mobile Documents/com~apple~CloudDocs"));
    if (QFileInfo(iCloud).isDir())
    {
        return QDir(iCloud).absoluteFilePath(
            QStringLiteral("ChattiFlexii-Sicherungen"));
    }
#endif
    return QDir(QStandardPaths::writableLocation(
                    QStandardPaths::DocumentsLocation))
        .absoluteFilePath(QStringLiteral("ChattiFlexii-Sicherungen"));
}

QString folder()
{
    const auto chosen = getSettings()->autoBackupFolder.getValue().trimmed();
    return chosen.isEmpty() ? defaultFolder() : chosen;
}

QDateTime lastBackup()
{
    return QDateTime::fromString(getSettings()->autoBackupLast.getValue(),
                                 Qt::ISODate);
}

}  // namespace chatterino::autobackup
