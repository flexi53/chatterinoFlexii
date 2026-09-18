// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/sound/ISoundController.hpp"
#include "common/FlagsEnum.hpp"
#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/ModerationAssistant.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"

#ifdef Q_OS_MACOS
#    include "util/MacOsHelpers.h"
#endif

#include "singletons/WindowManager.hpp"
#include "widgets/Window.hpp"
#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Paths.hpp"
#include "util/RoundPixmap.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/FormatTime.hpp"
#include "widgets/buttons/PixmapButton.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/Label.hpp"
#include "util/WidgetHelpers.hpp"

#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QSizeGrip>
#include <QEvent>
#include <QLocale>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
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

/// Alerts can be kept above every other program, so one that comes up while
/// the stream is in front does not open unseen behind it
FlagsEnum<BaseWindow::Flags> alertWindowFlags()
{
    FlagsEnum<BaseWindow::Flags> flags(BaseWindow::EnableCustomFrame,
                                       BaseWindow::DisableLayoutSave,
                                       BaseWindow::BoundsCheckOnShow);
    flags.set(BaseWindow::TopMost,
              getSettings()->modAlertAlwaysOnTop.getValue());
    return flags;
}

/// How far each alert opening while others are up sits from the one before
constexpr int CASCADE_STEP = 28;

QHash<QString, QPointer<ModAlertPopup>> &openAlerts()
{
    static QHash<QString, QPointer<ModAlertPopup>> alerts;
    return alerts;
}

QString alertKey(const QString &channel, const QString &login)
{
    return channel.toLower() + '\n' + login.toLower();
}

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

/// Remembers the size a moderator drags an alert window to and the place they
/// move it to, so the next ones open the same. Only what they chose counts - a
/// window that closes the way it came up leaves the settings as they were.
class GeometryKeeper : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Hide)
        {
            const auto *window = static_cast<QWidget *>(watched);
            const auto shown = window->property("alertShownSize").toSize();
            if (shown.isValid() && window->size() != shown)
            {
                getSettings()->modAlertWidth.setValue(window->width());
                getSettings()->modAlertHeight.setValue(window->height());
            }
            const auto shownAt = window->property("alertShownPos");
            if (shownAt.isValid() && window->pos() != shownAt.toPoint())
            {
                getSettings()->modAlertX.setValue(window->x());
                getSettings()->modAlertY.setValue(window->y());
                getSettings()->modAlertPositionSaved.setValue(true);
            }
        }
        return false;
    }
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

QString grey(const QString &html)
{
    return QStringLiteral("<span style=\"color:#9a9a9a\">%1</span>").arg(html);
}

QString elided(const QString &text, qsizetype length)
{
    return text.size() > length ? text.left(length - 1) + QStringLiteral("…")
                                : text;
}

/// "Closest case · 86% alike · 11 Sep 21:53", what they wrote, what they got
QString describeMatch(const ModMatch &match, bool html)
{
    const auto &modCase = match.modCase;
    const auto when = QLocale(QLocale::German)
                          .toString(modCase.time, QStringLiteral("d. MMM HH:mm"));
    const auto outcome =
        modCase.seconds > 0
            ? QStringLiteral("Timeout %1").arg(formatTime(modCase.seconds))
            : QStringLiteral("Bann");
    const auto by = modCase.moderator.isEmpty()
                        ? QString()
                        : QStringLiteral(" von %1").arg(modCase.moderator);
    const auto percent = qRound(match.similarity * 100);

    if (!html)
    {
        return QStringLiteral("%1 % ähnlich · %2 · %3: %4 → %5%6")
            .arg(QString::number(percent), when, modCase.user,
                 elided(match.matchedMessage, 80), outcome, by);
    }

    auto result =
        QStringLiteral("Ähnlichster Fall &middot; %1 % ähnlich &middot; "
                       "%2<br>%3<br>&rarr; %4%5")
            .arg(QString::number(percent), when,
                 grey(QStringLiteral("%1: %2").arg(
                     modCase.user.toHtmlEscaped(),
                     elided(match.matchedMessage, 90).toHtmlEscaped())),
                 outcome, by.toHtmlEscaped());
    if (!modCase.reasons.isEmpty())
    {
        QStringList labels;
        for (const auto &reason : modCase.reasons)
        {
            labels.append(ModerationAssistant::reasonLabel(reason));
        }
        result += grey(QStringLiteral(" &middot; %1")
                           .arg(labels.join(", ").toHtmlEscaped()));
    }
    return result;
}

