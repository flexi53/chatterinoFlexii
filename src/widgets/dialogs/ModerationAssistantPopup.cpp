// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/ModerationAssistantPopup.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"

#include "controllers/moderation/ModerationAssistant.hpp"
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
        QStringLiteral("Moderation assistant - #%1").arg(this->channel_));
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setMinimumSize(780, 500);

    auto *layout = new QVBoxLayout(this->getLayoutContainer());
    layout->setContentsMargins(10, 10, 10, 10);

    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(QStringLiteral("Mode:")));
    this->mode_ = new QComboBox;
    this->mode_->addItems({
        QStringLiteral("Off - collect nothing"),
        QStringLiteral("Learn - collect cases"),
        QStringLiteral("Suggest - collect cases and suggest"),
    });
    this->mode_->setCurrentIndex(static_cast<int>(
        ModerationAssistant::instance().mode(this->channel_)));
    modeRow->addWidget(this->mode_);
    modeRow->addStretch(1);
    layout->addLayout(modeRow);

    auto *repeatAlert = new QCheckBox(QStringLiteral(
        "Alert when someone sends the same message three times in a row"));
    repeatAlert->setToolTip(QStringLiteral(
        "Opens a window with their last messages and a timeout button. Each "
        "time they carry on after serving a timeout, the button offers the "
        "next step - the steps are set under Settings, Moderation, "
        "Assistant. Works independently of the mode above."));
    repeatAlert->setChecked(
        RepeatSpamDetector::instance().isEnabled(this->channel_));
    QObject::connect(repeatAlert, &QCheckBox::toggled, this, [this](bool on) {
        RepeatSpamDetector::instance().setEnabled(this->channel_, on);
    });
    layout->addWidget(repeatAlert);

    this->status_ = new QLabel;
    layout->addWidget(this->status_);

    auto *explanation = new QLabel(QStringLiteral(
        "A case is a timeout or ban by any moderator here, together with what "
        "the user wrote before it. Once enough have been collected, a message "
        "that resembles earlier cases opens a window like the repeated "
        "message alert, offering the action moderators usually took. Nothing "
        "happens unless you press its button."));
    explanation->setWordWrap(true);
    explanation->setEnabled(false);
    layout->addWidget(explanation);

    auto *actions = new QHBoxLayout;
    auto *importButton =
        new QPushButton(QStringLiteral("Import from chat logs"));
    importButton->setToolTip(QStringLiteral(
        "Read the timeouts and bans already in this channel's chat logs, so "
        "there is something to go on from the start."));
    auto *removeButton = new QPushButton(QStringLiteral("Remove selected"));
    actions->addWidget(importButton);
    actions->addWidget(removeButton);
    actions->addStretch(1);
    layout->addLayout(actions);

    this->search_ = new QLineEdit;
    this->search_->setPlaceholderText(
        QStringLiteral("Search user, moderator, reason or message"));
    layout->addWidget(this->search_);

    this->table_ = new QTableWidget(0, 7);
    this->table_->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Moderator"),
        QStringLiteral("User"),
        QStringLiteral("Action"),
        QStringLiteral("Mod comment"),
        QStringLiteral("Detected"),
        QStringLiteral("What they wrote"),
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
                QStringLiteral("There are no chat logs for this channel yet. "
                               "Logging has to be switched on for it first."));
        }
        else
        {
            QMessageBox::information(
                this, QStringLiteral("Import"),
                QStringLiteral("Added %1 new cases from the chat logs.")
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
                QStringLiteral("%1 cases stored. Switched off - nothing new "
                               "is collected.")
                    .arg(count));
            break;
        case ModAssistMode::Learn:
            this->status_->setText(
                count < minCases
                    ? QStringLiteral("%1 of %2 cases collected.")
                          .arg(count)
                          .arg(minCases)
                    : QStringLiteral("%1 cases collected - enough to switch "
                                     "suggestions on.")
                          .arg(count));
            break;
        case ModAssistMode::Suggest:
            this->status_->setText(
                count < minCases
                    ? QStringLiteral("%1 of %2 cases collected - suggestions "
                                     "start once there are %2.")
                          .arg(count)
                          .arg(minCases)
                    : QStringLiteral("%1 cases collected - suggesting.")
                          .arg(count));
            break;
    }

    const auto needle = this->search_->text().trimmed();

    this->table_->setRowCount(0);
    for (const auto &modCase : cases)
    {
        const auto said = modCase.messages.join(QStringLiteral("  |  "));
        if (!needle.isEmpty() &&
            !modCase.user.contains(needle, Qt::CaseInsensitive) &&
            !modCase.moderator.contains(needle, Qt::CaseInsensitive) &&
            !modCase.reason.contains(needle, Qt::CaseInsensitive) &&
            !modCase.reasons.join(' ').contains(needle, Qt::CaseInsensitive) &&
            !said.contains(needle, Qt::CaseInsensitive))
        {
            continue;
        }

        const int row = this->table_->rowCount();
        this->table_->insertRow(row);

        auto *timeItem = new QTableWidgetItem(
            modCase.time.toString(QStringLiteral("yyyy-MM-dd hh:mm")));
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
                    : QStringLiteral("Ban")));
        this->table_->setItem(row, 4, new QTableWidgetItem(modCase.reason));
        this->table_->setItem(
            row, 5,
            new QTableWidgetItem(modCase.reasons.join(QStringLiteral(", "))));

        auto *saidItem = new QTableWidgetItem(said);
        saidItem->setToolTip(modCase.messages.join('\n'));
        this->table_->setItem(row, 6, saidItem);
    }

    this->table_->resizeColumnsToContents();
}

}  // namespace chatterino
