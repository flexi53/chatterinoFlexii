// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/SelfUpdate.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/UpdateCheck.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace chatterino::selfupdate {

namespace {

const QString APP_NAME = QStringLiteral("ChattiFlexii.app");
/// Where the new app waits next to the running one, hidden, until the swap
const QString STAGING_NAME = QStringLiteral(".ChattiFlexii-update.app");

bool run(const QString &program, const QStringList &arguments, QString &output,
         int timeoutMs = 120000)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    if (!process.waitForFinished(timeoutMs))
    {
        process.kill();
        output = QStringLiteral("%1 hat nicht geantwortet.").arg(program);
        return false;
    }
    output = QString::fromUtf8(process.readAll()).trimmed();
    return process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
}

void fail(QWidget *parent, const QString &reason)
{
    QMessageBox::warning(
        parent, QStringLiteral("Update"),
        QStringLiteral("Das Update hat nicht geklappt: %1\n\nChattiFlexii "
                       "bleibt, wie es ist. Über „Herunterladen“ kannst du die "
                       "neue Version auch von Hand installieren.")
            .arg(reason));
}

QString downloadFile()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .absoluteFilePath(QStringLiteral("ChattiFlexii-update.dmg"));
}

/// Readies the downloaded image next to the app and restarts into it
void finish(const QString &diskImage, QWidget *parent)
{
    const auto bundle = bundlePath();
    const auto staging = QFileInfo(bundle).dir().absoluteFilePath(STAGING_NAME);

    QString error;
    if (!stageFromDiskImage(diskImage, staging, error))
    {
        QFile::remove(diskImage);
        fail(parent, error);
        return;
    }
    QFile::remove(diskImage);

    // What is set up now goes along - the settings are kept anyway, but the
    // last minutes of them are only written on the way out
    getSettings()->requestSave();
    getApp()->getWindows()->save();

    // Trying the update out on a copy, the new version is not opened
    const auto open = qEnvironmentVariableIsSet("CHATTIFLEXII_UPDATE_NOOPEN")
                          ? QStringLiteral("noopen")
                          : QStringLiteral("open");
    if (!QProcess::startDetached(
            QStringLiteral("/bin/sh"),
            {QStringLiteral("-c"), swapScript(),
             QStringLiteral("chattiflexii-update"),
             QString::number(QCoreApplication::applicationPid()), staging,
             bundle, open}))
    {
        QDir(staging).removeRecursively();
        fail(parent,
             QStringLiteral("Der Neustart ließ sich nicht vorbereiten."));
        return;
    }
    QApplication::quit();
}

}  // namespace

QString bundlePath()
{
#ifdef Q_OS_MACOS
    // .../ChattiFlexii.app/Contents/MacOS
    QDir dir(QCoreApplication::applicationDirPath());
    if (!dir.cdUp() || !dir.cdUp())
    {
        return {};
    }
    const auto path = dir.absolutePath();
    return path.endsWith(QStringLiteral(".app")) ? path : QString();
#else
    return {};
#endif
}

bool canInstall()
{
#ifdef Q_OS_MACOS
    if (!updatecheck::canCheck())
    {
        return false;
    }
    const auto bundle = bundlePath();
    if (bundle.isEmpty())
    {
        return false;
    }
    return QFileInfo(QFileInfo(bundle).absolutePath()).isWritable() &&
           QFileInfo(bundle).isWritable();
#else
    return false;
#endif
}