QString capitalized(QString text)
{
    if (!text.isEmpty())
    {
        text[0] = text[0].toUpper();
    }
    return text;
}

/// What a suggestion rests on: the reason in a few words, the closest case
/// underneath it, and the next closest cases for the tooltip
struct SuggestionReason {
    QString reason;
    QString details;
    QString tooltip;
};

SuggestionReason describeSuggestion(const ModSuggestion &suggestion)
{
    QString shared;
    if (!suggestion.closest.empty() &&
        !suggestion.closest.front().sharedWords.isEmpty())
    {
        QStringList quoted;
        for (const auto &word :
             suggestion.closest.front().sharedWords.mid(0, 5))
        {
            quoted.append(word == QStringLiteral("a link")
                              ? QStringLiteral("einen Link")
                              : QStringLiteral("&bdquo;%1&ldquo;")
                                    .arg(word.toHtmlEscaped()));
        }
        shared = QStringLiteral("teilt %1 mit früheren Fällen")
                     .arg(quoted.join(QStringLiteral(", ")));
    }

    SuggestionReason result;
    QStringList details;
    // What can be seen in the message itself leads; the words it has in
    // common with earlier cases only when there is nothing else to name
    if (!suggestion.messageReasons.isEmpty())
    {
        QStringList reasons;
        for (const auto &reason : suggestion.messageReasons)
        {
            reasons.append(
                ModerationAssistant::reasonLabel(reason).toHtmlEscaped());
        }
        result.reason =
            capitalized(reasons.join(QStringLiteral(" &middot; ")));
        if (!shared.isEmpty())
        {
            details.append(capitalized(shared));
        }
    }
    else if (!shared.isEmpty())
    {
        result.reason = capitalized(shared);
    }
    else
    {
        result.reason = QStringLiteral("Ähnelt früheren Fällen");
    }
    if (!suggestion.closest.empty())
    {
        details.append(describeMatch(suggestion.closest.front(), true));
    }
    result.details = details.join(QStringLiteral("<br>"));

    QStringList others;
    for (size_t i = 1; i < suggestion.closest.size(); i++)
    {
        others.append(describeMatch(suggestion.closest[i], false));
    }
    if (!others.isEmpty())
    {
        result.tooltip =
            QStringLiteral("Ebenfalls ähnlich:\n") + others.join('\n');
    }
    return result;
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

/// A chat line for the tests, built the way a real one reads
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

ModAlertPopup::ModAlertPopup(QString channel, QString login, QWidget *parent)
    : BasePopup(alertWindowFlags(), parent)
    , channel_(std::move(channel))
    , login_(std::move(login))
{
    this->setWindowTitle(QStringLiteral("#%1").arg(this->channel_));
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

    // Why the window came up, set off in the alert's colour so it is taken in
    // at a glance
    this->reasonBox_ = new QFrame;
    this->reasonBox_->setObjectName(QStringLiteral("reasonBox"));
    auto *reasonLayout = new QVBoxLayout(this->reasonBox_);
    reasonLayout->setContentsMargins(10, 8, 10, 8);
    reasonLayout->setSpacing(5);
    auto *reasonRow = new QHBoxLayout;
    reasonRow->setSpacing(9);
    this->reasonTag_ = new QLabel(QStringLiteral("REASON"));
    this->reasonTag_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *glow = new QGraphicsDropShadowEffect(this->reasonTag_);
    glow->setOffset(0, 0);
    glow->setBlurRadius(18);
    this->reasonTag_->setGraphicsEffect(glow);
    reasonRow->addWidget(this->reasonTag_, 0, Qt::AlignVCenter);
    this->reason_ = new QLabel;
    this->reason_->setTextFormat(Qt::RichText);
    this->reason_->setWordWrap(true);
    auto reasonFont = this->reason_->font();
    reasonFont.setBold(true);
    reasonFont.setPointSizeF(reasonFont.pointSizeF() * 1.15);
    this->reason_->setFont(reasonFont);
    reasonRow->addWidget(this->reason_, 1);
    reasonLayout->addLayout(reasonRow);
    this->reasonDetails_ = new QLabel;
    this->reasonDetails_->setTextFormat(Qt::RichText);
    this->reasonDetails_->setWordWrap(true);
    reasonLayout->addWidget(this->reasonDetails_);
    this->reasonBox_->hide();
    layout->addWidget(this->reasonBox_);

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
        QStringLiteral("<i style=\"color:#9a9a9a\">Test - ausgedachte "
                       "Nachrichten, die Knöpfe tun nichts.</i>"));
    this->testNote_->setTextFormat(Qt::RichText);
    this->testNote_->hide();
    layout->addWidget(this->testNote_);

    this->countdownBar_ = new CountdownBar(this);
    layout->addWidget(this->countdownBar_);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    this->ignore_ = new QPushButton(QStringLiteral("Ignorieren"));
    this->timeout_ = new QPushButton;
    buttons->addWidget(this->ignore_);
    buttons->addWidget(this->timeout_);
    // Something to grab, since the window's size is meant to be changed
    buttons->addWidget(new QSizeGrip(this), 0,
                       Qt::AlignBottom | Qt::AlignRight);
    layout->addLayout(buttons);

    QObject::connect(this->ignore_, &QPushButton::clicked, this, [this] {
        this->close();
    });

    QObject::connect(this->timeout_, &QPushButton::clicked, this, [this] {
        auto channel = getApp()->getTwitch()->getChannelOrEmpty(this->channel_);
        if (!this->test_ && !channel->isEmpty())
        {
            QStringList commands;
            if (this->seconds_ < 0)
            {
                for (const auto &id : this->deleteIds_)
                {
                    commands.append(QStringLiteral("/delete %1").arg(id));
                }
            }
            else if (this->seconds_ > 0)
            {
                commands.append(QStringLiteral("/timeout %1 %2")
                                    .arg(this->login_)
                                    .arg(this->seconds_));
            }
            else
            {
                commands.append(QStringLiteral("/ban %1").arg(this->login_));
            }

            for (auto command : commands)
            {
                command = getApp()->getCommands()->execCommand(command, channel,
                                                               false);
                channel->sendMessage(command);
            }
        }
        this->close();
    });

    this->countdown_.setInterval(TICK_MS);
    QObject::connect(&this->countdown_, &QTimer::timeout, this, [this] {
        this->tick();
    });

    // The size a moderator last dragged one to, where they did
    const auto width = getSettings()->modAlertWidth.getValue();
    const auto height = getSettings()->modAlertHeight.getValue();
    if (width > 0 || height > 0)
    {
        const auto natural = this->sizeHint();
        this->resize(width > 0 ? width : natural.width(),
                     height > 0 ? height : natural.height());
    }
    this->installEventFilter(new GeometryKeeper(this));
}

