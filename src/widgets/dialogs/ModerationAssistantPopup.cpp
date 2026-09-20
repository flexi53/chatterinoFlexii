// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/ModerationAssistantPopup.hpp"

#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/WordAlertDetector.hpp"
#include "controllers/moderation/ModerationAssistant.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "singletons/Settings.hpp"
#include "util/FormatTime.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace chatterino {

namespace {

/// The reasons detected for a case, as they read on screen
QString detectedReasons(const ModCase &modCase)
{
    QStringList labels;
    for (const auto &reason : modCase.reasons)
    {
        labels.append(ModerationAssistant::reasonLabel(reason));
    }
    return labels.join(QStringLiteral(", "));
}

}  // namespace

ModerationAssistantPopup::ModerationAssistantPopup(const QString &channel,
                                                   QWidget *parent)
    : BasePopup(
          {
              BaseWindow::EnableCustomFrame,
              BaseWindow::DisableLayoutSave,
              BaseWindow::BoundsCheckOnShow,
          },
          parent)
    , channel_(channel.toLower())
{
    this->setWindowTitle(
        QStringLiteral("Mod-Assistent – #%1").arg(this->channel_));
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setMinimumSize(780, 500);

    auto *layout = new QVBoxLayout(this->getLayoutContainer());
    layout->setContentsMargins(10, 10, 10, 10);

    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(QStringLiteral("Modus:")));
    this->mode_ = new QComboBox;
    this->mode_->addItems({
        QStringLiteral("Aus – nichts sammeln"),
        QStringLiteral("Lernen – Fälle sammeln"),
        QStringLiteral("Vorschlagen – Fälle sammeln und vorschlagen"),
    });
    this->mode_->setCurrentIndex(static_cast<int>(
        ModerationAssistant::instance().mode(this->channel_)));
    modeRow->addWidget(this->mode_);
    modeRow->addStretch(1);
    layout->addLayout(modeRow);

    auto *repeatAlert = new QCheckBox(QStringLiteral(
        "Alarm, wenn jemand dreimal hintereinander dieselbe Nachricht schreibt"));
    repeatAlert->setToolTip(QStringLiteral(
        "Öffnet ein Fenster mit den letzten Nachrichten und einem "
        "Timeout-Knopf. Macht der User nach einem Timeout weiter, bietet der "
        "Knopf die nächste Stufe an - die Stufen stellst du unter "
        "Einstellungen → Mod-Assistent ein. Unabhängig vom Modus oben."));
    repeatAlert->setChecked(
        RepeatSpamDetector::instance().isEnabled(this->channel_));
    QObject::connect(repeatAlert, &QCheckBox::toggled, this, [this](bool on) {
        RepeatSpamDetector::instance().setEnabled(this->channel_, on);
    });
    layout->addWidget(repeatAlert);

    auto *emoteAlert = new QCheckBox(QStringLiteral("Alarm bei Emote-Spam"));
    emoteAlert->setToolTip(QStringLiteral(
        "Öffnet ein Fenster, wenn jemand den Chat mit Emotes flutet - die "
        "Emotes werden über eine kurze Zeit zusammengezählt, wie unter "
        "Einstellungen → Mod-Assistent eingestellt. Zuerst bietet es an, die "
        "Nachrichten zu löschen, später einen Timeout. Unabhängig vom Modus "
        "oben."));
    emoteAlert->setChecked(
        EmoteSpamDetector::instance().isEnabled(this->channel_));
    QObject::connect(emoteAlert, &QCheckBox::toggled, this, [this](bool on) {
        EmoteSpamDetector::instance().setEnabled(this->channel_, on);
    });
    layout->addWidget(emoteAlert);

    auto *wordAlert = new QCheckBox(QStringLiteral("Alarm bei Wörtern von "
                                                   "der Liste"));
    wordAlert->setToolTip(QStringLiteral(
        "Öffnet ein Fenster, wenn jemand ein Wort schreibt, das du unter "
        "Einstellungen → Mod-Assistent → Wörter hinterlegt hast - auch "
        "abgewandelt geschrieben. Es schlägt die Timeout-Dauer vor, die du "
        "dort eingestellt hast. Unabhängig vom Modus oben."));
    wordAlert->setChecked(WordAlertDetector::instance().isEnabled(this->channel_));
    QObject::connect(wordAlert, &QCheckBox::toggled, this, [this](bool on) {
        WordAlertDetector::instance().setEnabled(this->channel_, on);
    });
    layout->addWidget(wordAlert);

    this->status_ = new QLabel;
    layout->addWidget(this->status_);

    auto *explanation = new QLabel(QStringLiteral(
        "Ein Fall ist ein Timeout oder Bann eines Mods hier, zusammen mit dem, "
        "was der User davor geschrieben hat. Sind genug gesammelt, öffnet eine "
        "Nachricht, die früheren Fällen ähnelt, ein Fenster wie der Alarm für "
        "wiederholte Nachrichten - mit der Aktion, die Mods meistens gegeben "
        "haben. Es passiert nichts, solange du nicht auf den Knopf drückst."));
    explanation->setWordWrap(true);
    explanation->setEnabled(false);
    layout->addWidget(explanation);

    auto *actions = new QHBoxLayout;
    auto *importButton =
        new QPushButton(QStringLiteral("Aus Chat-Logs importieren"));
    importButton->setToolTip(QStringLiteral(
        "Liest die Timeouts und Banns, die schon in den Chat-Logs dieses "
        "Kanals stehen, damit es von Anfang an etwas zum Lernen gibt."));
    auto *removeButton = new QPushButton(QStringLiteral("Auswahl entfernen"));
    actions->addWidget(importButton);
    actions->addWidget(removeButton);
    actions->addStretch(1);
    layout->addLayout(actions);

    this->search_ = new QLineEdit;
    this->search_->setPlaceholderText(
        QStringLiteral("User, Mod, Grund oder Nachricht suchen"));
    layout->addWidget(this->search_);

    this->table_ = new QTableWidget(0, 7);
    this->table_->setHorizontalHeaderLabels({
        QStringLiteral("Zeit"),
        QStringLiteral("Mod"),
        QStringLiteral("User"),
        QStringLiteral("Aktion"),
        QStringLiteral("Mod-Kommentar"),
        QStringLiteral("Erkannt"),
        QStringLiteral("Was geschrieben wurde"),
    });
    this->table_->horizontalHeader()->setStretchLastSection(true);
    this->table_->verticalHeader()->setVisible(false);
    this->table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->table_->setWordWrap(false);
    layout->addWidget(this->table_, 1);

    QObject::connect(this->mode_, &QComboBox::currentIndexChanged, this,
                     [this](int index) {
                         ModerationAssistant::instance().setMode(
                             this->channel_,
                             static_cast<ModAssistMode>(index));
                         this->refresh();
                     });

    QObject::connect(this->search_, &QLineEdit::textChanged, this, [this] {
        this->refresh();
    });

    QObject::connect(importButton, &QPushButton::clicked, this, [this] {
        const auto added =
            ModerationAssistant::instance().importFromLogs(this->channel_);
        if (added < 0)
        {
            QMessageBox::information(
                this, QStringLiteral("Import"),
                QStringLiteral("Für diesen Kanal gibt es noch keine Chat-Logs. "
                               "Dafür muss das Loggen für ihn eingeschaltet "
                               "sein."));
        }
        else
        {
            QMessageBox::information(
                this, QStringLiteral("Import"),
                QStringLiteral("%1 neue Fälle aus den Chat-Logs übernommen.")
                    .arg(added));
        }
        this->refresh();
    });

    QObject::connect(removeButton, &QPushButton::clicked, this, [this] {
        for (const auto &row : this->table_->selectionModel()->selectedRows())
        {
            const auto *item = this->table_->item(row.row(), 0);
            ModerationAssistant::instance().removeCase(
                this->channel_, item->data(Qt::UserRole).toDateTime(),
                item->data(Qt::UserRole + 1).toString());
        }
        this->refresh();
    });

    this->refresh();
}

