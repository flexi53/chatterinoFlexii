// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/VoteBannerWidget.hpp"

#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/Helpers.hpp"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <QVBoxLayout>

#include <cmath>

using namespace std::chrono_literals;
using namespace Qt::Literals;

namespace {

/// How long a finished vote stays up before it goes away by itself
/// ChattiFlexii: how long the result stands before the banner folds away
constexpr auto SHOW_RESULT_FOR = 30s;
/// How tall one line of the vote is, unscaled
constexpr int BAR_HEIGHT = 22;
/// Room left and right of the words inside a line
constexpr int BAR_PADDING = 6;

}  // namespace

namespace chatterino {

VoteBarWidget::VoteBarWidget(QWidget *parent)
    : BaseWidget(parent)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void VoteBarWidget::setBar(const QString &title, const QString &right,
                           double share, const QColor &color, bool leading)
{
    this->title_ = title;
    this->right_ = right;
    this->share_ = std::clamp(share, 0.0, 1.0);
    this->color_ = color;
    this->leading_ = leading;
    this->update();
}

QSize VoteBarWidget::sizeHint() const
{
    return {10, int(BAR_HEIGHT * this->scale())};
}

void VoteBarWidget::scaleChangedEvent(float newScale)
{
    QFont font = this->font();
    font.setPointSizeF(10.0F * newScale);
    this->setFont(font);
    this->setFixedHeight(int(BAR_HEIGHT * newScale));
    this->updateGeometry();
}

void VoteBarWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto rounded = this->height() / 4.0;
    const QRectF whole(0, 0, this->width(), this->height());

    // The track behind it, a touch lighter than the banner
    auto track = this->color_;
    track.setAlpha(this->leading_ ? 45 : 32);
    QPainterPath path;
    path.addRoundedRect(whole, rounded, rounded);
    painter.fillPath(path, track);

    // As far as this one got
    if (this->share_ > 0.0)
    {
        auto filled = this->color_;
        filled.setAlpha(this->leading_ ? 170 : 110);
        QPainterPath fill;
        fill.addRoundedRect(
            QRectF(0, 0, std::max(whole.width() * this->share_, rounded * 2),
                   whole.height()),
            rounded, rounded);
        painter.fillPath(fill.intersected(path), filled);
    }

    // What it says, and its number on the other side
    painter.setFont(this->font());
    auto pen = this->theme->messages.textColors.regular;
    painter.setPen(pen);

    const auto padding = int(BAR_PADDING * this->scale());
    const QFontMetrics metrics(this->font());
    const auto rightWidth = metrics.horizontalAdvance(this->right_);
    const QRect left(padding, 0,
                     std::max(0, this->width() - rightWidth - 3 * padding),
                     this->height());
    QFont bold = this->font();
    bold.setBold(this->leading_);
    painter.setFont(bold);
    painter.drawText(left, Qt::AlignVCenter | Qt::AlignLeft,
                     metrics.elidedText(this->title_, Qt::ElideRight,
                                        left.width()));
    painter.setFont(this->font());
    painter.drawText(QRect(0, 0, this->width() - padding, this->height()),
                     Qt::AlignVCenter | Qt::AlignRight, this->right_);
}

VoteBannerWidget::VoteBannerWidget(QWidget *parent)
    : SplitBanner(parent)
    , bars_(new QVBoxLayout())
    , footerLabel_(new QLabel(this))
{
    this->headerRow()->addWidget(this->countdownLabel());

    this->bars_->setContentsMargins(0, 2, 0, 0);
    this->bars_->setSpacing(3);
    this->contentBox()->addLayout(this->bars_);

    this->footerLabel_->setStyleSheet(MUTED_STYLE);
    this->contentBox()->addWidget(this->footerLabel_);

    this->hide();

    getSettings()->showVoteBanner.connect(
        [this] {
            this->refresh();
        },
        this->connections_, false);

    this->scaleChangedEvent(this->scale());
}

void VoteBannerWidget::setChannel(TwitchChannel *channel)
{
    this->signalHolder_.clear();
    this->channel_ = channel;
    this->userToggled_ = false;
    this->showing_.clear();
    this->setDismissed(false);
    this->stopAutoHide();

    if (channel != nullptr)
    {
        this->signalHolder_.managedConnect(channel->pollChanged, [this] {
            this->refresh();
        });
        this->signalHolder_.managedConnect(channel->predictionChanged, [this] {
            this->refresh();
        });
    }

    this->refresh();
}