ModAlertPopup *ModAlertPopup::openFor(const QString &channel,
                                      const QString &login)
{
    return openAlerts().value(alertKey(channel, login)).data();
}

ModAlertPopup *ModAlertPopup::obtain(const QString &channel,
                                     const QString &login, QWidget *parent)
{
    if (auto *open = openFor(channel, login))
    {
        return open;
    }

    auto *popup = new ModAlertPopup(channel, login, parent);
    openAlerts().insert(alertKey(channel, login), popup);
    return popup;
}

void ModAlertPopup::closeFor(const QString &channel, const QString &login)
{
    if (auto *open = openFor(channel, login))
    {
        open->close();
    }
}

void ModAlertPopup::present()
{
    // The ping goes with a window coming up, not with one already on screen
    // being updated. Test windows come through here too, so they sound like
    // the real thing.
    if (!this->announced_)
    {
        this->announced_ = true;
        if (getSettings()->modAlertSound)
        {
            getApp()->getSound()->play(soundFor(this->kind_));
        }
    }

#ifdef Q_OS_MACOS
    const bool onAllSpaces = getSettings()->modAlertAlwaysOnTop.getValue();
    // winId creates the native window, so what is set on it holds before it
    // first shows
    const auto view = static_cast<std::uintptr_t>(this->winId());
    if (onAllSpaces)
    {
        chatterinoShowOnAllSpaces(view);
    }

    const auto showHere = [this, onAllSpaces] {
        const auto nativeView = static_cast<std::uintptr_t>(this->winId());
        this->placeWindow();
        this->show();
        this->rememberShownGeometry();
        // Qt's raise() activates the whole app on macOS, which would take
        // the keyboard from whatever the moderator is typing into
        chatterinoOrderFrontWithoutActivating(nativeView);
        if (onAllSpaces)
        {
            chatterinoShowOnAllSpaces(nativeView);
        }
    };

    // With the app on another space - a full screen app in front, say - a
    // window shown now would open on that space and stay hidden there. Bring
    // the app back first and show the window once its space is up.
    auto &mainWindow = getApp()->getWindows()->getMainWindow();
    if (!chatterinoIsOnActiveSpace(
            static_cast<std::uintptr_t>(mainWindow.winId())))
    {
        chatterinoActivateApp();
        QTimer::singleShot(600, this, showHere);
        return;
    }

    showHere();
#else
    this->placeWindow();
    this->show();
    this->rememberShownGeometry();
    this->raise();
#endif
}

