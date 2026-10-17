// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderationactions/ModerationAction.hpp"

#include "Application.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "messages/Image.hpp"
#include "singletons/Resources.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QUrl>

namespace chatterino {

namespace {

/// ChattiFlexii: a pushpin, drawn in the colour and weight of the bin and
/// the ban sign next to it
QPixmap drawPin()
{
    constexpr int side = 64;
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(0x8a, 0x84, 0x94));
    pen.setWidthF(5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // Leaning to the right, as a pin stuck in a board
    painter.translate(side / 2.0, side / 2.0);
    painter.rotate(35);
    painter.translate(-side / 2.0, -side / 2.0);

    // The head, the body narrowing below it, the plate and the needle
    painter.drawLine(QPointF(22, 8), QPointF(42, 8));
    QPainterPath body;
    body.moveTo(26, 8);
    body.lineTo(26, 24);
    body.lineTo(18, 36);
    body.lineTo(46, 36);
    body.lineTo(38, 24);
    body.lineTo(38, 8);
    painter.drawPath(body);
    painter.drawLine(QPointF(32, 36), QPointF(32, 58));

    return pixmap;
}

}  // namespace

ModerationAction::ModerationAction(const QString &action, const QUrl &iconPath)
    : action_(action)
{
    static QRegularExpression replaceRegex("[!/.]");
    static QRegularExpression timeoutRegex("^[./]timeout.* (\\d+)([mhdw]?)");

    auto timeoutMatch = timeoutRegex.match(action);

    if (timeoutMatch.hasMatch())
    {
        this->type_ = Type::Timeout;

        // if (multipleTimeouts > 1) {
        // QString line1;
        // QString line2;

        constexpr int minute = 60;
        constexpr int hour = 60 * minute;
        constexpr int day = 24 * hour;
        constexpr int week = 7 * day;

        int amount = timeoutMatch.captured(1).toInt();
        QString unit = timeoutMatch.captured(2);

        if (unit == "m")
        {
            amount *= minute;
        }
        else if (unit == "h")
        {
            amount *= hour;
        }
        else if (unit == "d")
        {
            amount *= day;
        }
        else if (unit == "w")
        {
            amount *= week;
        }

        if (amount < minute)
        {
            this->line1_ = QString::number(amount);
            this->line2_ = "s";
        }
        else if (amount < hour)
        {
            this->line1_ = QString::number(amount / minute);
            this->line2_ = "m";
        }
        else if (amount < day)
        {
            this->line1_ = QString::number(amount / hour);
            this->line2_ = "h";
        }
        else if (amount < week)
        {
            this->line1_ = QString::number(amount / day);
            this->line2_ = "d";
        }
        else
        {
            // limit to max timeout duration
            if (amount > 2 * week)
            {
                this->line1_ = ">2";
            }
            else
            {
                this->line1_ = QString::number(amount / week);
            }
            this->line2_ = "w";
        }
    }
    else if (action.startsWith("/ban "))
    {
        this->type_ = Type::Ban;
    }
    else if (action.startsWith("/delete "))
    {
        this->type_ = Type::Delete;
    }
    else if (action.startsWith("/pin "))
    {
        this->type_ = Type::Pin;
    }
    else
    {
        this->type_ = Type::Custom;

        QString xD = action;

        xD.replace(replaceRegex, "");

        this->line1_ = xD.mid(0, 2);
        this->line2_ = xD.mid(2, 2);
    }

    if (iconPath.isValid())
    {
        this->iconPath_ = iconPath;
    }
}

bool ModerationAction::operator==(const ModerationAction &other) const
{
    return this == std::addressof(other);
}

bool ModerationAction::isImage() const
{
    return bool(this->image_);
}

const std::optional<ImagePtr> &ModerationAction::getImage() const
{
    assertInGuiThread();
    if (this->image_.has_value())
    {
        return this->image_;
    }

    if (this->iconPath_.isValid())
    {
        this->image_ = Image::fromUrl({this->iconPath_.toString()});
    }
    else if (this->type_ == Type::Ban)
    {
        this->image_ = Image::fromResourcePixmap(getResources().buttons.ban);
    }
    else if (this->type_ == Type::Delete)
    {
        this->image_ =
            Image::fromResourcePixmap(getResources().buttons.trashCan);
    }
    else if (this->type_ == Type::Pin)
    {
        this->image_ = Image::fromResourcePixmap(drawPin());
    }

    return this->image_;
}

const QString &ModerationAction::getLine1() const
{
    return this->line1_;
}

const QString &ModerationAction::getLine2() const
{
    return this->line2_;
}

const QString &ModerationAction::getAction() const
{
    return this->action_;
}

const QUrl &ModerationAction::iconPath() const
{
    return this->iconPath_;
}

ModerationAction::Type ModerationAction::getType() const
{
    return this->type_;
}

}  // namespace chatterino
