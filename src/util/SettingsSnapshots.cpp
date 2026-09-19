// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/SettingsSnapshots.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/ProfileSetup.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>

namespace chatterino::snapshots {

namespace {

const QString FOLDER = QStringLiteral("Snapshots");
const QString PENDING = QStringLiteral(".pending.json");

/// What belongs to this computer, not to a view
const std::vector<QStringList> LOCAL{
    {"accounts"},
    {"sync"},
    {"update"},
    {"snapshots"},
    {"backup", "last"},
    {"misc", "lastSeenChanges"},
    {"moderation", "alerts", "x"},
    {"moderation", "alerts", "y"},
    {"moderation", "alerts", "positionSaved"},
    {"appearance", "lastPopup"},
};

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
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) &&
           file.write(QJsonDocument(object).toJson()) > 0 && file.commit();
}

QJsonValue valueAt(const QJsonObject &object, const QStringList &path)
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
    if (inner.isEmpty())
    {
        object.remove(key);
    }
    else
    {
        object.insert(key, inner);
    }
}

QString folder(const QString &settingsDirectory)
{
    return QDir(settingsDirectory).absoluteFilePath(FOLDER);
}

/// The file of the view @a name - an existing one, or where a new one goes
QString fileFor(const QString &settingsDirectory, const QString &name)
{
    const QDir dir(folder(settingsDirectory));
    for (const auto &entry :
         dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files))
    {
        if (readJson(entry.absoluteFilePath()).value("name").toString() == name)
        {
            return entry.absoluteFilePath();
        }
    }

    static const QRegularExpression unsafe(
        QStringLiteral("[^\\w\\- ]"),
        QRegularExpression::UseUnicodePropertiesOption);
    auto base = name;
    base.replace(unsafe, QStringLiteral("_"));
    base = base.trimmed();
    if (base.isEmpty())
    {
        base = QStringLiteral("Ansicht");
    }
    auto path = dir.absoluteFilePath(base + ".json");
    for (int n = 2; QFileInfo::exists(path); n++)
    {
        path = dir.absoluteFilePath(
            QStringLiteral("%1 (%2).json").arg(base).arg(n));
    }
    return path;
}

QJsonObject withoutLocal(QJsonObject settings)
{
    for (const auto &path : LOCAL)
    {
        setAt(settings, path, QJsonValue::Undefined);
    }
    return settings;
}

}  // namespace

std::vector<View> list(const QString &settingsDirectory)
{
    std::vector<View> views;
    const QDir dir(folder(settingsDirectory));
    for (const auto &entry :
         dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files))
    {
        const auto content = readJson(entry.absoluteFilePath());
        const auto name = content.value("name").toString();
        if (!name.isEmpty())
        {
            views.push_back({
                .name = name,
                .saved = QDateTime::fromString(
                    content.value("saved").toString(), Qt::ISODate),
            });
        }
    }
    std::sort(views.begin(), views.end(), [](const auto &a, const auto &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return views;
}

bool save(const QString &settingsDirectory, const QString &name, QString &error)
{
    if (name.trimmed().isEmpty())
    {
        error = QStringLiteral("Die Ansicht braucht einen Namen.");
        return false;
    }
    if (!QDir().mkpath(folder(settingsDirectory)))
    {
        error = QStringLiteral("Der Ordner für Ansichten ließ sich nicht "
                               "anlegen.");
        return false;
    }
    const auto settings =
        readJson(QDir(settingsDirectory)
                     .absoluteFilePath(QStringLiteral("settings.json")));
    if (!writeJson(fileFor(settingsDirectory, name.trimmed()),
                   {
                       {"name", name.trimmed()},
                       {"saved",
                        QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                       {"settings", withoutLocal(settings)},
                   }))
    {
        error = QStringLiteral("Die Ansicht ließ sich nicht speichern.");
        return false;
    }
    return true;
}

bool remove(const QString &settingsDirectory, const QString &name)
{
    const auto path = fileFor(settingsDirectory, name);
    return QFileInfo::exists(path) && QFile::remove(path);
}

bool rename(const QString &settingsDirectory, const QString &from,
            const QString &to, QString &error)
{
    const auto path = fileFor(settingsDirectory, from);
    auto content = readJson(path);
    if (content.isEmpty())
    {
        error = QStringLiteral("Die Ansicht gibt es nicht mehr.");
        return false;
    }
    if (to.trimmed().isEmpty())
    {
        error = QStringLiteral("Die Ansicht braucht einen Namen.");
        return false;
    }
    content.insert("name", to.trimmed());
    if (!writeJson(path, content))
    {
        error = QStringLiteral("Die Ansicht ließ sich nicht umbenennen.");
        return false;
    }
    return true;
}

bool stage(const QString &settingsDirectory, const QString &name,
           QString &error)
{
    const auto view = readJson(fileFor(settingsDirectory, name));
    if (!view.contains("settings"))
    {
        error = QStringLiteral("Die Ansicht gibt es nicht mehr.");
        return false;
    }
    // A way back to what is set up now
    if (name != BEFORE_SWITCH && !save(settingsDirectory, BEFORE_SWITCH, error))
    {
        return false;
    }
    if (!writeJson(QDir(folder(settingsDirectory)).absoluteFilePath(PENDING),
                   view))
    {
        error = QStringLiteral("Der Wechsel ließ sich nicht vorbereiten.");
        return false;
    }
    return true;
}

void applyPending(const QString &settingsDirectory)
{
    const auto pendingFile =
        QDir(folder(settingsDirectory)).absoluteFilePath(PENDING);
    if (!QFileInfo::exists(pendingFile))
    {
        return;
    }
    const auto view = readJson(pendingFile);
    QFile::remove(pendingFile);

    const auto settingsFile =
        QDir(settingsDirectory)
            .absoluteFilePath(QStringLiteral("settings.json"));
    auto result =
        merged(view.value("settings").toObject(), readJson(settingsFile));
    setAt(result, {"snapshots", "current"}, view.value("name"));
    if (!writeJson(settingsFile, result))
    {
        qCWarning(chatterinoApp)
            << "Could not switch to the view" << view.value("name").toString();
    }
}

QJsonObject merged(const QJsonObject &view, const QJsonObject &current)
{
    auto result = withoutLocal(view);
    for (const auto &path : LOCAL)
    {
        setAt(result, path, valueAt(current, path));
    }
    return result;
}

void switchTo(const QString &name, QWidget *parent)
{
    const auto answer = QMessageBox::question(
        parent, QStringLiteral("Ansicht wechseln"),
        QStringLiteral("Zur Ansicht „%1“ wechseln? ChattiFlexii startet dafür "
                       "kurz neu. Was du jetzt eingestellt hast, bleibt als "
                       "„%2“ erhalten.")
            .arg(name, BEFORE_SWITCH));
    if (answer != QMessageBox::Yes)
    {
        return;
    }

    getSettings()->requestSave();
    getApp()->getWindows()->save();

    QString error;
    if (!stage(getApp()->getPaths().settingsDirectory, name, error))
    {
        QMessageBox::warning(parent, QStringLiteral("Ansicht wechseln"), error);
        return;
    }
    if (!relaunchAfterExit())
    {
        QMessageBox::information(
            parent, QStringLiteral("Ansicht wechseln"),
            QStringLiteral(
                "Der Wechsel ist vorbereitet. ChattiFlexii beendet "
                "sich jetzt - starte es danach bitte selbst wieder."));
    }
    QApplication::quit();
}

}  // namespace chatterino::snapshots
