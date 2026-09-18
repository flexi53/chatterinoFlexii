// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/UpdateCheck.hpp"

#include "Application.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "common/Version.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Window.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonArray>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include <chrono>

namespace chatterino::updatecheck {

namespace {

const QString RELEASE_URL = QStringLiteral(
    "https://api.github.com/repos/flexi53/chatterinoFlexii/releases/tags/"
    "latest");

constexpr auto FIRST_CHECK = std::chrono::seconds(30);
constexpr auto CHECK_EVERY = std::chrono::hours(6);
/// How long "Später" puts a download off
constexpr auto SNOOZE = std::chrono::hours(24);

QString assetName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("ChattiFlexii-windows-x64.zip");
#else
    return QStringLiteral("ChattiFlexii-macOS-arm64.dmg");
#endif
}

QString howToInstall()
{
#ifdef Q_OS_WIN
    return QStringLiteral(
        "Beende ChattiFlexii und entpacke die ZIP über den alten Ordner.");
#else
    return QStringLiteral(
        "Beende ChattiFlexii, öffne die DMG und zieh ChattiFlexii in den "
        "Programme-Ordner (ersetzen). Beim ersten Start: Rechtsklick -> "
        "Öffnen.");
#endif
}

bool snoozed(const Release &release)
{
    auto &s = *getSettings();
    if (s.updateSnoozedCommit.getValue() != release.commit)
    {
        return false;
    }
    const auto until =
        QDateTime::fromString(s.updateSnoozedUntil.getValue(), Qt::ISODate);
    return until.isValid() && QDateTime::currentDateTimeUtc() < until;
}

QWidget *mainWindow()
{
    return &getApp()->getWindows()->getMainWindow();
}

void offer(const Release &release)
{
    // One offer at a time
    static QPointer<QMessageBox> open;
    if (open)
    {
        return;
    }

    auto *box = new QMessageBox(mainWindow());
    open = box;
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setIcon(QMessageBox::Information);
    box->setWindowTitle(QStringLiteral("Neue ChattiFlexii-Version"));
    box->setText(
        QStringLiteral("Es gibt eine neuere Version von ChattiFlexii (vom %1).")
            .arg(QLocale(QLocale::German)
                     .toString(release.published.toLocalTime(),
                               QStringLiteral("d. MMMM, HH:mm"))));
    box->setInformativeText(
        QStringLiteral("„Herunterladen“ lädt sie in deinem Browser herunter. "
                       "%1 Deine Einstellungen bleiben, wie sie sind.")
            .arg(howToInstall()));
    auto *download = box->addButton(QStringLiteral("Herunterladen"),
                                    QMessageBox::AcceptRole);
    box->addButton(QStringLiteral("Später"), QMessageBox::RejectRole);
    box->setDefaultButton(download);

    QObject::connect(
        box, &QMessageBox::buttonClicked, box,
        [download, release](QAbstractButton *clicked) {
            if (clicked == download)
            {
                QDesktopServices::openUrl(QUrl(release.downloadUrl));
                return;
            }
            auto &s = *getSettings();
            s.updateSnoozedCommit.setValue(release.commit);
            s.updateSnoozedUntil.setValue(
                QDateTime::currentDateTimeUtc()
                    .addSecs(std::chrono::seconds(SNOOZE).count())
                    .toString(Qt::ISODate));
        });
    box->open();
}

void tell(const QString &text)
{
    QMessageBox::information(mainWindow(),
                             QStringLiteral("Nach Updates suchen"), text);
}

}  // namespace

std::optional<Release> parseRelease(const QJsonObject &json,
                                    const QString &assetName)
{
    static const QRegularExpression commitIn(
        QStringLiteral(R"(\bcommit\s+([0-9a-f]{7,40})\b)"));
    const auto commit =
        commitIn.match(json.value(QStringLiteral("body")).toString());
    if (!commit.hasMatch())
    {
        return std::nullopt;
    }

    for (const auto &value : json.value(QStringLiteral("assets")).toArray())
    {
        const auto asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString() != assetName)
        {
            continue;
        }
        auto published = QDateTime::fromString(
            asset.value(QStringLiteral("updated_at")).toString(), Qt::ISODate);
        if (!published.isValid())
        {
            published = QDateTime::fromString(
                json.value(QStringLiteral("published_at")).toString(),
                Qt::ISODate);
        }
        return Release{
            .commit = commit.captured(1),
            .downloadUrl =
                asset.value(QStringLiteral("browser_download_url")).toString(),
            .published = published,
        };
    }
    return std::nullopt;
}

bool isNewer(const Release &release, const QString &runningCommit)
{
    static const QRegularExpression commit(
        QStringLiteral("^[0-9a-f]{7,40}$"));
    const auto running = runningCommit.trimmed().toLower();
    if (!commit.match(running).hasMatch() ||
        !commit.match(release.commit).hasMatch())
    {
        // Nothing to compare - better no offer than a wrong one
        return false;
    }
    return !release.commit.startsWith(running) &&
           !running.startsWith(release.commit);
}

bool canCheck()
{
    return Version::instance().isGitHubBuild();
}

void checkNow(bool fromUser)
{
    if (!canCheck())
    {
        if (fromUser)
        {
            tell(QStringLiteral(
                "Diese ChattiFlexii-Version wurde nicht von GitHub gebaut, "
                "sondern selbst - sie sucht nicht nach Updates."));
        }
        return;
    }

    NetworkRequest(RELEASE_URL, NetworkRequestType::Get)
        .header("Accept", "application/vnd.github+json")
        .timeout(20000)
        .onSuccess([fromUser](const NetworkResult &result) {
            const auto release = parseRelease(result.parseJson(), assetName());
            if (!release ||
                !isNewer(*release, Version::instance().commitHash()))
            {
                if (fromUser)
                {
                    tell(QStringLiteral("Du hast schon die neueste Version."));
                }
                return;
            }
            if (!fromUser && snoozed(*release))
            {
                return;
            }
            offer(*release);
        })
        .onError([fromUser](const NetworkResult &result) {
            qCWarning(chatterinoApp)
                << "Looking for a newer ChattiFlexii failed:"
                << result.formatError();
            if (fromUser)
            {
                tell(QStringLiteral("GitHub war gerade nicht erreichbar - "
                                    "versuch es gleich noch einmal."));
            }
        })
        .execute();
}

void start()
{
    static bool started = false;
    if (started || !canCheck())
    {
        return;
    }
    started = true;

    const auto check = [] {
        if (getSettings()->updateCheckEnabled.getValue())
        {
            checkNow(false);
        }
    };
    auto *context = new QObject(QCoreApplication::instance());
    QTimer::singleShot(FIRST_CHECK, context, check);
    auto *timer = new QTimer(context);
    timer->setInterval(CHECK_EVERY);
    QObject::connect(timer, &QTimer::timeout, context, check);
    timer->start();
}

}  // namespace chatterino::updatecheck