void VoteBannerWidget::toggleUserPinned()
{
    if (this->isVisible())
    {
        // Put away by hand: it stays away while this one is running, every
        // update of it included
        this->userToggled_ = false;
        this->setDismissed(true);
        this->stopAutoHide();
        this->hide();
        return;
    }

    // Nothing to bring back where nothing is running, or where the banner
    // is switched off altogether
    if (this->channel_ == nullptr || !getSettings()->showVoteBanner ||
        (this->channel_->currentPoll() == nullptr &&
         this->channel_->currentPrediction() == nullptr))
    {
        return;
    }

    this->userToggled_ = true;
    this->setDismissed(false);
    this->stopAutoHide();
    this->refresh();
    this->show();
}

void VoteBannerWidget::refresh()
{
    // Aussehen -> Chat can do without the banner altogether
    if (this->channel_ == nullptr || !getSettings()->showVoteBanner)
    {
        this->stopCountdown();
        this->stopAutoHide();
        this->hide();
        return;
    }

    const auto *poll = this->channel_->currentPoll();
    const auto *prediction = this->channel_->currentPrediction();

    // A running vote comes first; of two running ones the poll, which is
    // the shorter of the two
    if (poll != nullptr && poll->running())
    {
        this->showPoll();
        return;
    }
    if (prediction != nullptr && prediction->running())
    {
        this->showPrediction();
        return;
    }
    // Neither runs - the one that just ended stays up for a while
    if (poll != nullptr && this->showing_ == poll->id)
    {
        this->showPoll();
        return;
    }
    if (prediction != nullptr && this->showing_ == prediction->id)
    {
        this->showPrediction();
        return;
    }
    if (poll != nullptr)
    {
        this->showPoll();
        return;
    }
    if (prediction != nullptr)
    {
        this->showPrediction();
        return;
    }

    this->stopCountdown();
    this->stopAutoHide();
    this->hide();
}

void VoteBannerWidget::useBars(size_t count)
{
    while (this->bar_.size() < count)
    {
        auto *bar = new VoteBarWidget(this);
        this->bars_->addWidget(bar);
        this->bar_.push_back(bar);
    }
    for (size_t i = 0; i < this->bar_.size(); i++)
    {
        this->bar_.at(i)->setVisible(i < count);
    }
}

void VoteBannerWidget::setFooter(const QString &text)
{
    this->footerLabel_->setText(text);
    this->footerLabel_->setVisible(!text.isEmpty());
}

void VoteBannerWidget::showPoll()
{
    const auto *poll = this->channel_->currentPoll();
    if (poll == nullptr)
    {
        return;
    }

    const bool isNew = this->showing_ != poll->id;
    this->showing_ = poll->id;
    if (isNew)
    {
        this->userToggled_ = false;
        this->setDismissed(false);
    }

    this->headerLabel()->setText(
        u"Umfrage · <b>%1</b>"_s.arg(poll->title.toHtmlEscaped()));

    const auto leader = poll->leader();
    this->useBars(poll->choices.size());
    for (size_t i = 0; i < poll->choices.size(); i++)
    {
        const auto &choice = poll->choices.at(i);
        const auto share = poll->totalVotes > 0
                               ? double(choice.votes) / poll->totalVotes
                               : 0.0;
        const auto percent = static_cast<int>(std::lround(share * 100));
        this->bar_.at(i)->setBar(
            choice.title,
            u"%1 % · %2"_s.arg(percent).arg(localizeNumbers(choice.votes)),
            share, this->theme->accent,
            leader.has_value() && *leader == i);
    }

    QString footer =
        poll->totalVotes == 1
            ? u"1 Stimme"_s
            : u"%1 Stimmen"_s.arg(localizeNumbers(poll->totalVotes));
    if (poll->pointsPerVote > 0)
    {
        footer += u" · je weitere Stimme %1 Punkte"_s.arg(
            localizeNumbers(poll->pointsPerVote));
    }
    switch (poll->status)
    {
        case PubSubPoll::Status::Completed:
            footer += u" · beendet"_s;
            break;
        case PubSubPoll::Status::Terminated:
            footer += u" · vorzeitig beendet"_s;
            break;
        case PubSubPoll::Status::Archived:
            footer += u" · verworfen"_s;
            break;
        default:
            break;
    }
    this->setFooter(footer);

    if (poll->running())
    {
        this->stopAutoHide();
        this->setTimeShare(this->timeShareOf(poll->id, poll->remaining()));
        this->startCountdown();
    }
    else
    {
        this->stopCountdown();
        this->countdownLabel()->hide();
        this->setTimeShare(-1);
        if (!this->userToggled_)
        {
            this->startAutoHide(SHOW_RESULT_FOR);
        }
    }

    this->showUnlessDismissed();
}