void ModAlertPopup::placeWindow()
{
    // Only before it first shows: an alert being updated stays where it is,
    // wherever the moderator has put it since
    if (this->placed_)
    {
        return;
    }
    this->placed_ = true;
    if (!getSettings()->modAlertPositionSaved)
    {
        return;
    }

    // Alerts already up keep the saved place, so each further one steps down
    // and to the right rather than hiding one behind it
    int others = 0;
    for (const auto &alert : openAlerts())
    {
        if (!alert.isNull() && alert.data() != this && alert->isVisible())
        {
            ++others;
        }
    }
    const QPoint saved(getSettings()->modAlertX.getValue(),
                       getSettings()->modAlertY.getValue());
    // Bounds checked, so a place on a screen since unplugged still ends up on
    // one that is there
    this->moveTo(saved + QPoint(CASCADE_STEP, CASCADE_STEP) * others,
                 widgets::BoundsChecking::DesiredPosition);
}

void ModAlertPopup::rememberShownGeometry()
{
    // Only the first time: an alert updated after the moderator resized or
    // moved it must not pass that off as the way it came up
    if (!this->property("alertShownSize").isValid())
    {
        this->setProperty("alertShownSize", this->size());
        this->setProperty("alertShownPos", this->pos());
    }
}

int ModAlertPopup::openCount()
{
    auto &alerts = openAlerts();
    for (auto it = alerts.begin(); it != alerts.end();)
    {
        it = it->isNull() ? alerts.erase(it) : std::next(it);
    }
    return static_cast<int>(alerts.size());
}

void ModAlertPopup::setCase(const QString &displayName, int seconds,
                            int timeoutsServed)
{
    this->setWindowTitle(
        QStringLiteral("Wiederholte Nachricht – #%1").arg(this->channel_));
    this->kind_ = Kind::RepeatedMessage;

    if (timeoutsServed == 0)
    {
        this->headline_->setText(
            QStringLiteral("Hat dieselbe Nachricht mehrmals hintereinander "
                           "geschickt."));
    }
    else if (timeoutsServed == 1)
    {
        this->headline_->setText(
            QStringLiteral("Hat die Nachricht nach einem Timeout wieder "
                           "geschickt."));
    }
    else
    {
        this->headline_->setText(
            QStringLiteral("Hat die Nachricht nach %1 Timeouts wieder "
                           "geschickt.")
                .arg(timeoutsServed));
    }

    const auto steps = RepeatSpamDetector::steps();
    this->setReason(
        QStringLiteral("Dieselbe Nachricht wiederholt"),
        QStringLiteral("Stufe %1 von %2")
            .arg(std::min<size_t>(static_cast<size_t>(timeoutsServed),
                                  steps.size() - 1) +
                 1)
            .arg(steps.size()));

    this->showChatter(displayName);
    this->setAction(seconds);
    this->showRecentLines();
    this->restartCountdown();
}

