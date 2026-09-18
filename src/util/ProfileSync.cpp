// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/ProfileSync.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/AutoBackup.hpp"
#include "util/ProfileSetup.hpp"
#include "widgets/Window.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QSysInfo>
#include <QTimer>

#include <array>
#include <chrono>
#include <vector>

namespace chatterino::profilesync {

namespace {

constexpr auto FIRST_CHECK = std::chrono::seconds(20);
constexpr auto CHECK_EVERY = std::chrono::minutes(30);

const QString SHARED_NAME = QStringLiteral("ChattiFlexii-Abgleich");
/// What a new shared setup is called until it is complete and takes the old
/// one's place
const QString FRESH_PREFIX = QStringLiteral("ChattiFlexii-Abgleich (neu)");
/// Written into the shared setup, next to the export's own marker
const QString SHARED_MARKER = QStringLiteral("chattiflexii-abgleich.json");
/// How fingerprint() works - a setup from a version that worked it out
/// otherwise cannot be checked against it
constexpr int FORMAT = 2;

/// Setup offers already answered with "Später" since the app started
QSet<QString> &putOff()
{
    static QSet<QString> offers;
    return offers;
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeJson(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) &&
           file.write(QJsonDocument(object).toJson()) > 0;
}

/// Removes the value at @a path, nested objects and all, if it is there
void removePath(QJsonObject &object, QStringList path)
{
    if (path.isEmpty())
    {
        return;
    }
    const auto key = path.takeFirst();
    if (path.isEmpty())
    {
        object.remove(key);
        return;
    }
    if (!object.value(key).isObject())
    {
        return;
    }
    auto inner = object.value(key).toObject();
    removePath(inner, path);
    object.insert(key, inner);
}

/// The value at @a path, or an undefined one
QJsonValue valueAt(const QJsonObject &object, QStringList path)
{
    QJsonValue value = object;
    for (const auto &key : path)
    {
        if (!value.isObject())
        {
            return QJsonValue::Undefined;
        }
        value = value.toObject().value(key);
    }
    return value;
}

/// Sets the value at @a path, making the objects on the way as needed - or
/// removes it, for an undefined @a value
void setAt(QJsonObject &object, QStringList path, const QJsonValue &value)
{
    if (path.isEmpty())
    {
        return;
    }
    const auto key = path.takeFirst();
    if (path.isEmpty())
    {
        if (value.isUndefined())
        {
            object.remove(key);
        }
        else
        {
            object.insert(key, value);
        }
        return;
    }
    auto inner = object.value(key).toObject();
    setAt(inner, path, value);
    object.insert(key, inner);
}

/// What says where a window sits and how big it is, in window-layout.json
const std::array<QLatin1String, 6> WINDOW_PLACE{
    QLatin1String("x"),     QLatin1String("y"),
    QLatin1String("width"), QLatin1String("height"),
    QLatin1String("state"), QLatin1String("emotePopup"),
};

/// The same for the alert windows and popups, in settings.json
const std::vector<QStringList> SETTINGS_PLACES{
    {"moderation", "alerts", "x"},
    {"moderation", "alerts", "y"},
    {"moderation", "alerts", "width"},
    {"moderation", "alerts", "height"},
    {"moderation", "alerts", "positionSaved"},
    {"appearance", "lastPopup"},
};

QString rootDirectory()
{
    return getApp()->getPaths().rootAppDataDirectory;
}

/// Leaves this computer's setup in the folder, replacing what is there
bool write(QString &error)
{
    getSettings()->requestSave();
    getApp()->getWindows()->save();

    const auto parent = autobackup::folder();
    if (!QDir().mkpath(parent))
    {
        error = QStringLiteral("Der Ordner %1 konnte nicht angelegt werden.")
                    .arg(QDir::toNativeSeparators(parent));
        return false;
    }
    const QDir dir(parent);

    // Leftovers of a write that did not finish
    for (const auto &entry : dir.entryInfoList(
             {FRESH_PREFIX + u'*'}, QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QDir(entry.absoluteFilePath()).removeRecursively();
    }

    const auto fresh =
        exportProfileTo(rootDirectory(), parent, false, error, FRESH_PREFIX);
    if (fresh.isEmpty())
    {
        return false;
    }

    const auto written =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const auto print = fingerprint(fresh);

    // The setup carries which one it is, so a computer that takes it knows
    // it has - and does not offer it again, or write it straight back
    const auto settingsFile =
        QDir(fresh).absoluteFilePath(QStringLiteral("Settings/settings.json"));
    auto values = readJson(settingsFile);
    auto sync = values.value(QStringLiteral("sync")).toObject();
    sync.insert(QStringLiteral("base"), written);
    sync.insert(QStringLiteral("written"), print);
    values.insert(QStringLiteral("sync"), sync);

    if (!writeJson(settingsFile, values) ||
        !writeJson(QDir(fresh).absoluteFilePath(SHARED_MARKER),
                   {
                       {QStringLiteral("computer"), thisComputer()},
                       {QStringLiteral("computerName"), thisComputerName()},
                       {QStringLiteral("written"), written},
                       {QStringLiteral("fingerprint"), print},
                       {QStringLiteral("format"), FORMAT},
                   }))
    {
        QDir(fresh).removeRecursively();
        error = QStringLiteral("Der Abgleich konnte nicht geschrieben werden.");
        return false;
    }

    // Complete before it takes the old one's place, so the other computer
    // never finds half a setup under the real name
    const auto kept = dir.absoluteFilePath(SHARED_NAME);
    if (QFileInfo::exists(kept) && !QDir(kept).removeRecursively())
    {
        QDir(fresh).removeRecursively();
        error = QStringLiteral("Der vorige Abgleich ließ sich nicht ersetzen.");
        return false;
    }
    if (!QDir().rename(fresh, kept))
    {
        QDir(fresh).removeRecursively();
        error = QStringLiteral("Der Abgleich ließ sich nicht umbenennen.");
        return false;
    }

    getSettings()->profileSyncBase.setValue(written);
    getSettings()->profileSyncWritten.setValue(print);
    return true;
}

QString describe(const Shared &shared)
{
    const auto when =
        QDateTime::fromString(shared.written, Qt::ISODateWithMs).toLocalTime();
    return QStringLiteral("„%1“ (%2)")
        .arg(shared.computerName,
             QLocale(QLocale::German)
                 .toString(when, QStringLiteral("d. MMMM, HH:mm")));
}

/// Takes the shared setup: it replaces this one at a restart, this one is
/// backed up first
void take(const Shared &shared)
{
    QString error;
    // Only checked when it was worked out the same way - one from another
    // version is taken as it is
    if (shared.format == FORMAT &&
        fingerprint(sharedFolder()) != shared.fingerprint)
    {
        QMessageBox::information(
            &getApp()->getWindows()->getMainWindow(),
            QStringLiteral("Abgleich"),
            QStringLiteral("Die Einstellungen vom anderen Computer sind noch "
                           "nicht ganz angekommen - iCloud lädt sie noch. "
                           "ChattiFlexii fragt gleich noch einmal."));
        return;
    }
    if (!stageProfileImport(getApp()->getPaths(), sharedFolder(), error))
    {
        QMessageBox::warning(&getApp()->getWindows()->getMainWindow(),
                             QStringLiteral("Abgleich"), error);
        return;
    }
    // The windows stay where they are on this computer's screens
    getSettings()->requestSave();
    getApp()->getWindows()->save();
    keepWindowPlaces(rootDirectory(), pendingImportFolder(rootDirectory()));
    if (!relaunchAfterExit())
    {
        QMessageBox::information(
            &getApp()->getWindows()->getMainWindow(),
            QStringLiteral("Abgleich"),
            QStringLiteral("Die Einstellungen sind vorbereitet. ChattiFlexii "
                           "beendet sich jetzt - starte es danach bitte selbst "
                           "wieder."));
    }
    QApplication::quit();
}

void offer(const Shared &shared)
{
    static QPointer<QMessageBox> open;
    if (open)
    {
        return;
    }

    auto *box = new QMessageBox(&getApp()->getWindows()->getMainWindow());
    open = box;
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setIcon(QMessageBox::Question);
    box->setWindowTitle(QStringLiteral("Abgleich"));
    box->setText(QStringLiteral("Auf %1 gibt es neuere Einstellungen.")
                     .arg(describe(shared)));
    box->setInformativeText(QStringLiteral(
        "Übernehmen: ChattiFlexii startet neu und ist danach eingerichtet wie "
        "dort. Deine jetzigen Einstellungen werden vorher gesichert, der "
        "Twitch-Login bleibt.\n\nMeine behalten: Deine Einstellungen hier "
        "ersetzen die vom anderen Computer."));
    auto *takeButton =
        box->addButton(QStringLiteral("Übernehmen"), QMessageBox::AcceptRole);
    auto *keep = box->addButton(QStringLiteral("Meine behalten"),
                                QMessageBox::DestructiveRole);
    box->addButton(QStringLiteral("Später"), QMessageBox::RejectRole);
    box->setDefaultButton(takeButton);

    QObject::connect(
        box, &QMessageBox::buttonClicked, box,
        [takeButton, keep, shared](QAbstractButton *clicked) {
            if (clicked == takeButton)
            {
                // After the box is gone, as taking quits the app
                QTimer::singleShot(0, [shared] {
                    take(shared);
                });
            }
            else if (clicked == keep)
            {
                getSettings()->profileSyncBase.setValue(shared.written);
                QString error;
                if (!write(error))
                {
                    qCWarning(chatterinoApp) << "Sync write failed:" << error;
                }
            }
            else
            {
                putOff().insert(shared.written);
            }
        });
    box->open();
}

}  // namespace

Step decide(const std::optional<Shared> &shared, const QString &computer,
            const QString &base, const QString &fingerprintNow,
            const QString &fingerprintWritten)
{
    if (shared && shared->computer != computer && shared->written != base)
    {
        return shared->fingerprint == fingerprintNow ? Step::Settle
                                                     : Step::Offer;
    }
    return fingerprintNow != fingerprintWritten ? Step::Write : Step::Nothing;
}

QString fingerprint(const QString &rootDirectory)
{
    const QDir settings(QDir(rootDirectory).absoluteFilePath("Settings"));

    auto values = readJson(settings.absoluteFilePath("settings.json"));
    // The login stays on each computer, and these change by themselves
    values.remove(QStringLiteral("accounts"));
    values.remove(QStringLiteral("sync"));
    values.remove(QStringLiteral("update"));
    removePath(values, {"backup", "last"});
    removePath(values, {"misc", "lastSeenChanges"});
    // Where windows sit stays with each computer - see keepWindowPlaces
    for (const auto &place : SETTINGS_PLACES)
    {
        removePath(values, place);
    }

    auto layout = readJson(settings.absoluteFilePath("window-layout.json"));
    QJsonArray windows;
    for (const auto &value : layout.value(QStringLiteral("windows")).toArray())
    {
        auto window = value.toObject();
        for (const auto &key : WINDOW_PLACE)
        {
            window.remove(key);
        }
        windows.append(window);
    }
    layout.insert(QStringLiteral("windows"), windows);

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QJsonDocument(values).toJson(QJsonDocument::Compact));
    hash.addData(QJsonDocument(layout).toJson(QJsonDocument::Compact));
    return QString::fromLatin1(hash.result().toHex());
}

QString thisComputer()
{
    // Lets the sync be tried out with two copies on one computer
    if (const auto forced = qEnvironmentVariable("CHATTIFLEXII_SYNC_COMPUTER");
        !forced.isEmpty())
    {
        return forced;
    }
    const auto id = QSysInfo::machineUniqueId();
    return id.isEmpty() ? QSysInfo::machineHostName() : QString::fromLatin1(id);
}

QString thisComputerName()
{
    if (const auto forced = qEnvironmentVariable("CHATTIFLEXII_SYNC_COMPUTER");
        !forced.isEmpty())
    {
        return forced;
    }
#ifdef Q_OS_MACOS
    // "Mac mini von Felix" rather than the host name, which is often just
    // "Mac"
    static const QString computerName = [] {
        QProcess scutil;
        scutil.start(QStringLiteral("/usr/sbin/scutil"),
                     {QStringLiteral("--get"), QStringLiteral("ComputerName")});
        if (scutil.waitForFinished(2000) && scutil.exitCode() == 0)
        {
            return QString::fromUtf8(scutil.readAllStandardOutput()).trimmed();
        }
        return QString();
    }();
    if (!computerName.isEmpty())
    {
        return computerName;
    }
#endif
    auto name = QSysInfo::machineHostName();
    if (name.endsWith(QStringLiteral(".local")))
    {
        name.chop(6);
    }
    return name.isEmpty() ? QStringLiteral("einem anderen Computer") : name;
}

QString sharedFolder()
{
    return QDir(autobackup::folder()).absoluteFilePath(SHARED_NAME);
}

std::optional<Shared> readShared(const QString &folder)
{
    const auto marker = readJson(QDir(folder).absoluteFilePath(SHARED_MARKER));
    const auto written = marker.value(QStringLiteral("written")).toString();
    if (written.isEmpty() || !isProfileExport(folder))
    {
        return std::nullopt;
    }
    return Shared{
        .computer = marker.value(QStringLiteral("computer")).toString(),
        .computerName = marker.value(QStringLiteral("computerName")).toString(),
        .written = written,
        .fingerprint = marker.value(QStringLiteral("fingerprint")).toString(),
        .format = marker.value(QStringLiteral("format")).toInt(1),
    };
}

void keepWindowPlaces(const QString &localRoot, const QString &stagedRoot)
{
    const QDir local(QDir(localRoot).absoluteFilePath("Settings"));
    const QDir staged(QDir(stagedRoot).absoluteFilePath("Settings"));

    // The main window takes the main window's place here, and popups the
    // places of the popups here in turn. A window with no counterpart here
    // keeps the place it came with.
    const auto layoutFile = staged.absoluteFilePath("window-layout.json");
    auto layout = readJson(layoutFile);
    const auto here = readJson(local.absoluteFilePath("window-layout.json"))
                          .value(QStringLiteral("windows"))
                          .toArray();
    QJsonArray windows;
    QHash<QString, int> seen;
    for (const auto &value : layout.value(QStringLiteral("windows")).toArray())
    {
        auto window = value.toObject();
        const auto type = window.value(QStringLiteral("type")).toString();
        const auto nth = seen[type]++;
        int count = 0;
        for (const auto &hereValue : here)
        {
            const auto hereWindow = hereValue.toObject();
            if (hereWindow.value(QStringLiteral("type")).toString() != type ||
                count++ != nth)
            {
                continue;
            }
            for (const auto &key : WINDOW_PLACE)
            {
                if (hereWindow.contains(key))
                {
                    window.insert(key, hereWindow.value(key));
                }
                else
                {
                    window.remove(key);
                }
            }
            break;
        }
        windows.append(window);
    }
    if (!layout.isEmpty())
    {
        layout.insert(QStringLiteral("windows"), windows);
        writeJson(layoutFile, layout);
    }

    const auto settingsFile = staged.absoluteFilePath("settings.json");
    auto values = readJson(settingsFile);
    const auto hereValues = readJson(local.absoluteFilePath("settings.json"));
    for (const auto &place : SETTINGS_PLACES)
    {
        setAt(values, place, valueAt(hereValues, place));
    }
    writeJson(settingsFile, values);
}

QString syncNow(bool fromUser)
{
    if (!getSettings()->profileSyncEnabled.getValue())
    {
        return QStringLiteral("Der Abgleich ist aus.");
    }

    // What is on disk is what gets compared and written
    getSettings()->requestSave();
    getApp()->getWindows()->save();

    const auto shared = readShared(sharedFolder());
    auto &s = *getSettings();
    switch (decide(shared, thisComputer(), s.profileSyncBase.getValue(),
                   fingerprint(rootDirectory()),
                   s.profileSyncWritten.getValue()))
    {
        case Step::Offer:
            if (fromUser || !putOff().contains(shared->written))
            {
                offer(*shared);
            }
            return QStringLiteral("Neuere Einstellungen von %1 warten.")
                .arg(describe(*shared));

        case Step::Settle:
            s.profileSyncBase.setValue(shared->written);
            s.profileSyncWritten.setValue(shared->fingerprint);
            return QStringLiteral("Alles gleich wie auf %1.")
                .arg(describe(*shared));

        case Step::Write: {
            QString error;
            if (!write(error))
            {
                qCWarning(chatterinoApp) << "Sync write failed:" << error;
                return error;
            }
            return QStringLiteral(
                "Deine Einstellungen liegen jetzt im Ordner.");
        }

        case Step::Nothing:
        default:
            return QStringLiteral("Alles abgeglichen.");
    }
}

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
        syncNow(false);
    });
    auto *timer = new QTimer(context);
    timer->setInterval(CHECK_EVERY);
    QObject::connect(timer, &QTimer::timeout, context, [] {
        syncNow(false);
    });
    timer->start();

    // The last changes go along on the way out - unless another computer's
    // setup is waiting, which is then still offered at the next start
    QObject::connect(
        QCoreApplication::instance(), &QCoreApplication::aboutToQuit, context,
        [] {
            if (!getSettings()->profileSyncEnabled.getValue())
            {
                return;
            }
            const auto shared = readShared(sharedFolder());
            auto &s = *getSettings();
            getSettings()->requestSave();
            if (decide(shared, thisComputer(), s.profileSyncBase.getValue(),
                       fingerprint(rootDirectory()),
                       s.profileSyncWritten.getValue()) == Step::Write)
            {
                QString error;
                if (!write(error))
                {
                    qCWarning(chatterinoApp)
                        << "Sync write on exit failed:" << error;
                }
            }
        });

    // Switching it on looks in a moment, not half an hour later. Never
    // destroyed, so nothing is torn down after the settings at exit.
    auto *connections = new pajlada::Signals::SignalHolder;
    getSettings()->profileSyncEnabled.connect(
        [context](const bool &on, auto) {
            if (on)
            {
                QTimer::singleShot(std::chrono::seconds(3), context, [] {
                    syncNow(false);
                });
            }
        },
        *connections, false);
}

}  // namespace chatterino::profilesync
