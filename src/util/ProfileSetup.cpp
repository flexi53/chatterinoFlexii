// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/ProfileSetup.hpp"

#include "common/QLogging.hpp"
#include "singletons/Paths.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringBuilder>

#include <functional>

namespace {

using namespace chatterino;

/// The parts of a profile worth carrying over. Everything else - logs, caches,
/// crash dumps, the IPC socket - is either huge, stale or specific to the
/// installation that wrote it.
const QStringList PROFILE_CONTENTS{
    QStringLiteral("Settings"),   QStringLiteral("Themes"),
    QStringLiteral("Plugins"),    QStringLiteral("Misc"),
    QStringLiteral("Dictionaries"),
};

/// Directory names other Chatterino installations use, in the order we would
/// rather find them. Case matters on Linux, so both spellings are listed.
const QStringList CHATTERINO_DIRECTORIES{
    QStringLiteral("Chatterino2"),  // Windows
    QStringLiteral("chatterino"),   // macOS, Linux
    QStringLiteral("Chatterino"),
};

/// Left behind when the user turns the offer down, so they are not asked again
/// on every start until the settings file finally exists.
const QString DECLINED_MARKER = QStringLiteral(".no-profile-import");

/// Marks a folder as one of our exports, and says what is in it
const QString EXPORT_MARKER = QStringLiteral("chattiflexii-export.json");

/// Where an import chosen in the settings waits for the next start
const QString PENDING_IMPORT = QStringLiteral("PendingImport");

/// Copies a file and gives the copy normal permissions. Anything read out of
/// the Qt resource system arrives read-only, which would leave the user unable
/// to edit a plugin they have installed.
bool copyFile(const QString &from, const QString &to)
{
    if (!QFile::copy(from, to))
    {
        return false;
    }

    return QFile::setPermissions(to, QFileDevice::ReadOwner |
                                         QFileDevice::WriteOwner |
                                         QFileDevice::ReadGroup |
                                         QFileDevice::ReadOther);
}

/// Recursively copies @a from onto @a to, creating directories as needed.
/// Existing files are left as they are, and so is anything @a skip turns
/// down.
bool copyTree(const QString &from, const QString &to,
              const std::function<bool(const QFileInfo &)> &skip = {})
{
    QDir source(from);
    if (!source.exists())
    {
        return false;
    }

    if (!QDir().mkpath(to))
    {
        return false;
    }

    bool ok = true;

    for (const auto &entry : source.entryInfoList(
             QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden))
    {
        if (skip && skip(entry))
        {
            continue;
        }

        const auto target = QDir(to).absoluteFilePath(entry.fileName());

        if (entry.isDir())
        {
            ok = copyTree(entry.absoluteFilePath(), target, skip) && ok;
            continue;
        }

        if (QFileInfo::exists(target))
        {
            continue;
        }

        if (!copyFile(entry.absoluteFilePath(), target))
        {
            qCWarning(chatterinoApp)
                << "Failed to copy" << entry.absoluteFilePath() << "to"
                << target;
            ok = false;
        }
    }

    return ok;
}

/// Moves @a from to @a to, copying where a plain rename is not possible
bool moveTree(const QString &from, const QString &to)
{
    if (QDir().rename(from, to))
    {
        return true;
    }
    if (!copyTree(from, to))
    {
        return false;
    }
    return QDir(from).removeRecursively();
}

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeJsonObject(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

/// The Chatterino profile to offer, or an empty string if there is none worth
/// offering. A profile counts as worth offering once it has settings in it.
QString findChatterinoProfile(const Paths &paths)
{
    const QDir parent = QFileInfo(paths.rootAppDataDirectory).dir();

    for (const auto &name : CHATTERINO_DIRECTORIES)
    {
        const auto candidate = parent.absoluteFilePath(name);

        // On a case insensitive file system this can be our own directory
        if (QFileInfo(candidate) == QFileInfo(paths.rootAppDataDirectory))
        {
            continue;
        }

        if (QFileInfo::exists(candidate % QStringLiteral("/Settings/settings.json")))
        {
            return candidate;
        }
    }

    return {};
}

/// Copies the Chatterino installation at @a source into this profile
void copyChatterinoProfile(const Paths &paths, const QString &source)
{
    bool ok = true;
    for (const auto &part : PROFILE_CONTENTS)
    {
        const auto from = QDir(source).absoluteFilePath(part);
        if (!QFileInfo::exists(from))
        {
            continue;  // that installation never made one
        }

        ok = copyTree(from, QDir(paths.rootAppDataDirectory)
                                .absoluteFilePath(part)) &&
             ok;
    }

    if (ok)
    {
        return;  // the settings speak for themselves once the app opens
    }

    QMessageBox::warning(
        nullptr, QStringLiteral("ChattiFlexii"),
        QStringLiteral("Some of the settings could not be copied. Whatever "
                       "came across is in place; the rest you will have to "
                       "set up by hand. Chatterino itself is unchanged."));
}

/// Asks for an export folder until one is picked, or the user gives up - then
/// the string is empty. AirDrop leaves folders in Downloads, so that is where
/// it starts looking.
QString askForExportFolder()
{
    const auto start =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    while (true)
    {
        const auto folder = QFileDialog::getExistingDirectory(
            nullptr, QStringLiteral("Choose a ChattiFlexii export"), start);
        if (folder.isEmpty() || isProfileExport(folder))
        {
            return folder;
        }

        QMessageBox::warning(
            nullptr, QStringLiteral("ChattiFlexii"),
            QStringLiteral("There is no ChattiFlexii export in that folder. "
                           "Choose the \"ChattiFlexii-Export ...\" folder "
                           "itself, not the one it is in."));
    }
}

}  // namespace

namespace chatterino {

void installBundledPlugins(const Paths &paths)
{
    QDir bundled(QStringLiteral(":/plugins"));
    if (!bundled.exists())
    {
        return;
    }

    for (const auto &entry :
         bundled.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        const auto target =
            QDir(paths.pluginsDirectory).absoluteFilePath(entry.fileName());

        if (QFileInfo::exists(target))
        {
            continue;  // already installed, possibly edited by the user
        }

        if (copyTree(entry.absoluteFilePath(), target))
        {
            qCInfo(chatterinoApp)
                << "Installed bundled plugin" << entry.fileName();
        }
        else
        {
            qCWarning(chatterinoApp)
                << "Failed to install bundled plugin" << entry.fileName();
        }
    }
}

void importExistingProfile(const Paths &paths)
{
    const QDir settings(paths.settingsDirectory);

    // Anything in here means this profile has been used before
    if (QFileInfo::exists(settings.absoluteFilePath("settings.json")) ||
        QFileInfo::exists(settings.absoluteFilePath("window-layout.json")) ||
        QFileInfo::exists(settings.absoluteFilePath(DECLINED_MARKER)))
    {
        return;
    }

    const auto source = findChatterinoProfile(paths);

    // Asked again when the user backs out of choosing an export
    while (true)
    {
        QMessageBox box;
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(QStringLiteral("Welcome to ChattiFlexii"));

        QPushButton *copyChatterino = nullptr;
        if (!source.isEmpty())
        {
            box.setText(QStringLiteral("Bring your Chatterino setup over?"));
            box.setInformativeText(
                QStringLiteral(
                    "Chatterino was found at\n%1\n\nIts tabs, highlights, "
                    "commands, themes and plugins can be copied across so you "
                    "start out where you left off. Chat logs are left behind. "
                    "Chatterino itself is only read from - nothing is moved or "
                    "deleted, and from here on the two keep their own "
                    "settings.\n\nMoving over from ChattiFlexii on another "
                    "computer? Import the folder made under Settings > Export "
                    "& Import there, and everything is exactly as it was.")
                    .arg(QDir::toNativeSeparators(source)));
            copyChatterino = box.addButton(QStringLiteral("Copy My Setup"),
                                           QMessageBox::AcceptRole);
        }
        else
        {
            box.setText(QStringLiteral("Moving over from another computer?"));
            box.setInformativeText(QStringLiteral(
                "Import the folder made under Settings > Export & Import in "
                "ChattiFlexii on the other computer, and everything is exactly "
                "as it was there."));
        }
        auto *importExport = box.addButton(
            QStringLiteral("Import Export Folder..."), QMessageBox::ActionRole);
        box.addButton(QStringLiteral("Start Fresh"), QMessageBox::RejectRole);
        box.setDefaultButton(copyChatterino != nullptr ? copyChatterino
                                                       : importExport);
        box.exec();

        if (box.clickedButton() == importExport)
        {
            const auto folder = askForExportFolder();
            if (folder.isEmpty())
            {
                continue;
            }

            QString error;
            if (!stageProfileImport(paths, folder, error))
            {
                QMessageBox::warning(nullptr, QStringLiteral("ChattiFlexii"),
                                     error);
                continue;
            }
            // Nothing has read the settings yet, so it applies right away
            applyPendingImport(paths);
            return;
        }

        if (copyChatterino != nullptr && box.clickedButton() == copyChatterino)
        {
            copyChatterinoProfile(paths, source);
            return;
        }

        QFile marker(settings.absoluteFilePath(DECLINED_MARKER));
        marker.open(QIODevice::WriteOnly);
        return;
    }
}

QString exportProfile(const Paths &paths, bool includeLogin, QString &error)
{
    const QDir desktop(
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    const auto stamp = QDateTime::currentDateTime().toString(
        QStringLiteral("yyyy-MM-dd HH-mm"));
    auto folder =
        desktop.absoluteFilePath(QStringLiteral("ChattiFlexii-Export %1").arg(stamp));
    for (int n = 2; QFileInfo::exists(folder); n++)
    {
        folder = desktop.absoluteFilePath(
            QStringLiteral("ChattiFlexii-Export %1 (%2)").arg(stamp).arg(n));
    }
    if (!QDir().mkpath(folder))
    {
        error = QStringLiteral("Der Ordner %1 konnte nicht angelegt werden.")
                    .arg(QDir::toNativeSeparators(folder));
        return {};
    }

    // The rolling backups and our own marker stay behind, and so do stored
    // credentials unless the login is meant to travel
    const auto skip = [includeLogin](const QFileInfo &entry) {
        const auto name = entry.fileName();
        return name.contains(QStringLiteral(".bkp-")) ||
               name == DECLINED_MARKER ||
               (!includeLogin && name.startsWith(QStringLiteral("credentials")));
    };

    const QDir root(paths.rootAppDataDirectory);
    bool ok = true;
    for (const auto &part : PROFILE_CONTENTS)
    {
        const auto from = root.absoluteFilePath(part);
        if (!QFileInfo::exists(from))
        {
            continue;
        }
        ok = copyTree(from, QDir(folder).absoluteFilePath(part), skip) && ok;
    }

    if (!includeLogin)
    {
        const auto copied =
            QDir(folder).absoluteFilePath(QStringLiteral("Settings/settings.json"));
        auto settings = readJsonObject(copied);
        if (settings.contains(QStringLiteral("accounts")))
        {
            settings.remove(QStringLiteral("accounts"));
            ok = writeJsonObject(copied, settings) && ok;
        }
    }

    const QJsonObject marker{
        {QStringLiteral("app"), QStringLiteral("ChattiFlexii")},
        {QStringLiteral("format"), 1},
        {QStringLiteral("created"),
         QDateTime::currentDateTime().toString(Qt::ISODate)},
        {QStringLiteral("includesLogin"), includeLogin},
    };
    ok = writeJsonObject(QDir(folder).absoluteFilePath(EXPORT_MARKER), marker) &&
         ok;

    if (!ok)
    {
        // Half an export would import as a broken setup
        QDir(folder).removeRecursively();
        error = QStringLiteral("Nicht alles konnte kopiert werden - der Export "
                               "wurde abgebrochen.");
        return {};
    }
    return folder;
}

bool isProfileExport(const QString &folder)
{
    return QFileInfo::exists(QDir(folder).absoluteFilePath(EXPORT_MARKER));
}

bool stageProfileImport(const Paths &paths, const QString &folder,
                        QString &error)
{
    const auto pending =
        QDir(paths.rootAppDataDirectory).absoluteFilePath(PENDING_IMPORT);
    // One chosen earlier that never got applied gives way
    QDir(pending).removeRecursively();

    if (!copyTree(folder, pending))
    {
        QDir(pending).removeRecursively();
        error = QStringLiteral("Der Export konnte nicht übernommen werden.");
        return false;
    }
    return true;
}

void applyPendingImport(const Paths &paths)
{
    const QDir root(paths.rootAppDataDirectory);
    const auto pending = root.absoluteFilePath(PENDING_IMPORT);
    if (!QFileInfo::exists(pending))
    {
        return;
    }
    if (!isProfileExport(pending))
    {
        qCWarning(chatterinoApp) << "Dropping an import that is no export";
        QDir(pending).removeRecursively();
        return;
    }

    const auto backup = root.absoluteFilePath(
        QStringLiteral("Backup before import %1")
            .arg(QDateTime::currentDateTime().toString(
                QStringLiteral("yyyy-MM-dd HH-mm-ss"))));
    QDir().mkpath(backup);

    // The login this computer already has, kept when the export brings none
    const auto accounts =
        readJsonObject(QDir(paths.settingsDirectory)
                           .absoluteFilePath(QStringLiteral("settings.json")))
            .value(QStringLiteral("accounts"))
            .toObject();

    for (const auto &part : PROFILE_CONTENTS)
    {
        const auto incoming = QDir(pending).absoluteFilePath(part);
        if (!QFileInfo::exists(incoming))
        {
            continue;  // the export has none, so ours stays
        }

        const auto current = root.absoluteFilePath(part);
        if (QFileInfo::exists(current) &&
            !moveTree(current, QDir(backup).absoluteFilePath(part)))
        {
            qCWarning(chatterinoApp)
                << "Could not set aside" << current << "- keeping it";
            continue;
        }
        if (!moveTree(incoming, current))
        {
            qCWarning(chatterinoApp) << "Could not move in" << incoming;
        }
    }

    const QDir settingsDir(paths.settingsDirectory);
    const auto settingsPath =
        settingsDir.absoluteFilePath(QStringLiteral("settings.json"));
    auto settings = readJsonObject(settingsPath);
    if (settings.value(QStringLiteral("accounts")).toObject().isEmpty() &&
        !accounts.isEmpty())
    {
        settings.insert(QStringLiteral("accounts"), accounts);
        writeJsonObject(settingsPath, settings);
    }

    const auto credentials =
        settingsDir.absoluteFilePath(QStringLiteral("credentials.json"));
    const auto keptCredentials = QDir(backup).absoluteFilePath(
        QStringLiteral("Settings/credentials.json"));
    if (!QFileInfo::exists(credentials) && QFileInfo::exists(keptCredentials))
    {
        copyFile(keptCredentials, credentials);
    }

    QDir(pending).removeRecursively();
    // Nothing set aside leaves an empty backup, which only gets in the way
    QDir().rmdir(backup);
    qCInfo(chatterinoApp) << "Imported a ChattiFlexii export; the settings it "
                             "replaced are in"
                          << backup;
}

bool relaunchAfterExit()
{
    const auto pid = QString::number(QCoreApplication::applicationPid());
#if defined(Q_OS_MACOS)
    // The bundle rather than the binary inside it, or macOS shows a second
    // icon in the dock
    const auto bundle =
        QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../.."))
            .absolutePath();
    return QProcess::startDetached(
        QStringLiteral("/bin/sh"),
        {QStringLiteral("-c"),
         QStringLiteral("while kill -0 %1 2>/dev/null; do sleep 0.2; done; "
                        "open -n \"$0\"")
             .arg(pid),
         bundle});
#elif defined(Q_OS_WIN)
    auto exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    exe.replace(u'\'', QStringLiteral("''"));
    return QProcess::startDetached(
        QStringLiteral("powershell"),
        {QStringLiteral("-NoProfile"), QStringLiteral("-WindowStyle"),
         QStringLiteral("Hidden"), QStringLiteral("-Command"),
         QStringLiteral("Wait-Process -Id %1 -ErrorAction SilentlyContinue; "
                        "Start-Process -FilePath '%2'")
             .arg(pid, exe)});
#else
    return QProcess::startDetached(
        QStringLiteral("/bin/sh"),
        {QStringLiteral("-c"),
         QStringLiteral("while kill -0 %1 2>/dev/null; do sleep 0.2; done; "
                        "exec \"$0\"")
             .arg(pid),
         QCoreApplication::applicationFilePath()});
#endif
}

}  // namespace chatterino