void ModAlertPopup::setSuggestion(const QString &displayName,
                                  const ModSuggestion &suggestion)
{
    this->setWindowTitle(
        QStringLiteral("Mod-Assistent – #%1").arg(this->channel_));
    this->kind_ = Kind::Suggestion;
    this->showChatter(displayName);
    this->applySuggestion(suggestion);
    this->showRecentLines();
    this->restartCountdown();
}

void ModAlertPopup::applySuggestion(const ModSuggestion &suggestion)
{
    this->headline_->setText(
        QStringLiteral("Ähnelt %1 früheren Fällen in diesem Kanal. Mods "
                       "gaben %2.")
            .arg(suggestion.similarCases)
            .arg(suggestion.spread));

    const auto described = describeSuggestion(suggestion);
    this->setReason(described.reason, described.details, described.tooltip);
    this->setAction(suggestion.seconds);
}

void ModAlertPopup::setReason(const QString &reason, const QString &details,
                              const QString &tooltip)
{
    const auto color = reasonColor(this->kind_);
    this->reasonColor_ = color;

    this->reasonBox_->setStyleSheet(
        QStringLiteral("QFrame#reasonBox { background: rgba(%1, %2, %3, 40); "
                       "border: 1px solid rgba(%1, %2, %3, 120); "
                       "border-left: 4px solid %4; border-radius: 6px; }")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.name()));
    this->reasonTag_->setStyleSheet(reasonTagStyle(color));
    if (auto *glow = qobject_cast<QGraphicsDropShadowEffect *>(
            this->reasonTag_->graphicsEffect()))
    {
        glow->setColor(color);
    }

    // Lit up text reads on a dark theme; on a light one it would glare, so
    // the text goes deep in the same colour and the box carries the light
    const auto textColor =
        this->theme->isLightTheme() ? color.darker(190) : color;
    this->reason_->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
                               .arg(textColor.name(), reason));
    this->reasonDetails_->setText(details);
    this->reasonDetails_->setVisible(!details.isEmpty());
    this->reasonBox_->setToolTip(tooltip);
    this->reasonBox_->setVisible(!reason.isEmpty());
}

QColor ModAlertPopup::defaultReasonColor(Kind kind)
{
    // The settings' own defaults, so the colours are written down only once
    const auto *settings = getSettings();
    switch (kind)
    {
        case Kind::RepeatedMessage:
            return {settings->modAlertColorRepeat.getDefaultValue()};
        case Kind::EmoteSpam:
            return {settings->modAlertColorEmote.getDefaultValue()};
        case Kind::Suggestion:
        default:
            return {settings->modAlertColorSuggestion.getDefaultValue()};
    }
}

QColor ModAlertPopup::reasonColor(Kind kind)
{
    const auto *settings = getSettings();
    QString picked;
    switch (kind)
    {
        case Kind::RepeatedMessage:
            picked = settings->modAlertColorRepeat.getValue();
            break;
        case Kind::EmoteSpam:
            picked = settings->modAlertColorEmote.getValue();
            break;
        case Kind::Suggestion:
        default:
            picked = settings->modAlertColorSuggestion.getValue();
            break;
    }
    return vividColor(QColor(picked), defaultReasonColor(kind));
}

QColor ModAlertPopup::vividColor(const QColor &picked, const QColor &fallback)
{
    const auto hasHue = [](const QColor &color) {
        return color.isValid() && color.hsvHue() >= 0 &&
               color.hsvSaturationF() >= 0.12F && color.valueF() >= 0.12F;
    };
    const auto source = hasHue(picked) ? picked : fallback;
    return QColor::fromHsvF(source.hsvHueF(),
                            std::max(0.8F, source.hsvSaturationF()), 1.0F);
}

QString ModAlertPopup::reasonTagStyle(const QColor &color)
{
    // Dark lettering on the light colours, white on the deep ones like blue
    const auto luminance =
        0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue();
    return QStringLiteral("QLabel { background: %1; color: %2; "
                          "border-radius: 4px; padding: 2px 8px; "
                          "font-weight: bold; }")
        .arg(color.name(), luminance > 140 ? QStringLiteral("#101010")
                                           : QStringLiteral("#ffffff"));
}

