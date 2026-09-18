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

/// Backups are named apart from exports made by hand, so pruning only ever
/// touches its own
const QString BACKUP_PREFIX = QStringLiteral("ChattiFlexii-Sicherung");

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

/// Deletes all but the newest @a keep backups in @a folder. Only folders this
/// wrote are touched: named like a backup, and marked as an export.
void prune(const QString &folder, int keep)
{
    const QDir dir(folder);
    // The time in the name sorts newest last, so reversed the newest lead
    const auto entries = dir.entryInfoList(
        {BACKUP_PREFIX + QStringLiteral("*")},
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

    int kept = 0;
    for (const auto &entry : entries)
    {
        if (!isProfileExport(entry.absoluteFilePath()))
        {
            continue;
        }
        if (++kept > keep)
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

    const auto made =
        exportProfileTo(getApp()->getPaths().rootAppDataDirectory, target,
                        false, error, BACKUP_PREFIX);
    if (made.isEmpty())
    {
        return {};
    }

    getSettings()->autoBackupLast.setValue(
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    prune(target, std::max(1, getSettings()->autoBackupKeep.getValue()));
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
