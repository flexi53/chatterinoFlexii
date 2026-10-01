// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/HypeTrainBannerWidget.hpp"

#include "providers/twitch/TwitchChannel.hpp"
#include "controllers/banners/BannerChannels.hpp"
#include "singletons/Settings.hpp"
#include "util/Helpers.hpp"
#include "widgets/splits/VoteBannerWidget.hpp"

#include <QLabel>
#include <QVBoxLayout>

#include <cmath>

using namespace std::chrono_literals;
using namespace Qt::Literals;

namespace {

/// How long a train that is over stays up
constexpr auto SHOW_RESULT_FOR = 20s;
/// The purple Twitch itself puts on the hype train
const QColor HYPE_COLOR{0x91, 0x46, 0xff};

}  // namespace

namespace chatterino {

HypeTrainBannerWidget::HypeTrainBannerWidget(QWidget *parent)
    : SplitBanner(parent)
    , bar_(new VoteBarWidget(this))
    , footerLabel_(new QLabel(this))
{
    this->headerRow()->addWidget(this->countdownLabel());
    this->contentBox()->addWidget(this->bar_);

    this->footerLabel_->setStyleSheet(MUTED_STYLE);
    this->contentBox()->addWidget(this->footerLabel_);

    this->hide();

    // ChattiFlexii: and a channel it is left out of - set in the split's
    // own menu
    getSettings()->hypeTrainOffChannels.connect(
        [this](const auto &, auto) {
            this->refresh();
        },
        this->connections_, false);
    getSettings()->showHypeTrainBanner.connect(
        [this] {
            this->refresh();
        },
        this->connections_, false);

    this->scaleChangedEvent(this->scale());
}

void HypeTrainBannerWidget::setChannel(TwitchChannel *channel)
{
    this->signalHolder_.clear();
    this->channel_ = channel;
    this->userToggled_ = false;
    this->showing_.clear();
    this->setDismissed(false);
    this->stopAutoHide();

    if (channel != nullptr)
    {
        this->signalHolder_.managedConnect(channel->hypeTrainChanged, [this] {
            this->refresh();
        });
    }

    this->refresh();
}

void HypeTrainBannerWidget::toggleUserPinned()
{
    if (this->isVisible())
    {
        // Put away by hand: it stays away until this train is over, however
        // often Twitch sends an update for it
        this->userToggled_ = false;
        this->setDismissed(true);
        this->stopAutoHide();
        this->hide();
        return;
    }

    if (this->channel_ == nullptr || !getSettings()->showHypeTrainBanner ||
        !banners::hypeShownIn(this->channel_->getName()) ||
        this->channel_->currentHypeTrain() == nullptr)
    {
        return;
    }

    this->userToggled_ = true;
    this->setDismissed(false);
    this->stopAutoHide();
    this->refresh();
    this->show();
}

void HypeTrainBannerWidget::refresh()
{
    // Aussehen -> Splits can do without the banner altogether, and the
    // split's menu can leave it out of this one channel
    if (this->channel_ == nullptr || !getSettings()->showHypeTrainBanner ||
        !banners::hypeShownIn(this->channel_->getName()))
    {
        this->stopCountdown();
        this->stopAutoHide();
        this->hide();
        return;
    }

    const auto *train = this->channel_->currentHypeTrain();
    if (train == nullptr)
    {
        this->stopCountdown();
        this->stopAutoHide();
        this->hide();
        return;
    }

    if (this->showing_ != train->id)
    {
        // A train of its own: whatever was put away belonged to the one
        // before it
        this->showing_ = train->id;
        this->userToggled_ = false;
        this->setDismissed(false);
    }

    this->headerLabel()->setText(
        u"Hype Train · <b>Level %1</b>"_s.arg(train->level));

    const auto percent = static_cast<int>(std::lround(train->share() * 100));
    this->bar_->setBar(u"Level %1"_s.arg(train->level),
                       u"%1 %"_s.arg(percent), train->share(), HYPE_COLOR,
                       !train->over);

    QString footer;
    if (train->goal > 0)
    {
        footer = u"%1 von %2 bis Level %3"_s.arg(localizeNumbers(train->value),
                                                 localizeNumbers(train->goal))
                     .arg(train->level + 1);
    }
    if (train->over)
    {
        footer += footer.isEmpty() ? QString() : u" · "_s;
        footer += train->completed ? u"durchgefahren"_s : u"ausgelaufen"_s;
    }
    this->footerLabel_->setText(footer);
    this->footerLabel_->setVisible(!footer.isEmpty());

    if (train->over)
    {
        this->stopCountdown();
        this->countdownLabel()->hide();
        if (!this->userToggled_)
        {
            this->startAutoHide(SHOW_RESULT_FOR);
        }
    }
    else
    {
        this->stopAutoHide();
        this->startCountdown();
    }

    this->showUnlessDismissed();
}

void HypeTrainBannerWidget::tickCountdown()
{
    if (this->channel_ == nullptr)
    {
        return;
    }
    const auto *train = this->channel_->currentHypeTrain();
    if (train == nullptr || train->remaining().count() <= 0)
    {
        this->countdownLabel()->hide();
        this->stopCountdown();
        return;
    }

    this->countdownLabel()->setText(
        formatCountdown(train->remaining().count()));
    this->countdownLabel()->show();
}

void HypeTrainBannerWidget::showEvent(QShowEvent *event)
{
    SplitBanner::showEvent(event);
    this->visibilityChanged.invoke();
}

void HypeTrainBannerWidget::hideEvent(QHideEvent *event)
{
    SplitBanner::hideEvent(event);
    this->visibilityChanged.invoke();
}

void HypeTrainBannerWidget::scaleChangedEvent(float newScale)
{
    SplitBanner::scaleChangedEvent(newScale);

    QFont footerFont = this->footerLabel_->font();
    footerFont.setPointSizeF(9.0F * newScale);
    this->footerLabel_->setFont(footerFont);
}

}  // namespace chatterino
