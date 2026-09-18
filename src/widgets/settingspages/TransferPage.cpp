// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/TransferPage.hpp"

#include "Application.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/AutoBackup.hpp"
#include "util/ProfileSetup.hpp"
#include "widgets/settingspages/PageSections.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLocale>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace chatterino {

using namespace pagesections;

namespace {

#ifdef Q_OS_MACOS
const QString FILE_MANAGER = QStringLiteral("Finder");
#elif defined(Q_OS_WIN)
const QString FILE_MANAGER = QStringLiteral("Explorer");
#else
const QString FILE_MANAGER = QStringLiteral("Dateimanager");
#endif

}  // namespace

TransferPage::TransferPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *tabs = new QTabWidget;
    outer->addWidget(tabs);
    this->buildExportTab(addPageTab(tabs, "Exportieren"));
    this->buildImportTab(addPageTab(tabs, "Importieren"));
    this->buildBackupTab(addPageTab(tabs, "Automatische Sicherung"));
    this->showBackupState();
}

void TransferPage::buildExportTab(QVBoxLayout *layout)
{
    addText(layout,
            "Packt dein komplettes ChattiFlexii in einen Ordner auf dem "
            "Schreibtisch - zum Beispiel, um ihn per AirDrop auf ein anderes "
            "Gerät zu schicken und dort genau so weiterzumachen.");

    addHeading(layout, "Was mitkommt");
    addText(layout,
            "Alle Einstellungen, Tabs und Tab-Gruppen, Highlights mit Farben "
            "und Captions, Befehle, Hotkeys, Nicknames, Ignores und Filter, "
            "benannte Farben, der Mod-Assistent mit seinen gelernten Fällen, "
            "Mod-Highlights, Themes sowie Plugins mit ihren Daten.");
    addText(layout,
            "Nicht dabei sind Chat-Logs und der Zwischenspeicher - die sind "
            "groß und werden auf dem anderen Gerät von selbst neu angelegt.",
            true);

    addHeading(layout, "Twitch-Login");
    this->includeLogin_ =
        new QCheckBox("Twitch-Login mitnehmen - nur für deine eigenen Geräte");
    layout->addWidget(this->includeLogin_);
    auto *loginNote = addText(layout, QString(), true);
    const auto showLoginNote = [this, loginNote] {
        if (this->includeLogin_->isChecked())
        {
            loginNote->setEnabled(true);
            loginNote->setText(QStringLiteral(
                "<span style=\"color:#ffaa00\">Mit Login darf der Ordner nie an "
                "andere gehen: Wer ihn hat, kann mit deinem Account schreiben "
                "und moderieren. Lösch ihn nach dem Import.</span>"));
        }
        else
        {
            loginNote->setEnabled(false);
            loginNote->setText(QStringLiteral(
                "Ohne Login meldest du dich auf dem anderen Gerät einmal bei "
                "Twitch an. Ist dort schon jemand angemeldet, bleibt das so."));
        }
    };
    showLoginNote();
    QObject::connect(this->includeLogin_, &QCheckBox::toggled, this,
                     showLoginNote);

    addHeading(layout, "Export");
    auto *exportButton = new QPushButton("Export-Ordner erstellen");
    QObject::connect(exportButton, &QPushButton::clicked, this, [this] {
        this->exportNow();
    });
    addButtonRow(layout, exportButton);

    this->exportStatus_ = addText(layout, QString());
    this->exportStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    this->exportStatus_->hide();

    this->reveal_ = new QPushButton(
        QStringLiteral("Im %1 zeigen").arg(FILE_MANAGER));
    QObject::connect(this->reveal_, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(this->lastExport_));
    });
    this->reveal_->hide();
    addButtonRow(layout, this->reveal_);

    layout->addStretch(1);
}

