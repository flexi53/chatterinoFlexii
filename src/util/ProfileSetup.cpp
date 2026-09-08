// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/ProfileSetup.hpp"

#include "common/QLogging.hpp"
#include "singletons/Paths.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QStringBuilder>

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
/// Existing files are left as they are.
bool copyTree(const QString &from, const QString &to)
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
        const auto target = QDir(to).absoluteFilePath(entry.fileName());

        if (entry.isDir())
        {
            ok = copyTree(entry.absoluteFilePath(), target) && ok;
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
    if (source.isEmpty())
    {
        return;
    }

    QMessageBox box;
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Welcome to ChattiFlexii"));
    box.setText(QStringLiteral("Bring your Chatterino setup over?"));
    box.setInformativeText(
        QStringLiteral(
            "Chatterino was found at\n%1\n\nIts tabs, highlights, commands, "
            "themes and plugins can be copied across so you start out where "
            "you left off. Chat logs are left behind.\n\nChatterino itself is "
            "only read from - nothing is moved or deleted, and from here on "
            "the two keep their own settings.")
            .arg(QDir::toNativeSeparators(source)));

    auto *accept =
        box.addButton(QStringLiteral("Copy My Setup"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Start Fresh"), QMessageBox::RejectRole);
    box.setDefaultButton(accept);
    box.exec();

    if (box.clickedButton() != accept)
    {
        QFile marker(settings.absoluteFilePath(DECLINED_MARKER));
        marker.open(QIODevice::WriteOnly);
        return;
    }

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

}  // namespace chatterino