std::vector<std::pair<QString, QString>> ModAlertPopup::builtInSounds()
{
    return {
        {QStringLiteral("builtin:zweiton"), QStringLiteral("Zweiton")},
        {QStringLiteral("builtin:glocke"), QStringLiteral("Glocke")},
        {QStringLiteral("builtin:dringend"), QStringLiteral("Dringend")},
        {QStringLiteral("builtin:tief"), QStringLiteral("Tief")},
        {QStringLiteral("builtin:kurz"), QStringLiteral("Kurz")},
    };
}

QUrl ModAlertPopup::soundUrl(const QString &choice)
{
    const QUrl ping(QStringLiteral("qrc:/sounds/ping2.wav"));
    if (choice.startsWith(QStringLiteral("builtin:")))
    {
        const auto name =
            QStringLiteral("alert-%1.wav").arg(choice.mid(QStringLiteral("builtin:").size()));
        const QString resource = QStringLiteral(":/sounds/") + name;
        if (!QFileInfo::exists(resource))
        {
            return ping;
        }

        const QDir folder(QDir(getApp()->getPaths().miscDirectory)
                              .absoluteFilePath(QStringLiteral("alert-sounds")));
        const auto path = folder.absoluteFilePath(name);
        // Copied again when a newer app brings a different sound
        if (QFileInfo(path).size() != QFileInfo(resource).size())
        {
            QDir().mkpath(folder.absolutePath());
            QFile::remove(path);
            if (!QFile::copy(resource, path))
            {
                return ping;
            }
        }
        return QUrl::fromLocalFile(path);
    }

    if (!choice.isEmpty() && QFileInfo::exists(choice))
    {
        return QUrl::fromLocalFile(choice);
    }
    return ping;
}

QUrl ModAlertPopup::soundFor(Kind kind)
{
    const auto *settings = getSettings();
    switch (kind)
    {
        case Kind::RepeatedMessage:
            return soundUrl(settings->modAlertSoundRepeat.getValue());
        case Kind::EmoteSpam:
            return soundUrl(settings->modAlertSoundEmote.getValue());
        case Kind::Suggestion:
        default:
            return soundUrl(settings->modAlertSoundSuggestion.getValue());
    }
}

void ModAlertPopup::showChatter(const QString &displayName)
{
    this->test_ = false;
    this->testNote_->hide();
    this->name_->setText(displayName);

    if (!this->profileLoaded_)
    {
        this->avatar_->setPixmap(
            initialAvatar(displayName, this->theme->accent));
        this->loadProfile();
    }
}

void ModAlertPopup::showRecentLines()
{
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
}

void ModAlertPopup::showTestChatter(const QString &title)
{
    this->test_ = true;
    this->profileLoaded_ = true;
    this->testNote_->show();
    this->setWindowTitle(title);

    const QString name = QStringLiteral("TestUser");
    this->name_->setText(name);
    this->details_->setText(QStringLiteral(
        "testuser &middot; <span style=\"color:#ffaa00\">Account vor 2 Tagen "
        "erstellt</span>"));
    this->avatar_->setPixmap(initialAvatar(name, this->theme->accent));

    this->liveMessages_.reset();
    this->view_ = std::make_shared<Channel>(QStringLiteral("test"),
                                            Channel::Type::None);
}

void ModAlertPopup::showTestCase(bool afterTimeout)
{
    this->kind_ = Kind::RepeatedMessage;
    this->showTestChatter(QStringLiteral("Wiederholte Nachricht – Test"));

    const QString name = QStringLiteral("TestUser");
    const auto now = QDateTime::currentDateTime();
    const auto steps = RepeatSpamDetector::steps();

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
        this->headline_->setText(
            QStringLiteral("Hat die Nachricht nach einem Timeout wieder "
                           "geschickt."));
        this->setAction(steps[std::min<size_t>(1, steps.size() - 1)]);
    }
    else
    {
        this->headline_->setText(
            QStringLiteral("Hat dieselbe Nachricht mehrmals hintereinander "
                           "geschickt."));
        this->setAction(steps.front());
    }
    this->setReason(
        QStringLiteral("Dieselbe Nachricht wiederholt"),
        QStringLiteral("Stufe %1 von %2")
            .arg(afterTimeout ? std::min<size_t>(2, steps.size()) : 1)
            .arg(steps.size()));

    this->messages_->setChannel(this->view_);
    this->restartCountdown();
}

