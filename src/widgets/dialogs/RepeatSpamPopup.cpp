// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/RepeatSpamPopup.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/FormatTime.hpp"
#include "widgets/buttons/PixmapButton.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/Label.hpp"

#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

using namespace chatterino;

/// How many of the chatter's lines - messages and timeouts - the window shows
constexpr size_t SHOWN_MESSAGES = 12;
constexpr int AVATAR_SIZE = 64;
/// Accounts younger than this are marked, since fresh accounts are where
/// spam tends to come from
constexpr qint64 NEW_ACCOUNT_DAYS = 30;
constexpr int TICK_MS = 50;

/// A thin bar along the bottom that empties as the window's time runs out
class CountdownBar : public QWidget
{
public:
    explicit CountdownBar(QWidget *parent)
        : QWidget(parent)
    {
        this->setFixedHeight(4);
    }

    void setState(double fraction, const QColor &color)
    {
        this->fraction_ = std::clamp(fraction, 0.0, 1.0);
        this->color_ = color;
        this->update();
    }

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF track = this->rect();
        const auto radius = track.height() / 2;

        auto trackColor = this->color_;
        trackColor.setAlpha(45);
        QPainterPath trackPath;
        trackPath.addRoundedRect(track, radius, radius);
        painter.fillPath(trackPath, trackColor);

        auto filled = track;
        filled.setWidth(track.width() * this->fraction_);
        QPainterPath fillPath;
        fillPath.addRoundedRect(filled, radius, radius);
        painter.fillPath(fillPath, this->color_);
    }

private:
    double fraction_ = 1.0;
    QColor color_{"#00aeef"};
};

/// The picture until the real one arrives, and the one a test chatter gets:
/// their initial on a coloured disc
QPixmap initialAvatar(const QString &name, const QColor &color)
{
    QPixmap pixmap(AVATAR_SIZE * 2, AVATAR_SIZE * 2);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(pixmap.rect());

    auto font = painter.font();
    font.setPixelSize(AVATAR_SIZE);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, name.left(1).toUpper());
    return pixmap;
}

/// Lines that belong on the card: what the chatter wrote, and the timeouts
/// and bans handed to them
bool isAbout(const MessagePtr &message, const QString &login)
{
    if (message->flags.has(MessageFlag::Whisper))
    {
        return false;
    }
    return message->loginName.compare(login, Qt::CaseInsensitive) == 0 ||
           message->timeoutUser.compare(login, Qt::CaseInsensitive) == 0;
}

/// A chat line for the test, built the way a real one reads
MessagePtr makeTestMessage(const QString &displayName, const QString &text,
                           const QDateTime &time)
{
    MessageBuilder builder;
    builder.emplace<TimestampElement>(time.time());
    builder.emplace<TextElement>(displayName + ":", MessageElementFlag::Username,
                                 MessageColor(QColor(255, 127, 80)),
                                 FontStyle::ChatMediumBold);
    builder.appendOrEmplaceText(text, MessageColor::Text);
    builder->loginName = displayName.toLower();
    builder->displayName = displayName;
    builder->messageText = text;
    builder->searchText = displayName + ": " + text;
    builder->serverReceivedTime = time;
    return builder.release();
}

}  // namespace

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
    this->setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this->getLayoutContainer());
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // Who it is, laid out like the top of a user card
    auto *head = new QHBoxLayout;
    head->setSpacing(12);

    this->avatar_ = new PixmapButton(nullptr);
    this->avatar_->setScaleIndependentSize(AVATAR_SIZE, AVATAR_SIZE);
    this->avatar_->setDim(DimButton::Dim::None);
    head->addWidget(this->avatar_, 0, Qt::AlignTop);

    auto *who = new QVBoxLayout;
    who->setSpacing(2);
    this->name_ = new Label(QString());
    this->name_->setFontStyle(FontStyle::UiMediumBold);
    // Lines up with the details below it
    this->name_->setPadding(QMargins(0, 0, 0, 0));
    who->addWidget(this->name_);
    this->details_ = new QLabel(this->login_);
    this->details_->setTextFormat(Qt::RichText);
    who->addWidget(this->details_);
    this->headline_ = new QLabel;
    this->headline_->setWordWrap(true);
    who->addWidget(this->headline_);
    who->addStretch(1);
    head->addLayout(who, 1);
    layout->addLayout(head);

    // What they wrote, as it looked in chat
    this->messages_ = new ChannelView(this, ChannelView::Context::UserCard,
                                      SHOWN_MESSAGES * 2);
    // Tall enough for an alert after a timeout - three messages, the
    // timeout, the new one - without scrolling
    this->messages_->setMinimumSize(456, 150);
    this->messages_->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);
    layout->addWidget(this->messages_, 1);

    this->testNote_ = new QLabel(
        QStringLiteral("<i style=\"color:#9a9a9a\">Test alert - made up "
                       "messages, and the buttons do nothing.</i>"));
    this->testNote_->setTextFormat(Qt::RichText);
    this->testNote_->hide();
    layout->addWidget(this->testNote_);

    this->countdownBar_ = new CountdownBar(this);
    layout->addWidget(this->countdownBar_);

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

    this->countdown_.setInterval(TICK_MS);
    QObject::connect(&this->countdown_, &QTimer::timeout, this, [this] {
        this->tick();
    });
}

