// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/ModChannelsRow.hpp"

#include "controllers/moderation/ModHighlights.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "singletons/Settings.hpp"
#include "util/RoundPixmap.hpp"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QUrl>

#include <algorithm>

namespace chatterino {

namespace {

/// How many pictures fit on the line before the rest goes under "+N"
constexpr int SHOWN_PICTURES = 8;
constexpr int PICTURE_SIDE = 22;

void openChannel(const QString &channel)
{
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://www.twitch.tv/%1").arg(channel)));
}

/// The initial on a grey disc, until the picture is in
QPixmap placeholder(const QString &channel, qreal ratio)
{
    const int side = qRound(PICTURE_SIDE * ratio);
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(90, 90, 90));
    painter.drawEllipse(pixmap.rect());

    auto font = painter.font();
    font.setPixelSize(side / 2);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, channel.left(1).toUpper());

    pixmap.setDevicePixelRatio(ratio);
    return pixmap;
}

/// One channel's picture, opening the channel when clicked
class ChannelPicture : public QLabel
{
public:
    ChannelPicture(const QString &channel, QWidget *parent)
        : QLabel(parent)
        , channel_(channel)
    {
        const auto ratio = this->devicePixelRatioF();
        this->setFixedSize(PICTURE_SIDE, PICTURE_SIDE);
        this->setCursor(Qt::PointingHandCursor);
        this->setToolTip(profilepictures::displayName(channel));
        this->setPixmap(placeholder(channel, ratio));

        const auto side = qRound(PICTURE_SIDE * ratio);
        QPointer<ChannelPicture> self(this);
        profilepictures::whenKnown(channel, this, [self](const TwitchProfile &profile) {
            if (self)
            {
                self->setToolTip(profile.displayName);
            }
        });
        profilepictures::pixmap(channel, side, this,
                                [self, side, ratio](const QPixmap &picture) {
                                    if (!self)
                                    {
                                        return;
                                    }
                                    auto round = roundPixmap(picture, side);
                                    round.setDevicePixelRatio(ratio);
                                    self->setPixmap(round);
                                });
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            openChannel(this->channel_);
        }
    }

private:
    QString channel_;
};

}  // namespace

ModChannelsRow::ModChannelsRow(QWidget *parent)
    : QWidget(parent)
{
    this->layout_ = new QHBoxLayout(this);
    this->layout_->setContentsMargins(8, 2, 0, 2);
    this->layout_->setSpacing(4);
    this->hide();
}

void ModChannelsRow::showFor(const QString &login)
{
    this->login_ = login.toLower();
    this->hide();

    if (!ModHighlights::pluginAvailable() ||
        !getSettings()->modHighlightsUserCard.getValue())
    {
        return;
    }

    const auto asked = this->login_;
    QPointer<ModChannelsRow> self(this);
    ModHighlights::instance().lookUpModChannels(
        asked, this, [self, asked](const ModChannelsOfUser &result) {
            // A later showFor for someone else wins
            if (!self || self->login_ != asked)
            {
                return;
            }
            self->fill(result.channels, result.total, result.former,
                       result.formerTotal);
        });
}

void ModChannelsRow::fill(const QStringList &channels, int total,
                          const QStringList &former, int formerTotal)
{
    while (auto *item = this->layout_->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
    if (channels.isEmpty() && former.isEmpty())
    {
        this->hide();
        return;
    }

    // The channels chosen under Mod-Highlights are the ones that matter most
    const auto chosen = getSettings()->modHighlightChannels.getValue();
    auto ordered = channels;
    std::stable_partition(ordered.begin(), ordered.end(),
                          [&chosen](const QString &channel) {
                              return std::find(chosen.begin(), chosen.end(),
                                               channel) != chosen.end();
                          });

    auto *caption = new QLabel(channels.isEmpty() ? QStringLiteral("Früher Mod")
                                                  : QStringLiteral("Mod in"));
    caption->setStyleSheet(QStringLiteral("color: #aaa;"));
    this->layout_->addWidget(caption);

    const auto &pictured = channels.isEmpty() ? former : ordered;
    for (qsizetype i = 0; i < std::min<qsizetype>(pictured.size(), SHOWN_PICTURES);
         i++)
    {
        this->layout_->addWidget(new ChannelPicture(pictured[i], this));
    }
    const auto shownTotal = channels.isEmpty() ? formerTotal : total;
    if (shownTotal > SHOWN_PICTURES)
    {
        auto *more = new QLabel(QStringLiteral("+%1").arg(shownTotal - SHOWN_PICTURES));
        more->setStyleSheet(QStringLiteral("color: #aaa;"));
        this->layout_->addWidget(more);
    }

    // The whole list, as /wtm gives it
    auto *all = new QPushButton(QStringLiteral("Alle"));
    all->setFlat(true);
    all->setCursor(Qt::PointingHandCursor);
    all->setToolTip(QStringLiteral("Alle Kanäle, in denen %1 laut whosthemod.xyz "
                                   "Mod ist oder war")
                        .arg(this->login_));
    QObject::connect(all, &QPushButton::clicked, this,
                     [this, all, ordered, total, former, formerTotal] {
                         QMenu menu(this);
                         if (!ordered.isEmpty())
                         {
                             menu.addSection(
                                 QStringLiteral("Mod in %1 Kanälen").arg(total));
                             for (const auto &channel : ordered)
                             {
                                 menu.addAction(
                                     profilepictures::displayName(channel),
                                     [channel] {
                                         openChannel(channel);
                                     });
                             }
                         }
                         if (!former.isEmpty())
                         {
                             menu.addSection(
                                 QStringLiteral("Früher Mod in %1 Kanälen")
                                     .arg(formerTotal));
                             for (const auto &channel : former)
                             {
                                 menu.addAction(
                                     profilepictures::displayName(channel),
                                     [channel] {
                                         openChannel(channel);
                                     });
                             }
                         }
                         menu.exec(all->mapToGlobal(QPoint(0, all->height())));
                     });
    this->layout_->addWidget(all);
    this->layout_->addStretch(1);

    this->show();
}

}  // namespace chatterino