void ModAlertPopup::showTestSuggestion()
{
    this->showTestChatter(QStringLiteral("Mod-Assistent – Test"));

    const QString name = QStringLiteral("TestUser");
    const auto now = QDateTime::currentDateTime();

    this->view_->addMessage(makeTestMessage(name, "hey chat", now.addSecs(-20)),
                            MessageContext::Original);
    this->view_->addMessage(
        makeTestMessage(name, "gratis follower bei www.example.com", now),
        MessageContext::Original);

    // A made up suggestion, shown through the same path a real one takes
    ModSuggestion suggestion{
        .seconds = 300,
        .similarCases = 7,
        .spread = QStringLiteral("5m ×6, 1h ×1"),
        .messageReasons = {QStringLiteral("link")},
    };

    const auto makeMatch = [&](const QString &user, const QString &moderator,
                               const QString &message, int seconds,
                               double similarity, qint64 daysAgo) {
        ModMatch match;
        match.modCase.time = now.addDays(-daysAgo).addSecs(-3600);
        match.modCase.user = user;
        match.modCase.moderator = moderator;
        match.modCase.seconds = seconds;
        match.modCase.messages = {message};
        match.modCase.reasons = ModerationAssistant::detectReasons({message});
        match.similarity = similarity;
        match.matchedMessage = message;
        match.sharedWords = {QStringLiteral("follower"),
                             QStringLiteral("gratis"), QStringLiteral("a link")};
        return match;
    };
    suggestion.closest.push_back(makeMatch("spammer99", "modxy",
                                           "gratis follower bei www.example.net",
                                           300, 0.86, 2));
    suggestion.closest.push_back(makeMatch(
        "werbebot", "", "follower gratis hier www.example.org", 300, 0.71, 4));
    suggestion.closest.push_back(makeMatch(
        "followme", "modxy", "gratis viewer und follower", 3600, 0.62, 5));

    this->applySuggestion(suggestion);
    this->messages_->setChannel(this->view_);
    this->restartCountdown();
}

void ModAlertPopup::setAction(int seconds)
{
    this->seconds_ = seconds;
    if (seconds < 0)
    {
        this->timeout_->setText(
            this->deleteIds_.size() > 1
                ? QStringLiteral("%1 Nachrichten löschen")
                      .arg(this->deleteIds_.size())
                : QStringLiteral("Nachricht löschen"));
    }
    else
    {
        this->timeout_->setText(
            seconds > 0 ? QStringLiteral("Timeout %1").arg(formatTime(seconds))
                        : QStringLiteral("Bannen"));
    }
    this->timeout_->setDefault(true);
}

void ModAlertPopup::setEmoteSpam(const QString &displayName, int emotes,
                                 int messages, int window, int action,
                                 const QStringList &messageIds,
                                 int actionsServed, int stepCount)
{
    this->setWindowTitle(QStringLiteral("Emote-Spam – #%1").arg(this->channel_));
    this->kind_ = Kind::EmoteSpam;
    this->deleteIds_ = messageIds;

    this->showChatter(displayName);
    this->applyEmoteSpam(emotes, messages, window, action, actionsServed,
                         stepCount);
    this->showRecentLines();
    this->restartCountdown();
}