void TransferPage::buildImportTab(QVBoxLayout *layout)
{
    addText(layout,
            "Übernimmt einen Export-Ordner von einem anderen Gerät, sodass "
            "ChattiFlexii hier genau so aussieht wie dort. Ein per AirDrop "
            "geschickter Ordner landet auf dem Mac in „Downloads“.");

    addHeading(layout, "So läuft es ab");
    addText(layout,
            "1. Du wählst den Ordner „ChattiFlexii-Export …“ aus.<br>"
            "2. Deine jetzigen Einstellungen werden gesichert, in einen Ordner "
            "„Sicherung vor Import …“ im ChattiFlexii-Datenordner.<br>"
            "3. ChattiFlexii startet neu und übernimmt dabei den Export.");
    addText(layout,
            "Bist du hier schon bei Twitch angemeldet und der Export bringt "
            "keinen Login mit, bleibst du angemeldet.",
            true);

    addHeading(layout, "Import");
    auto *importButton = new QPushButton("Export-Ordner auswählen …");
    QObject::connect(importButton, &QPushButton::clicked, this, [this] {
        this->importNow();
    });
    addButtonRow(layout, importButton);

    addText(layout,
            "Auf einem neuen Gerät geht das auch gleich beim ersten Start von "
            "ChattiFlexii, im Begrüßungsfenster.",
            true);

    layout->addStretch(1);
}

void TransferPage::buildBackupTab(QVBoxLayout *layout)
{
    addText(layout,
            "Legt regelmäßig von selbst einen Export an - falls mal etwas "
            "kaputtgeht oder du ein Gerät neu einrichtest. Eine Sicherung holst "
            "du wie jeden Export unter „Importieren“ zurück. Der Twitch-Login "
            "ist nie dabei.");
    layout->addWidget(this->createCheckBox("Automatisch sichern",
                                           getSettings()->autoBackupEnabled));

    auto *form = new QFormLayout;
    auto *days = this->createSpinBox(getSettings()->autoBackupDays, 1, 30);
    days->setPrefix("alle ");
    days->setSuffix(" Tage");
    form->addRow("Wie oft", days);
    auto *keep = this->createSpinBox(getSettings()->autoBackupKeep, 1, 50);
    keep->setSuffix(" Sicherungen");
    form->addRow("Behalten", keep);
    layout->addLayout(form);
    addText(layout,
            "Ältere Sicherungen werden gelöscht - nur Ordner, die ChattiFlexii "
            "selbst als „ChattiFlexii-Sicherung …“ angelegt hat.",
            true);

    addHeading(layout, "Ordner");
    this->backupFolder_ = addText(layout, QString());
    this->backupFolder_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *change = new QPushButton("Ändern …");
    auto *reset = new QPushButton("Standardordner");
    auto *open = new QPushButton(QStringLiteral("Im %1 zeigen").arg(FILE_MANAGER));
    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(change);
    folderRow->addWidget(reset);
    folderRow->addWidget(open);
    folderRow->addStretch(1);
    layout->addLayout(folderRow);
    addText(layout,
            "Standard ist „ChattiFlexii-Sicherungen“ in iCloud Drive, sonst in "
            "Dokumente. In iCloud Drive hast du die Sicherungen auch auf deinen "
            "anderen Geräten.",
            true);

    addHeading(layout, "Jetzt");
    auto *now = new QPushButton("Jetzt sichern");
    addButtonRow(layout, now);
    this->backupStatus_ = addText(layout, QString());

    layout->addStretch(1);

    QObject::connect(change, &QPushButton::clicked, this, [this] {
        const auto chosen = QFileDialog::getExistingDirectory(
            this, "Ordner für Sicherungen", autobackup::folder());
        if (!chosen.isEmpty())
        {
            getSettings()->autoBackupFolder.setValue(chosen);
        }
    });
    QObject::connect(reset, &QPushButton::clicked, this, [] {
        getSettings()->autoBackupFolder.setValue(QString());
    });
    QObject::connect(open, &QPushButton::clicked, this, [] {
        const auto target = autobackup::folder();
        QDir().mkpath(target);
        QDesktopServices::openUrl(QUrl::fromLocalFile(target));
    });
    QObject::connect(now, &QPushButton::clicked, this, [this] {
        QString error;
        const auto made = autobackup::backUpNow(error);
        this->showBackupState();
        if (made.isEmpty())
        {
            this->backupStatus_->setText(
                QStringLiteral("<span style=\"color:#e05050\">%1</span>")
                    .arg(error.toHtmlEscaped()));
        }
    });

    getSettings()->autoBackupFolder.connect(
        [this](const auto &, auto) {
            this->showBackupState();
        },
        this->managedConnections_, false);
    getSettings()->autoBackupLast.connect(
        [this](const auto &, auto) {
            this->showBackupState();
        },
        this->managedConnections_, false);
}