void ModerationAssistantPopup::refresh()
{
    auto &assistant = ModerationAssistant::instance();
    const auto cases = assistant.cases(this->channel_);
    const auto count = static_cast<int>(cases.size());
    const auto minCases = getSettings()->modAssistMinCases.getValue();

    switch (assistant.mode(this->channel_))
    {
        case ModAssistMode::Off:
            this->status_->setText(
                QStringLiteral("%1 Fälle gespeichert. Ausgeschaltet - es wird "
                               "nichts Neues gesammelt.")
                    .arg(count));
            break;
        case ModAssistMode::Learn:
            this->status_->setText(
                count < minCases
                    ? QStringLiteral("%1 von %2 Fällen gesammelt.")
                          .arg(count)
                          .arg(minCases)
                    : QStringLiteral("%1 Fälle gesammelt - genug, um "
                                     "Vorschläge einzuschalten.")
                          .arg(count));
            break;
        case ModAssistMode::Suggest:
            this->status_->setText(
                count < minCases
                    ? QStringLiteral("%1 von %2 Fällen gesammelt - Vorschläge "
                                     "kommen ab %2.")
                          .arg(count)
                          .arg(minCases)
                    : QStringLiteral("%1 Fälle gesammelt - Vorschläge sind an.")
                          .arg(count));
            break;
    }

    const auto needle = this->search_->text().trimmed();

    this->table_->setRowCount(0);
    for (const auto &modCase : cases)
    {
        const auto said = modCase.messages.join(QStringLiteral("  |  "));
        const auto detected = detectedReasons(modCase);
        if (!needle.isEmpty() &&
            !modCase.user.contains(needle, Qt::CaseInsensitive) &&
            !modCase.moderator.contains(needle, Qt::CaseInsensitive) &&
            !modCase.reason.contains(needle, Qt::CaseInsensitive) &&
            !detected.contains(needle, Qt::CaseInsensitive) &&
            !said.contains(needle, Qt::CaseInsensitive))
        {
            continue;
        }

        const int row = this->table_->rowCount();
        this->table_->insertRow(row);

        auto *timeItem = new QTableWidgetItem(
            modCase.time.toString(QStringLiteral("dd.MM.yyyy HH:mm")));
        timeItem->setData(Qt::UserRole, modCase.time);
        timeItem->setData(Qt::UserRole + 1, modCase.user);
        this->table_->setItem(row, 0, timeItem);

        this->table_->setItem(
            row, 1,
            new QTableWidgetItem(modCase.moderator.isEmpty()
                                     ? QStringLiteral("-")
                                     : modCase.moderator));
        this->table_->setItem(row, 2, new QTableWidgetItem(modCase.user));
        this->table_->setItem(
            row, 3,
            new QTableWidgetItem(
                modCase.seconds > 0
                    ? QStringLiteral("Timeout %1").arg(formatTime(modCase.seconds))
                    : QStringLiteral("Bann")));
        this->table_->setItem(row, 4, new QTableWidgetItem(modCase.reason));
        this->table_->setItem(row, 5, new QTableWidgetItem(detected));

        auto *saidItem = new QTableWidgetItem(said);
        saidItem->setToolTip(modCase.messages.join('\n'));
        this->table_->setItem(row, 6, saidItem);
    }

    this->table_->resizeColumnsToContents();
}

}  // namespace chatterino
