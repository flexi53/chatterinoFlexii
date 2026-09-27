// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/NukePopup.hpp"

#include "Application.hpp"
#include "controllers/commands/CommandController.hpp"
#include "messages/Message.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "singletons/Theme.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

using namespace chatterino;

/// Twitch takes a while to work through a burst of actions, so they are
/// handed over one after the other rather than all at once
constexpr int BETWEEN_ACTIONS_MS = 150;

QString lengthSaid(int seconds)
{
    if (seconds % 3600 == 0)
    {
        return QStringLiteral("%1 Std.").arg(seconds / 3600);
    }
    if (seconds % 60 == 0)
    {
        return QStringLiteral("%1 Min.").arg(seconds / 60);
    }
    return QStringLiteral("%1 Sek.").arg(seconds);
}

}  // namespace

namespace chatterino {

bool NukePopup::beyondReach(const std::vector<TwitchBadge> &badges)
{
    // A timeout would not stick on any of these, and one of them being hit
    // by a phrase everyone is repeating is the likely case
    static const QStringList SAFE{"broadcaster", "moderator", "vip", "staff",
                                  "admin", "global_mod"};
    return std::any_of(badges.begin(), badges.end(),
                       [](const TwitchBadge &badge) {
                           return SAFE.contains(badge.key_);
                       });
}

std::vector<NukePopup::Caught> NukePopup::whoWrote(const ChannelPtr &channel,
                                                   const QString &phrase,
                                                   int minutes,
                                                   const QString &ownLogin)
{
    std::vector<Caught> caught;
    if (channel == nullptr || phrase.isEmpty())
    {
        return caught;
    }

    const auto since =
        QDateTime::currentDateTimeUtc().addSecs(-qint64(minutes) * 60);

    for (const auto &message : channel->getMessageSnapshot())
    {
        if (message->flags.has(MessageFlag::System) ||
            message->loginName.isEmpty())
        {
            continue;
        }
        if (message->serverReceivedTime.isValid() &&
            message->serverReceivedTime.toUTC() < since)
        {
            continue;
        }
        if (!message->messageText.contains(phrase, Qt::CaseInsensitive))
        {
            continue;
        }
        if (message->loginName.compare(ownLogin, Qt::CaseInsensitive) == 0 ||
            beyondReach(message->twitchBadges))
        {
            continue;
        }

        // One line per person - the last thing they wrote with it in
        auto found = std::find_if(caught.begin(), caught.end(),
                                  [&message](const Caught &one) {
                                      return one.login.compare(
                                                 message->loginName,
                                                 Qt::CaseInsensitive) == 0;
                                  });
        if (found != caught.end())
        {
            found->message = message->messageText;
            found->when = message->serverReceivedTime;
            continue;
        }

        caught.push_back({
            .login = message->loginName,
            .displayName = message->displayName.isEmpty()
                               ? message->loginName
                               : message->displayName,
            .message = message->messageText,
            .when = message->serverReceivedTime,
        });
    }
    return caught;
}

NukePopup::NukePopup(ChannelPtr channel, QString phrase, int seconds,
                     std::vector<Caught> caught, QWidget *parent)
    : BasePopup({BaseWindow::EnableCustomFrame}, parent)
    , channel_(std::move(channel))
    , phrase_(std::move(phrase))
    , seconds_(seconds)
    , caught_(std::move(caught))
{
    this->setWindowTitle(QStringLiteral("Aufräumen"));
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this->getLayoutContainer());

    auto *what = new QLabel(
        QStringLiteral("„%1“ - %2 %3 getroffen. Ein Timeout von %4 für alle "
                       "davon?")
            .arg(this->phrase_)
            .arg(this->caught_.size())
            .arg(this->caught_.size() == 1 ? "Person" : "Leute")
            .arg(lengthSaid(this->seconds_)));
    what->setWordWrap(true);
    layout->addWidget(what);

    auto *list = new QListWidget;
    for (const auto &one : this->caught_)
    {
        list->addItem(QStringLiteral("%1 — %2")
                          .arg(one.displayName,
                               one.message.simplified().left(90)));
    }
    list->setMinimumHeight(140);
    layout->addWidget(list);

    auto *note = new QLabel(
        "Moderatoren, VIPs und der Streamer sind nicht dabei. Es passiert "
        "nichts, bis du drückst.");
    note->setWordWrap(true);
    note->setStyleSheet("color: #8a8a8a;");
    layout->addWidget(note);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *cancel = new QPushButton("Abbrechen");
    QObject::connect(cancel, &QPushButton::clicked, this, &QWidget::close);
    buttons->addWidget(cancel);

    auto *go = new QPushButton(QStringLiteral("%1 timeouten")
                                   .arg(this->caught_.size()));
    go->setDefault(false);
    go->setAutoDefault(false);
    QObject::connect(go, &QPushButton::clicked, this, [this] {
        this->give();
        this->close();
    });
    buttons->addWidget(go);
    layout->addLayout(buttons);
}

void NukePopup::give()
{
    auto channel = this->channel_;
    const auto seconds = this->seconds_;
    const auto reason = QStringLiteral("Aufräumen: %1").arg(this->phrase_);

    int delay = 0;
    for (const auto &one : this->caught_)
    {
        const auto command = QStringLiteral("/timeout %1 %2 %3")
                                 .arg(one.login)
                                 .arg(seconds)
                                 .arg(reason);
        QTimer::singleShot(delay, [channel, command] {
            getApp()->getCommands()->execCommand(command, channel, false);
        });
        delay += BETWEEN_ACTIONS_MS;
    }

    channel->addSystemMessage(
        QStringLiteral("Timeout von %1 für %2 - der Grund steht dabei.")
            .arg(lengthSaid(seconds))
            .arg(this->caught_.size()));
}

}  // namespace chatterino
