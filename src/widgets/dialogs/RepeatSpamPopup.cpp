// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/RepeatSpamPopup.hpp"

#include "Application.hpp"
#include "controllers/commands/CommandController.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "util/FormatTime.hpp"

#include <QCursor>
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
    this->setMinimumWidth(440);

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

    this->testNote_ = new QLabel(
        QStringLiteral("<i style=\"color:#9a9a9a\">Test alert - made up "
                       "messages, and the buttons do nothing.</i>"));
    this->testNote_->setTextFormat(Qt::RichText);
    this->testNote_->hide();
    layout->addWidget(this->testNote_);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    this->ignore_ = new QPushButton(QStringLiteral("Ignore"));
    this->timeout_ = new QPushButton;
    buttons->addWidget(this->ignore_);
    buttons->addWidget(this->timeout_);
    layout->addLayout(buttons);

    QObject::connect(this->ignore_, &QPushButton::clicked, this, [this] {
        this->close();
    });

    QObject::connect(this->timeout_, &QPushButton::clicked, this, [this] {
        auto channel = getApp()->getTwitch()->getChannelOrEmpty(this->channel_);
        if (!this->test_ && !channel->isEmpty())
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

    this->countdown_.setInterval(1000);
    QObject::connect(&this->countdown_, &QTimer::timeout, this, [this] {
        this->tick();
    });
}

void RepeatSpamPopup::setCase(const QString &displayName,
                              const QList<Entry> &history, int seconds,
                              int timeoutsServed)
{
    this->seconds_ = seconds;

    const auto name = displayName.toHtmlEscaped();
    if (timeoutsServed == 0)
    {
        this->headline_->setText(
            QStringLiteral("<b>%1</b> sent the same message several times in "
                           "a row.")
                .arg(name));
    }
    else if (timeoutsServed == 1)
    {
        this->headline_->setText(
            QStringLiteral("<b>%1</b> sent the message again after being "
                           "timed out.")
                .arg(name));
    }
    else
    {
        this->headline_->setText(
            QStringLiteral("<b>%1</b> sent the message again after %2 "
                           "timeouts.")
                .arg(name)
                .arg(timeoutsServed));
    }

    QStringList lines;
    for (const auto &entry : history)
    {
        const auto time = entry.time.toString(QStringLiteral("hh:mm:ss"));
        if (entry.timeoutSeconds < 0)
        {
            lines.append(QStringLiteral("<tt>%1</tt>&nbsp;&nbsp;%2")
                             .arg(time, entry.text.toHtmlEscaped()));
        }
        else
        {
            lines.append(
                QStringLiteral(
                    "<tt>%1</tt>&nbsp;&nbsp;<i style=\"color:#9a9a9a\">"
                    "— %2</i>")
                    .arg(time,
                         entry.timeoutSeconds > 0
                             ? QStringLiteral("Timeout %1")
                                   .arg(formatTime(entry.timeoutSeconds))
                             : QStringLiteral("Ban")));
        }
    }
    this->messages_->setText(lines.join(QStringLiteral("<br>")));

    this->timeout_->setText(
        QStringLiteral("Timeout %1").arg(formatTime(seconds)));
    this->timeout_->setDefault(true);

    // Every new case starts the countdown over
    this->remaining_ =
        std::max(0, getSettings()->repeatAlertAutoClose.getValue());
    if (this->remaining_ > 0)
    {
        this->ignore_->setText(
            QStringLiteral("Ignore (%1)").arg(this->remaining_));
        this->countdown_.start();
    }
    else
    {
        this->ignore_->setText(QStringLiteral("Ignore"));
        this->countdown_.stop();
    }
}

void RepeatSpamPopup::setTestMode(bool test)
{
    this->test_ = test;
    this->testNote_->setVisible(test);
    this->setWindowTitle(
        test ? QStringLiteral("Repeated message - test")
             : QStringLiteral("Repeated message - #%1").arg(this->channel_));
}

void RepeatSpamPopup::tick()
{
    if (this->remaining_ <= 0)
    {
        return;
    }

    // Held open while the pointer rests on it, so it cannot vanish from under
    // a click on the timeout button
    if (this->frameGeometry().contains(QCursor::pos()))
    {
        this->ignore_->setText(QStringLiteral("Ignore"));
        return;
    }

    this->remaining_--;
    if (this->remaining_ <= 0)
    {
        this->close();
        return;
    }
    this->ignore_->setText(QStringLiteral("Ignore (%1)").arg(this->remaining_));
}

}  // namespace chatterino