void RepeatSpamPopup::setCase(const QString &displayName, int seconds,
                              int timeoutsServed)
{
    this->test_ = false;
    this->testNote_->hide();
    this->name_->setText(displayName);
    this->setHeadline(timeoutsServed);
    this->setTimeoutSeconds(seconds);

    if (!this->profileLoaded_)
    {
        this->avatar_->setPixmap(
            initialAvatar(displayName, this->theme->accent));
        this->loadProfile();
    }

    // Straight from the channel, so the lines look exactly as they did in
    // chat - badges, emotes and Twitch's own timeout notices included
    auto source = getApp()->getTwitch()->getChannelOrEmpty(this->channel_);

    std::vector<MessagePtr> picked;
    for (const auto &message : source->getMessageSnapshot())
    {
        if (isAbout(message, this->login_))
        {
            picked.push_back(message);
        }
    }

    this->view_ = std::make_shared<Channel>(this->channel_, Channel::Type::None);
    const auto first = picked.size() > SHOWN_MESSAGES
                           ? picked.end() - SHOWN_MESSAGES
                           : picked.begin();
    for (auto it = first; it != picked.end(); ++it)
    {
        this->view_->addMessage(*it, MessageContext::Repost);
    }
    this->messages_->setChannel(this->view_);
    this->messages_->setSourceChannel(source);

    // Whatever else they write while the window is open joins in
    this->liveMessages_.reset();
    this->liveMessages_.emplace(
        source->messageAppended.connect([this](auto message, auto) {
            if (this->view_ && isAbout(message, this->login_))
            {
                this->view_->addMessage(message, MessageContext::Repost);
            }
        }));

    this->restartCountdown();
}

void RepeatSpamPopup::showTestCase(bool afterTimeout)
{
    this->test_ = true;
    this->profileLoaded_ = true;
    this->testNote_->show();
    this->setWindowTitle(QStringLiteral("Repeated message - test"));

    const QString name = QStringLiteral("TestUser");
    this->name_->setText(name);
    this->details_->setText(QStringLiteral(
        "testuser &middot; <span style=\"color:#ffaa00\">account created 2 "
        "days ago</span>"));
    this->avatar_->setPixmap(initialAvatar(name, this->theme->accent));

    const auto now = QDateTime::currentDateTime();
    const auto steps = RepeatSpamDetector::steps();

    this->liveMessages_.reset();
    this->view_ = std::make_shared<Channel>(QStringLiteral("test"),
                                            Channel::Type::None);
    this->view_->addMessage(
        makeTestMessage(name, "kauft jetzt merch", now.addSecs(-40)),
        MessageContext::Original);
    this->view_->addMessage(
        makeTestMessage(name, "kauft jetzt merch", now.addSecs(-36)),
        MessageContext::Original);
    this->view_->addMessage(
        makeTestMessage(name, "kauft zarbex merch", now.addSecs(-31)),
        MessageContext::Original);

    if (afterTimeout)
    {
        this->view_->addMessage(
            MessageBuilder(timeoutMessage, name.toLower(),
                           QString::number(steps.front()), false,
                           now.addSecs(-25))
                .release(),
            MessageContext::Original);
        this->view_->addMessage(
            makeTestMessage(name, "kauft jetzt merch", now),
            MessageContext::Original);
        this->setHeadline(1);
        this->setTimeoutSeconds(steps[std::min<size_t>(1, steps.size() - 1)]);
    }
    else
    {
        this->setHeadline(0);
        this->setTimeoutSeconds(steps.front());
    }

    this->messages_->setChannel(this->view_);
    this->restartCountdown();
}

