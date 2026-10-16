// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/TwitchActionsButton.hpp"

#include "providers/twitch/api/ModGql.hpp"
#include "singletons/Theme.hpp"

#include <QEvent>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace chatterino {

namespace {

/// How often the balance is asked for again while nothing here changed it -
/// watching earns points slowly
constexpr auto REFRESH_EVERY = std::chrono::minutes(5);

/// Room around the coin and the number inside the pill, unscaled
constexpr int PADDING = 6;
constexpr int GAP = 4;

}  // namespace

TwitchActionsButton::TwitchActionsButton(BaseWidget *parent)
    : Button(parent)
{
    this->refreshTooltip();

    this->refresh_.setInterval(REFRESH_EVERY);
    QObject::connect(&this->refresh_, &QTimer::timeout, this, [this] {
        this->fetch();
    });

    // Whoever asked - this button, the window, another split - the number
    // here follows
    this->connections_.managedConnect(
        modgql::balanceChanged(), [this](const QString &channel, qint64 points) {
            if (channel != this->channel_)
            {
                return;
            }
            this->balance_ = points;
            this->refreshTooltip();
            this->fitWidth();
            this->update();
        });
    this->connections_.managedConnect(modgql::loginChanged(), [this] {
        this->loggedIn_ = modgql::currentLogin().has_value();
        this->balance_.reset();
        this->refreshTooltip();
        this->fitWidth();
        this->update();
        this->fetch();
    });

    QPointer<TwitchActionsButton> self(this);
    modgql::whenReady(this, [self](bool loggedIn) {
        if (!self)
        {
            return;
        }
        self->loggedIn_ = loggedIn;
        self->refreshTooltip();
        self->fetch();
    });
}

void TwitchActionsButton::setChannel(const QString &channelLogin)
{
    const auto channel = channelLogin.toLower();
    if (channel == this->channel_)
    {
        return;
    }
    this->channel_ = channel;
    this->balance_.reset();
    this->refreshTooltip();
    this->fitWidth();
    this->update();
    this->fetch();
}

void TwitchActionsButton::setBaseSize(QSize size)
{
    this->baseSize_ = size;
    this->setFixedHeight(size.height());
    this->fitWidth();
}

QString TwitchActionsButton::pointsText(qint64 points)
{
    // The whole number, as twitch.tv shows it beside the chat
    return QLocale(QLocale::German).toString(points);
}

void TwitchActionsButton::fetch()
{
    if (!this->loggedIn_ || this->channel_.isEmpty())
    {
        this->refresh_.stop();
        return;
    }
    this->refresh_.start();
    // The answer comes back through balanceChanged; a channel without
    // points just leaves the coin alone
    modgql::channelPoints(
        this->channel_, [](const modgql::ChannelPoints &) {},
        [](const QString &) {});
}

void TwitchActionsButton::refreshTooltip()
{
    if (!this->loggedIn_)
    {
        this->setToolTip(QStringLiteral(
            "Kanalpunkte, und als Mod Umfragen und Vorhersagen starten. "
            "Braucht die Anmeldung unter Einstellungen → Twitch-Extras."));
        return;
    }
    if (this->balance_)
    {
        this->setToolTip(
            QStringLiteral("%1 Kanalpunkte in #%2 - klicken für die "
                           "Belohnungen")
                .arg(QLocale(QLocale::German).toString(*this->balance_),
                     this->channel_));
        return;
    }
    this->setToolTip(QStringLiteral("Kanalpunkte dieses Kanals"));
}

void TwitchActionsButton::fitWidth()
{
    if (!this->baseSize_.isValid())
    {
        return;
    }
    if (!this->balance_)
    {
        this->setFixedWidth(this->baseSize_.width());
        return;
    }
    // Laid out as paintContent draws it: the pill's edge, room, the coin, a
    // gap, the number, room, the edge - and a little air, so the last digit
    // is never cut off by rounding
    const auto scale = this->scale();
    const QFontMetricsF metrics(this->font());
    const auto text = metrics.horizontalAdvance(pointsText(*this->balance_));
    const auto coin = this->baseSize_.height() - (10 * scale);
    const auto needed = (2 * scale) + (PADDING * 2 * scale) + coin +
                        (GAP * scale) + text + (3 * scale);
    this->setFixedWidth(
        std::max(this->baseSize_.width(), int(std::ceil(needed))));
}

void TwitchActionsButton::changeEvent(QEvent *event)
{
    Button::changeEvent(event);
    if (event->type() == QEvent::FontChange)
    {
        this->fitWidth();
    }
}

void TwitchActionsButton::paintContent(QPainter &painter)
{
    const auto *theme = getTheme();
    const auto scale = this->scale();
    painter.setRenderHint(QPainter::Antialiasing);

    QColor text = theme->messages.textColors.regular;
    text.setAlpha(this->mouseOver() ? 255 : 210);
    // The colour Twitch gives its points, a touch deeper on a light theme
    const QColor coinColor =
        theme->isLightTheme() ? QColor("#7a3fd1") : QColor("#bf94ff");

    const QRectF whole(this->rect());
    const qreal side =
        std::min(whole.height() - (10 * scale), whole.width() - (12 * scale));

    // With a number it is a pill - coin and number side by side
    QRectF coin(0, 0, side, side);
    if (this->balance_)
    {
        auto pill = whole.adjusted(1 * scale, 3 * scale, -1 * scale,
                                   -3 * scale);
        QColor fill = coinColor;
        fill.setAlpha(this->mouseOver() ? 55 : 32);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
        coin.moveCenter(
            QPointF(pill.left() + (PADDING * scale) + (side / 2),
                    pill.center().y()));
    }
    else
    {
        coin.moveCenter(whole.center());
    }

    // Twitch's coin: a ring with a notch, so it reads as the points one
    QPen pen(coinColor);
    pen.setWidthF(1.6 * scale);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto inset = coin.width() * 0.08;
    const auto ring = coin.adjusted(inset, inset, -inset, -inset);
    painter.drawArc(ring, 120 * 16, 300 * 16);
    painter.setBrush(coinColor);
    painter.setPen(Qt::NoPen);
    const auto dot = ring.width() * 0.16;
    painter.drawEllipse(ring.center(), dot, dot);

    if (this->balance_)
    {
        painter.setPen(text);
        painter.setFont(this->font());
        const QRectF label(coin.right() + (GAP * scale), whole.top(),
                           whole.right() - coin.right() - (GAP * scale) -
                               (PADDING * scale),
                           whole.height());
        painter.drawText(label,
                         Qt::AlignVCenter | Qt::AlignLeft | Qt::TextDontClip,
                         pointsText(*this->balance_));
    }
}

}  // namespace chatterino