void ModAlertPopup::applyEmoteSpam(int emotes, int messages, int window,
                                   int action, int actionsServed, int stepCount)
{
    this->headline_->setText(
        messages <= 1
            ? QStringLiteral("Hat eine Nachricht mit %1 Emotes geschickt.")
                  .arg(emotes)
            : QStringLiteral("Hat %1 Emotes in %2 Nachrichten in unter %3 "
                             "Sekunden geschickt.")
                  .arg(emotes)
                  .arg(messages)
                  .arg(window));
    this->setReason(
        QStringLiteral("Emote-Spam"),
        QStringLiteral("%1 Emotes in %2 s, Alarm ab %3 &middot; Stufe %4 von "
                       "%5")
            .arg(emotes)
            .arg(window)
            .arg(std::max(1, getSettings()->emoteAlertMinEmotes.getValue()))
            .arg(std::min(actionsServed, std::max(1, stepCount) - 1) + 1)
            .arg(std::max(1, stepCount)));
    this->setAction(action);
}

void ModAlertPopup::showTestEmoteSpam(int step)
{
    this->kind_ = Kind::EmoteSpam;
    this->showTestChatter(QStringLiteral("Emote-Spam – Test"));

    const QString name = QStringLiteral("TestUser");
    const auto now = QDateTime::currentDateTime();
    const auto steps = EmoteSpamDetector::steps();

    // Like a real flood: short bursts, now and then with a word in between
    const QStringList lines{
        QStringLiteral("🥕 👩‍🚒 🚒 🚒"),
        QStringLiteral("Möhrchen 🥕 🥕 🥕"),
        QStringLiteral("🥕 👩‍🚒 🥕"),
        QStringLiteral("peeeeeeteeeeer 🥕 👩‍🚒"),
    };
    for (qsizetype i = 0; i < lines.size(); i++)
    {
        this->view_->addMessage(
            makeTestMessage(name, lines[i],
                            now.addSecs(-12 * (lines.size() - 1 - i))),
            MessageContext::Original);
    }

    this->deleteIds_ = {QStringLiteral("test1"), QStringLiteral("test2"),
                        QStringLiteral("test3"), QStringLiteral("test4")};
    this->applyEmoteSpam(
        12, static_cast<int>(lines.size()),
        std::max(1, getSettings()->emoteAlertWindowSeconds.getValue()),
        steps[std::min<size_t>(static_cast<size_t>(step), steps.size() - 1)],
        step, static_cast<int>(steps.size()));

    this->messages_->setChannel(this->view_);
    this->restartCountdown();
}

void ModAlertPopup::loadProfile()
{
    this->profileLoaded_ = true;

    profilepictures::whenKnown(
        this->login_, this, [this](const TwitchProfile &profile) {
            if (!profile.createdAt.isValid())
            {
                return;
            }

            const auto days =
                profile.createdAt.daysTo(QDateTime::currentDateTimeUtc());
            const auto createdText =
                days <= 0   ? QStringLiteral("Account heute erstellt")
                : days == 1 ? QStringLiteral("Account gestern erstellt")
                            : QStringLiteral("Account vor %1 Tagen erstellt")
                                  .arg(days);

            this->details_->setText(
                QStringLiteral("%1 &middot; %2")
                    .arg(this->login_.toHtmlEscaped(),
                         days < NEW_ACCOUNT_DAYS
                             ? QStringLiteral(
                                   "<span style=\"color:#ffaa00\">%1</span>")
                                   .arg(createdText)
                             : createdText));
            this->details_->setToolTip(profile.createdAt.toString(Qt::ISODate));
        });

    profilepictures::pixmap(this->login_, AVATAR_SIZE * 2, this,
                            [this](const QPixmap &picture) {
                                this->avatar_->setPixmap(
                                    roundPixmap(picture, AVATAR_SIZE * 2));
                            });
}

void ModAlertPopup::restartCountdown()
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

void ModAlertPopup::tick()
{
    const auto elapsed = this->sinceTick_.restart();
    if (this->remainingMs_ <= 0 || this->totalMs_ <= 0)
    {
        return;
    }

    auto *bar = static_cast<CountdownBar *>(this->countdownBar_);
    const auto fraction = double(this->remainingMs_) / double(this->totalMs_);

    // Held while the pointer rests on the window, so it cannot vanish from
    // under a click on the button - the bar greys out to show it. Only once
    // the pointer has moved, though: a window that happened to open under it
    // would otherwise stay open for good.
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
                  this->reasonColor_.isValid() ? this->reasonColor_
                                               : this->theme->accent);
}

}  // namespace chatterino