void RepeatSpamPopup::setHeadline(int timeoutsServed)
{
    if (timeoutsServed == 0)
    {
        this->headline_->setText(
            QStringLiteral("Sent the same message several times in a row."));
    }
    else if (timeoutsServed == 1)
    {
        this->headline_->setText(
            QStringLiteral("Sent the message again after being timed out."));
    }
    else
    {
        this->headline_->setText(
            QStringLiteral("Sent the message again after %1 timeouts.")
                .arg(timeoutsServed));
    }
}

void RepeatSpamPopup::setTimeoutSeconds(int seconds)
{
    this->seconds_ = seconds;
    this->timeout_->setText(
        QStringLiteral("Timeout %1").arg(formatTime(seconds)));
    this->timeout_->setDefault(true);
}

void RepeatSpamPopup::loadProfile()
{
    this->profileLoaded_ = true;

    std::weak_ptr<bool> alive = this->alive_;
    getHelix()->getUserByName(
        this->login_,
        [this, alive](const HelixUser &user) {
            if (!alive.lock())
            {
                return;
            }

            const auto created =
                QDateTime::fromString(user.createdAt, Qt::ISODate);
            if (created.isValid())
            {
                const auto days =
                    created.daysTo(QDateTime::currentDateTimeUtc());
                const auto age =
                    days <= 0   ? QStringLiteral("today")
                    : days == 1 ? QStringLiteral("1 day ago")
                                : QStringLiteral("%1 days ago").arg(days);
                const auto created_ =
                    QStringLiteral("account created %1").arg(age);

                this->details_->setText(
                    QStringLiteral("%1 &middot; %2")
                        .arg(this->login_.toHtmlEscaped(),
                             days < NEW_ACCOUNT_DAYS
                                 ? QStringLiteral(
                                       "<span style=\"color:#ffaa00\">%1</span>")
                                       .arg(created_)
                                 : created_));
                this->details_->setToolTip(created.toString(Qt::ISODate));
            }

            if (user.profileImageUrl.isEmpty())
            {
                return;
            }

            static auto *manager = new QNetworkAccessManager();
            QNetworkRequest request{QUrl(user.profileImageUrl)};
            request.setHeader(QNetworkRequest::UserAgentHeader, "Chatterino");
            auto *reply = manager->get(request);
            QObject::connect(reply, &QNetworkReply::finished, this,
                             [this, reply] {
                                 reply->deleteLater();
                                 if (reply->error() != QNetworkReply::NoError)
                                 {
                                     return;
                                 }
                                 QPixmap picture;
                                 if (picture.loadFromData(reply->readAll()))
                                 {
                                     this->avatar_->setPixmap(picture);
                                 }
                             });
        },
        [] {
            // Without the profile the card still shows the name and messages
        });
}

void RepeatSpamPopup::restartCountdown()
{
    const auto seconds =
        std::max(0, getSettings()->repeatAlertAutoClose.getValue());
    this->totalMs_ = qint64(seconds) * 1000;
    this->remainingMs_ = this->totalMs_;
    this->pointerAtStart_ = QCursor::pos();
    this->pointerMoved_ = false;

    auto *bar = static_cast<CountdownBar *>(this->countdownBar_);
    bar->setVisible(this->totalMs_ > 0);
    bar->setState(1.0, this->theme->accent);

    if (this->totalMs_ > 0)
    {
        this->sinceTick_.start();
        this->countdown_.start();
    }
    else
    {
        this->countdown_.stop();
    }
}

void RepeatSpamPopup::tick()
{
    const auto elapsed = this->sinceTick_.restart();
    if (this->remainingMs_ <= 0 || this->totalMs_ <= 0)
    {
        return;
    }

    auto *bar = static_cast<CountdownBar *>(this->countdownBar_);
    const auto fraction = double(this->remainingMs_) / double(this->totalMs_);

    // Held while the pointer rests on the window, so it cannot vanish from
    // under a click on the timeout button - the bar greys out to show it.
    // Only once the pointer has moved, though: a window that happened to open
    // under it would otherwise stay open for good.
    const auto pointer = QCursor::pos();
    this->pointerMoved_ = this->pointerMoved_ || pointer != this->pointerAtStart_;
    if (this->pointerMoved_ && this->frameGeometry().contains(pointer))
    {
        bar->setState(fraction, QColor(150, 150, 150));
        return;
    }

    this->remainingMs_ -= elapsed;
    if (this->remainingMs_ <= 0)
    {
        this->close();
        return;
    }
    bar->setState(double(this->remainingMs_) / double(this->totalMs_),
                  this->theme->accent);
}

}  // namespace chatterino
