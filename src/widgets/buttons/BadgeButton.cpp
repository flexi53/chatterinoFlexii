// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/BadgeButton.hpp"

#include "singletons/Theme.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"

#include <QActionGroup>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QToolTip>

#include <algorithm>

namespace chatterino {

BadgeButton::BadgeButton(BaseWidget *parent)
    : Button(parent)
{
    this->refreshTooltip();

    QObject::connect(this, &Button::leftClicked, this, [this] {
        this->refresh(true);
    });
}

void BadgeButton::setChannel(const QString &name, const QString &id)
{
    if (name == this->name_ && id == this->id_)
    {
        return;
    }
    this->name_ = name;
    this->id_ = id;
    if (this->knownFor_ != id)
    {
        this->worn_.reset();
        this->picture_ = {};
    }
    this->refreshTooltip();
    this->update();

    // Only asked while it is to be seen - a hidden one costs nothing
    if (this->isVisible())
    {
        this->refresh(false);
    }
}

void BadgeButton::showEvent(QShowEvent *event)
{
    Button::showEvent(event);
    if (this->knownFor_ != this->id_)
    {
        this->refresh(false);
    }
}

void BadgeButton::refresh(bool thenOpen)
{
    if (this->id_.isEmpty() || this->asking_)
    {
        return;
    }
    this->asking_ = true;
    const auto id = this->id_;
    webbadges::fetchChoices(
        id, this, [this, id, thenOpen](const webbadges::Choices &choices) {
            this->asking_ = false;
            if (id != this->id_)
            {
                return;
            }
            if (choices.problem.isEmpty())
            {
                this->knownFor_ = id;
                this->showWorn(choices.shown());
            }
            if (thenOpen)
            {
                this->openMenu(choices);
            }
        });
}

void BadgeButton::openMenu(const webbadges::Choices &choices)
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (!choices.problem.isEmpty())
    {
        menu->addAction(choices.problem)->setEnabled(false);
        menu->addAction("Einstellungen öffnen", this, [this] {
            SettingsDialog::showDialog(this);
        });
        menu->popup(this->mapToGlobal(QPoint(0, 0)));
        return;
    }

    // Twitch keeps one of each: one of the channel's, and one worn
    // everywhere - both are seen next to the name
    const auto addSection =
        [this, menu](const QString &title,
                     const std::vector<webbadges::Badge> &badges,
                     const std::optional<webbadges::Badge> &worn, bool global) {
            // A heading of its own - the Mac leaves out the text of a
            // menu section
            if (!menu->isEmpty())
            {
                menu->addSeparator();
            }
            auto *heading = menu->addAction(title);
            heading->setEnabled(false);
            auto font = heading->font();
            font.setBold(true);
            heading->setFont(font);
            if (badges.empty())
            {
                menu->addAction("Keins zur Auswahl")->setEnabled(false);
                return;
            }
            auto *group = new QActionGroup(menu);
            for (const auto &badge : badges)
            {
                auto *action = menu->addAction(
                    badge.title.isEmpty() ? badge.setID : badge.title);
                action->setCheckable(true);
                action->setChecked(worn && *worn == badge);
                // The badge's picture beside its name, on the Mac too
                action->setIconVisibleInMenu(true);
                group->addAction(action);
                QObject::connect(action, &QAction::triggered, this,
                                 [this, badge, global] {
                                     this->wear(badge, global);
                                 });

                const QPointer<QAction> guard(action);
                webbadges::picture(badge.image, action,
                                   [guard](const QPixmap &picture) {
                                       if (!guard.isNull())
                                       {
                                           guard->setIcon(QIcon(picture));
                                       }
                                   });
            }
        };

    addSection(QStringLiteral("Hier in #%1").arg(this->name_), choices.channel,
               choices.channelWorn, false);
    addSection(QStringLiteral("Überall"), choices.global, choices.globalWorn,
               true);

    // Opens upwards, as the input bar sits at the bottom
    menu->adjustSize();
    menu->popup(this->mapToGlobal(QPoint(0, -menu->sizeHint().height())));
}

void BadgeButton::wear(const webbadges::Badge &badge, bool global)
{
    const auto id = this->id_;
    webbadges::choose(
        id, badge, global, this, [this, id](const QString &problem) {
            if (!problem.isEmpty())
            {
                QToolTip::showText(
                    this->mapToGlobal(QPoint(0, 0)),
                    QStringLiteral("Badge nicht gewechselt: ") + problem, this);
                return;
            }
            // What is seen next to the name may have changed either way
            if (id == this->id_)
            {
                this->knownFor_.clear();
                this->refresh(false);
            }
        });
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
        this->setToolTip(QStringLiteral("Badge wählen - in %1 trägst du "
                                        "gerade „%2“.")
                             .arg(where, this->worn_->title));
    }
    else
    {
        this->setToolTip(
            QStringLiteral("Badge wählen, das du in %1 trägst").arg(where));
    }
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
