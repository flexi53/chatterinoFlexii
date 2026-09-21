// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/BadgeButton.hpp"

#include "common/Channel.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Theme.hpp"
#include "widgets/dialogs/BadgePicker.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace chatterino {

BadgeButton::BadgeButton(BaseWidget *parent)
    : Button(parent)
{
    this->refreshTooltip();

    QObject::connect(this, &Button::leftClicked, this, [this] {
        this->openPicker();
    });
}

void BadgeButton::setChannel(const std::shared_ptr<Channel> &channel)
{
    if (channel == this->channel_.lock())
    {
        return;
    }
    this->channel_ = channel;
    this->name_ = channel != nullptr ? channel->getName() : QString();
    this->knownFor_.clear();
    this->worn_.reset();
    this->picture_ = {};
    this->refreshTooltip();
    this->update();

    // Twitch says which channel it is a moment after it was joined - with
    // the chat's modes, so that is when to ask what is worn there
    this->joined_.reset();
    if (auto *twitch = dynamic_cast<TwitchChannel *>(channel.get()))
    {
        this->joined_.emplace(twitch->roomModesChanged.connect([this] {
            QMetaObject::invokeMethod(
                this,
                [this] {
                    if (this->isVisible() &&
                        this->knownFor_ != this->channelId())
                    {
                        this->refresh();
                    }
                },
                Qt::QueuedConnection);
        }));
    }

    // Only asked while it is to be seen - a hidden one costs nothing
    if (this->isVisible())
    {
        this->refresh();
    }
}

QString BadgeButton::channelId() const
{
    auto channel = this->channel_.lock();
    auto *twitch = dynamic_cast<TwitchChannel *>(channel.get());
    return twitch != nullptr ? twitch->roomId() : QString();
}

void BadgeButton::showEvent(QShowEvent *event)
{
    Button::showEvent(event);
    if (this->knownFor_.isEmpty() || this->knownFor_ != this->channelId())
    {
        this->refresh();
    }
}

void BadgeButton::refresh()
{
    const auto id = this->channelId();
    if (id.isEmpty() || this->asking_)
    {
        return;
    }
    this->asking_ = true;
    webbadges::fetchChoices(id, this,
                            [this, id](const webbadges::Choices &choices) {
                                this->asking_ = false;
                                if (id != this->channelId())
                                {
                                    return;
                                }
                                if (choices.problem.isEmpty())
                                {
                                    this->knownFor_ = id;
                                    this->showWorn(choices.shown());
                                }
                                if (!this->picker_.isNull())
                                {
                                    this->picker_->setChoices(choices);
                                }
                            });
}

void BadgeButton::openPicker()
{
    if (!this->picker_.isNull())
    {
        this->picker_->close();
        return;
    }

    const auto id = this->channelId();
    auto *picker = new BadgePicker(this->name_, id, this);
    this->picker_ = picker;
    picker->onWorn = [this](const std::optional<webbadges::Badge> &shown) {
        this->showWorn(shown);
    };
    picker->show();

    if (id.isEmpty())
    {
        webbadges::Choices waiting;
        waiting.problem = QStringLiteral(
            "Der Kanal lädt noch - gleich noch einmal versuchen.");
        picker->setChoices(waiting);
        return;
    }
    // Asked afresh each time it opens, so a badge won meanwhile is there
    this->asking_ = false;
    this->refresh();
}

void BadgeButton::showWorn(const std::optional<webbadges::Badge> &badge)
{
    this->worn_ = badge;
    this->picture_ = {};
    this->refreshTooltip();
    this->update();
    if (!badge)
    {
        return;
    }

    const auto url = badge->image;
    webbadges::picture(url, this, [this, url](const QPixmap &picture) {
        if (this->worn_ && this->worn_->image == url)
        {
            this->picture_ = picture;
            this->update();
        }
    });
}

void BadgeButton::refreshTooltip()
{
    const auto where = this->name_.isEmpty()
                           ? QStringLiteral("diesem Kanal")
                           : QStringLiteral("#%1").arg(this->name_);
    if (this->worn_)
    {
        this->setToolTip(QStringLiteral("Chat-Identität - in %1 trägst du "
                                        "gerade „%2“.")
                             .arg(where, this->worn_->title));
    }
    else
    {
        this->setToolTip(
            QStringLiteral("Chat-Identität: Badge wählen, das du in %1 "
                           "trägst")
                .arg(where));
    }
}

QPoint BadgeButton::placeMenu(const QRect &button, const QSize &menu,
                              const QRect &screen)
{
    // Upwards, as the input bar sits at the bottom - downwards only when
    // the screen ends above it
    int y = button.top() - menu.height();
    if (y < screen.top())
    {
        y = button.bottom() + 1;
    }
    const int x = std::max(
        screen.left(),
        std::min(button.left(), screen.left() + screen.width() - menu.width()));
    y = std::max(screen.top(),
                 std::min(y, screen.top() + screen.height() - menu.height()));
    return {x, y};
}

void BadgeButton::paintContent(QPainter &painter)
{
    const auto scale = this->scale();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // As big as the icons next to it, inside the same padding
    const qreal side =
        std::min(this->width() - (12 * scale), this->height() - (4 * scale));
    QRectF box(0, 0, side, side);
    box.moveCenter(QRectF(this->rect()).center());

    // The badge worn here, as it is seen in the chat
    if (!this->picture_.isNull())
    {
        painter.setOpacity(this->mouseOver() ? 1.0 : 0.9);
        painter.drawPixmap(box, this->picture_, QRectF(this->picture_.rect()));
        return;
    }

    // Until then a plain badge: a shield-like shape, quiet like the icons
    // next to it
    QColor color =
        getTheme()->isLightTheme() ? QColor("#333333") : QColor("#e6e6e6");
    if (!this->mouseOver())
    {
        color.setAlpha(200);
    }
    QPen pen(color);
    pen.setWidthF(1.4 * scale);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const auto x = [&](qreal part) {
        return box.left() + (box.width() * part);
    };
    const auto y = [&](qreal part) {
        return box.top() + (box.height() * part);
    };
    QPainterPath shape;
    shape.addRoundedRect(
        QRectF(QPointF(x(0.2), y(0.14)), QPointF(x(0.8), y(0.86))), 2 * scale,
        2 * scale);
    painter.drawPath(shape);
    painter.drawLine(QPointF(x(0.36), y(0.5)), QPointF(x(0.64), y(0.5)));
}

}  // namespace chatterino