void VoteBannerWidget::showPrediction()
{
    const auto *prediction = this->channel_->currentPrediction();
    if (prediction == nullptr)
    {
        return;
    }

    const bool isNew = this->showing_ != prediction->id;
    this->showing_ = prediction->id;
    if (isNew)
    {
        this->userToggled_ = false;
        this->setDismissed(false);
    }

    this->headerLabel()->setText(
        u"Vorhersage · <b>%1</b>"_s.arg(prediction->title.toHtmlEscaped()));

    const auto total = prediction->totalPoints();
    this->useBars(prediction->outcomes.size());
    for (size_t i = 0; i < prediction->outcomes.size(); i++)
    {
        const auto &outcome = prediction->outcomes.at(i);
        const auto share =
            total > 0 ? double(outcome.points) / double(total) : 0.0;
        const auto payout = prediction->payoutOf(i);
        auto right = u"%1 Punkte"_s.arg(localizeNumbers(outcome.points));
        if (payout > 0.0)
        {
            right = u"%1× · %2"_s.arg(QString::number(payout, 'f', 1),
                                      localizeNumbers(outcome.points));
        }
        const bool won = !prediction->winningOutcomeId.isEmpty() &&
                         prediction->winningOutcomeId == outcome.id;
        this->bar_.at(i)->setBar(outcome.title, right, share,
                                 outcome.asColor(), won);
    }

    QString footer = u"%1 Punkte gesetzt"_s.arg(localizeNumbers(total));
    switch (prediction->status)
    {
        case PubSubPrediction::Status::Locked:
            footer += u" · keine Einsätze mehr"_s;
            break;
        case PubSubPrediction::Status::Resolved:
            footer += u" · entschieden"_s;
            break;
        case PubSubPrediction::Status::Canceled:
            footer += u" · abgebrochen, Punkte zurück"_s;
            break;
        default:
            break;
    }
    this->setFooter(footer);

    if (prediction->status == PubSubPrediction::Status::Active)
    {
        this->stopAutoHide();
        this->setTimeShare(
            this->timeShareOf(prediction->id, prediction->remaining()));
        this->startCountdown();
    }
    else
    {
        this->stopCountdown();
        this->countdownLabel()->hide();
        this->setTimeShare(-1);
        if (prediction->status == PubSubPrediction::Status::Locked)
        {
            // ChattiFlexii: no more bets to place and nothing decided yet -
            // it folds away by itself and comes back with the result,
            // unless it was opened by hand
            this->stopAutoHide();
            if (!this->userToggled_)
            {
                this->hide();
                return;
            }
        }
        else if (!this->userToggled_)
        {
            this->startAutoHide(SHOW_RESULT_FOR);
        }
    }

    this->showUnlessDismissed();
}

double VoteBannerWidget::timeShareOf(const QString &id,
                                    std::chrono::milliseconds left)
{
    if (this->timedId_ != id)
    {
        this->timedId_ = id;
        this->timedTotal_ = left;
    }
    // Where more time is left than ever before, this one turned up here
    // after it had already started
    this->timedTotal_ = std::max(this->timedTotal_, left);

    if (this->timedTotal_.count() <= 0 || left.count() <= 0)
    {
        return -1;
    }
    return double(left.count()) / double(this->timedTotal_.count());
}

void VoteBannerWidget::tickCountdown()
{
    if (this->channel_ == nullptr)
    {
        return;
    }

    std::chrono::milliseconds left{0};
    const auto *poll = this->channel_->currentPoll();
    const auto *prediction = this->channel_->currentPrediction();
    if (poll != nullptr && this->showing_ == poll->id)
    {
        left = poll->remaining();
    }
    else if (prediction != nullptr && this->showing_ == prediction->id)
    {
        left = prediction->remaining();
    }

    if (left.count() <= 0)
    {
        this->countdownLabel()->hide();
        this->setTimeShare(-1);
        this->stopCountdown();
        return;
    }

    this->countdownLabel()->setText(formatCountdown(left.count()));
    this->countdownLabel()->show();
    this->setTimeShare(this->timeShareOf(this->showing_, left));
}

void VoteBannerWidget::autoHide()
{
    this->hide();
}

void VoteBannerWidget::showEvent(QShowEvent *event)
{
    SplitBanner::showEvent(event);
    this->visibilityChanged.invoke();
}

void VoteBannerWidget::hideEvent(QHideEvent *event)
{
    SplitBanner::hideEvent(event);
    this->visibilityChanged.invoke();
}

void VoteBannerWidget::scaleChangedEvent(float newScale)
{
    SplitBanner::scaleChangedEvent(newScale);

    QFont footerFont = this->footerLabel_->font();
    footerFont.setPointSizeF(9.0F * newScale);
    this->footerLabel_->setFont(footerFont);
}

}  // namespace chatterino
