// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/RepeatSpamPopup.hpp"

#include "Application.hpp"
#include "controllers/commands/CommandController.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "util/FormatTime.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace chatterino {

RepeatSpamPopup::RepeatSpamPopup(QString channel, QString login,
                                 QWidget *parent)
    : BasePopup(
          {
              BaseWindow::EnableCustomFrame,
              BaseWindow::DisableLayoutSave,
              BaseWindow::BoundsCheckOnShow,
          },
          parent)
    , channel_(std::move(channel))
    , login_(std::move(login))
{
    this->setWindowTitle(
        QStringLiteral("Repeated message - #%1").arg(this->channel_));
    this->setAttribute(Qt::WA_DeleteOnClose);
    // Pops up while the moderator may be typing - it must not take the
    // keyboard away from them
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this->getLayoutContainer());
    layout->setContentsMargins(12, 12, 12, 12);

    this->headline_ = new QLabel;
    this->headline_->setTextFormat(Qt::RichText);
    this->headline_->setWordWrap(true);
    layout->addWidget(this->headline_);

    this->messages_ = new QLabel;
    this->messages_->setTextFormat(Qt::RichText);
    this->messages_->setWordWrap(true);
    this->messages_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(this->messages_, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *ignore = new QPushButton(QStringLiteral("Ignore"));
    this->timeout_ = new QPushButton;
    buttons->addWidget(ignore);
    buttons->addWidget(this->timeout_);
    layout->addLayout(buttons);

    QObject::connect(ignore, &QPushButton::clicked, this, [this] {
        this->close();
    });

    QObject::connect(this->timeout_, &QPushButton::clicked, this, [this] {
        auto channel = getApp()->getTwitch()->getChannelOrEmpty(this->channel_);
        if (!channel->isEmpty())
        {
            auto command = QStringLiteral("/timeout %1 %2")
                               .arg(this->login_)
                               .arg(this->seconds_);
            command =
                getApp()->getCommands()->execCommand(command, channel, false);
            channel->sendMessage(command);
        }
        this->close();
    });
}

void RepeatSpamPopup::setCase(const QString &displayName,
                              const QList<QPair<QDateTime, QString>> &messages,
                              int seconds, bool again)
{
    this->seconds_ = seconds;

    this->headline_->setText(
        again ? QStringLiteral("<b>%1</b> sent the message again after being "
                               "timed out.")
                    .arg(displayName.toHtmlEscaped())
              : QStringLiteral("<b>%1</b> sent the same message several times "
                               "in a row.")
                    .arg(displayName.toHtmlEscaped()));

    QStringList lines;
    for (const auto &[time, text] : messages)
    {
        lines.append(QStringLiteral("<tt>%1</tt>&nbsp;&nbsp;%2")
                         .arg(time.toString(QStringLiteral("hh:mm:ss")),
                              text.toHtmlEscaped()));
    }
    this->messages_->setText(lines.join(QStringLiteral("<br>")));

    this->timeout_->setText(
        QStringLiteral("Timeout %1").arg(formatTime(seconds)));
    this->timeout_->setDefault(true);
}

}  // namespace chatterino