void TransferPage::showBackupState()
{
    if (this->backupFolder_ == nullptr || this->backupStatus_ == nullptr)
    {
        return;
    }

    this->backupFolder_->setText(
        QDir::toNativeSeparators(autobackup::folder()).toHtmlEscaped());

    const auto last = autobackup::lastBackup();
    this->backupStatus_->setText(
        last.isValid()
            ? QStringLiteral("Letzte Sicherung: %1")
                  .arg(QLocale(QLocale::German)
                           .toString(last.toLocalTime(),
                                     QStringLiteral("dd.MM.yyyy HH:mm")))
            : QStringLiteral("Noch keine Sicherung."));
}

void TransferPage::exportNow()
{
    // Settings and tabs are otherwise only written when the app closes, and
    // the export copies what is on disk
    getSettings()->requestSave();
    getApp()->getWindows()->save();

    QString error;
    const auto folder = exportProfile(getApp()->getPaths(),
                                      this->includeLogin_->isChecked(), error);
    this->exportStatus_->show();
    if (folder.isEmpty())
    {
        this->exportStatus_->setText(
            QStringLiteral("<span style=\"color:#e05050\">%1</span>")
                .arg(error.toHtmlEscaped()));
        this->reveal_->hide();
        return;
    }

    this->lastExport_ = folder;
    this->exportStatus_->setText(
        QStringLiteral("Erstellt auf dem Schreibtisch: <b>%1</b><br>Schick den "
                       "ganzen Ordner aufs andere Gerät und wähl ihn dort unter "
                       "Export &amp; Import → Importieren aus.")
            .arg(QFileInfo(folder).fileName().toHtmlEscaped()));
    this->reveal_->show();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void TransferPage::importNow()
{
    const auto start =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const auto folder = QFileDialog::getExistingDirectory(
        this, "ChattiFlexii-Export auswählen", start);
    if (folder.isEmpty())
    {
        return;
    }

    if (!isProfileExport(folder))
    {
        QMessageBox::warning(
            this, "Kein Export",
            "In diesem Ordner liegt kein ChattiFlexii-Export. Wähl den Ordner "
            "„ChattiFlexii-Export …“ selbst aus, nicht den Ordner, in dem er "
            "liegt.");
        return;
    }

    const auto answer = QMessageBox::question(
        this, "Import",
        QStringLiteral("ChattiFlexii übernimmt „%1“ und startet dafür neu. "
                       "Deine jetzigen Einstellungen werden vorher gesichert."
                       "\n\nJetzt importieren?")
            .arg(QFileInfo(folder).fileName()));
    if (answer != QMessageBox::Yes)
    {
        return;
    }

    QString error;
    if (!stageProfileImport(getApp()->getPaths(), folder, error))
    {
        QMessageBox::warning(this, "Import", error);
        return;
    }

    if (!relaunchAfterExit())
    {
        QMessageBox::information(
            this, "Import",
            "Der Import ist vorbereitet. ChattiFlexii beendet sich jetzt - "
            "starte es danach bitte selbst wieder, dann ist alles da.");
    }
    QApplication::quit();
}

bool TransferPage::filterElements(const QString &query)
{
    static const QStringList keywords{
        "export", "import",  "übertragen", "backup", "sicherung", "icloud",
        "airdrop", "macbook", "computer",  "gerät",
    };

    return matchesKeywords(query, keywords);
}

}  // namespace chatterino