bool stageFromDiskImage(const QString &diskImage, const QString &staging,
                        QString &error)
{
    QTemporaryDir mount;
    if (!mount.isValid())
    {
        error = QStringLiteral("Kein Platz für das Update gefunden.");
        return false;
    }

    QString output;
    if (!run(QStringLiteral("/usr/bin/hdiutil"),
             {QStringLiteral("attach"), QStringLiteral("-nobrowse"),
              QStringLiteral("-readonly"), QStringLiteral("-mountpoint"),
              mount.path(), diskImage},
             output))
    {
        error = QStringLiteral("Das Update ließ sich nicht öffnen (%1).")
                    .arg(output);
        return false;
    }

    bool ok = false;
    const auto app = QDir(mount.path()).absoluteFilePath(APP_NAME);
    if (!QFileInfo(app).isDir())
    {
        error = QStringLiteral("Im Update steckt kein ChattiFlexii.");
    }
    else
    {
        QDir(staging).removeRecursively();
        ok = run(QStringLiteral("/usr/bin/ditto"), {app, staging}, output);
        if (!ok)
        {
            QDir(staging).removeRecursively();
            error = QStringLiteral("Die neue Version ließ sich nicht "
                                   "kopieren (%1).")
                        .arg(output);
        }
    }

    QString ignored;
    run(QStringLiteral("/usr/bin/hdiutil"),
        {QStringLiteral("detach"), mount.path(), QStringLiteral("-force")},
        ignored);
    return ok;
}

QString swapScript()
{
    return QStringLiteral(R"(
pid="$1"; staging="$2"; app="$3"
old="$(dirname "$app")/.ChattiFlexii-old.app"
# Until the app has quit, a minute at most
i=0
while kill -0 "$pid" 2>/dev/null && [ "$i" -lt 600 ]; do
    sleep 0.1
    i=$((i + 1))
done
rm -rf "$old"
if mv "$app" "$old"; then
    if mv "$staging" "$app"; then
        rm -rf "$old"
    else
        mv "$old" "$app"
    fi
fi
xattr -dr com.apple.quarantine "$app" 2>/dev/null
[ "$4" = "noopen" ] || open "$app"
)");
}

void install(const updatecheck::Release &release, QWidget *parent)
{
    auto *progress =
        new QProgressDialog(QStringLiteral("Die neue Version wird geladen …"),
                            QStringLiteral("Abbrechen"), 0, 100, parent);
    progress->setWindowTitle(QStringLiteral("Update"));
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setValue(0);
    progress->show();

    auto *network = new QNetworkAccessManager(progress);
    QNetworkRequest request{QUrl(release.downloadUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = network->get(request);

    auto file = std::make_shared<QFile>(downloadFile());
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        reply->abort();
        progress->close();
        fail(parent, QStringLiteral("Die Datei ließ sich nicht anlegen."));
        return;
    }

    QObject::connect(reply, &QNetworkReply::readyRead, progress, [reply, file] {
        file->write(reply->readAll());
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, progress,
                     [progress](qint64 received, qint64 total) {
                         if (total > 0)
                         {
                             progress->setValue(int(received * 100 / total));
                         }
                     });
    QObject::connect(progress, &QProgressDialog::canceled, reply, [reply] {
        reply->abort();
    });
    QObject::connect(
        reply, &QNetworkReply::finished, progress,
        [reply, file, progress, parent, sha256 = release.sha256] {
            reply->deleteLater();
            file->write(reply->readAll());
            file->close();

            if (reply->error() != QNetworkReply::NoError)
            {
                const bool canceled =
                    reply->error() == QNetworkReply::OperationCanceledError;
                QFile::remove(file->fileName());
                progress->close();
                if (!canceled)
                {
                    fail(parent, QStringLiteral("Der Download brach ab (%1).")
                                     .arg(reply->errorString()));
                }
                return;
            }

            // Only what GitHub says it published goes in
            if (!sha256.isEmpty())
            {
                QFile check(file->fileName());
                QCryptographicHash hash(QCryptographicHash::Sha256);
                if (!check.open(QIODevice::ReadOnly) || !hash.addData(&check) ||
                    QString::fromLatin1(hash.result().toHex()) !=
                        sha256.toLower())
                {
                    QFile::remove(file->fileName());
                    progress->close();
                    fail(parent, QStringLiteral("Die Datei kam nicht "
                                                "vollständig an."));
                    return;
                }
            }

            progress->setLabelText(
                QStringLiteral("ChattiFlexii startet gleich neu …"));
            progress->setCancelButton(nullptr);
            progress->setValue(100);
            // After this paints, as readying it takes a moment
            QTimer::singleShot(100, progress, [file, progress, parent] {
                finish(file->fileName(), parent);
                progress->close();
            });
        });
}

}  // namespace chatterino::selfupdate
