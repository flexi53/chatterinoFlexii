// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/RaidBannerWidget.hpp"

#include "providers/twitch/api/ModGql.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "util/Helpers.hpp"

#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

using namespace std::chrono_literals;
using namespace Qt::Literals;

namespace {

/// How long a raid that went or was called off stays up
constexpr auto SHOW_RESULT_FOR = 15s;

}  // namespace

namespace chatterino {

RaidBannerWidget::RaidBannerWidget(QWidget *parent)
    : SplitBanner(parent)
    , footerLabel_(new QLabel(this))
{
    this->headerRow()->addWidget(this->countdownLabel());

    this->goButton_ = this->addHeaderButton(
        u"Jetzt"_s, u"Schickt den Raid sofort los, statt bis zum Ende der "
                    "Wartezeit zu warten."_s);
    this->cancelButton_ = this->addHeaderButton(
        u"Abbrechen"_s, u"Bricht den Raid ab - niemand geht mit."_s);

    QObject::connect(this->goButton_, &QPushButton::clicked, this, [this] {
        if (this->channel_ == nullptr)
        {
            return;
        }
        QPointer<RaidBannerWidget> self(this);
        modgql::goRaid(
            this->channel_->roomId(),
            [self] {
                if (self)
                {
                    self->noteResult(u"Losgeschickt"_s, false);
                }
            },
            [self](const QString &error) {
                if (self)
                {
                    self->noteResult(error, true);
                }
            });
    });
    QObject::connect(this->cancelButton_, &QPushButton::clicked, this, [this] {
        if (this->channel_ == nullptr)
        {
            return;
        }
        QPointer<RaidBannerWidget> self(this);
        modgql::cancelRaid(
            this->channel_->roomId(),
            [self] {
                if (self)
                {
                    self->noteResult(u"Abgebrochen"_s, false);
                }
            },
            [self](const QString &error) {
                if (self)
                {
                    self->noteResult(error, true);
                }
            });
    });

    this->footerLabel_->setStyleSheet(MUTED_STYLE);
    this->footerLabel_->setTextFormat(Qt::RichText);
    this->contentBox()->addWidget(this->footerLabel_);

    this->hide();

    getSettings()->showRaidBanner.connect(
        [this] {
            this->refresh();
        },
        this->connections_, false);

    // The buttons go through the Twitch app login
    QPointer<RaidBannerWidget> self(this);
    modgql::whenReady(this, [self](bool loggedIn) {
        if (self)
        {
            self->loggedIn_ = loggedIn;
            self->refresh();
        }
    });
    this->connections_.managedConnect(modgql::loginChanged(), [this] {
        this->loggedIn_ = modgql::currentLogin().has_value();
        this->refresh();
    });

    this->scaleChangedEvent(this->scale());
}

void RaidBannerWidget::setChannel(TwitchChannel *channel)
{
    this->signalHolder_.clear();
    this->channel_ = channel;
    this->showing_.clear();
    this->note_.clear();
    this->setDismissed(false);
    this->stopAutoHide();

    if (channel != nullptr)
    {
        this->signalHolder_.managedConnect(channel->raidChanged, [this] {
            this->refresh();
        });
    }

    this->refresh();
}

void RaidBannerWidget::noteResult(const QString &text, bool error)
{
    this->note_ = text;
    this->noteIsError_ = error;
    this->refresh();
}

void RaidBannerWidget::refresh()
{
    // Aussehen -> Splits can do without the banner altogether
    const auto *raid =
        this->channel_ != nullptr ? this->channel_->currentRaid() : nullptr;
    if (raid == nullptr || !getSettings()->showRaidBanner)
    {
        this->stopCountdown();
        this->stopAutoHide();
        this->hide();
        return;
    }

    if (this->showing_ != raid->id)
    {
        this->showing_ = raid->id;
        this->note_.clear();
        this->setDismissed(false);
    }

    const bool waiting = raid->kind == PubSubRaid::Kind::Update;
    this->headerLabel()->setText(
        u"Raid zu <b>%1</b>"_s.arg(raid->targetDisplayName.toHtmlEscaped()));

    QString footer =
        raid->viewerCount == 1
            ? u"1 Zuschauer geht mit"_s
            : u"%1 Zuschauer gehen mit"_s.arg(
                  localizeNumbers(raid->viewerCount));
    if (raid->kind == PubSubRaid::Kind::Went)
    {
        footer = u"Losgegangen · %1 Zuschauer"_s.arg(
            localizeNumbers(raid->viewerCount));
    }
    else if (raid->kind == PubSubRaid::Kind::Canceled)
    {
        footer = u"Abgebrochen"_s;
    }
    footer = footer.toHtmlEscaped();
    if (!this->note_.isEmpty())
    {
        footer += u" · <span style=\"color:%1\">%2</span>"_s.arg(
            this->noteIsError_ ? u"#ff6b6b"_s : u"#3ecf8e"_s,
            this->note_.toHtmlEscaped());
    }
    this->footerLabel_->setText(footer);

    // Only while it waits is there anything to send or call off, and only
    // for whoever runs the channel
    const bool canAct =
        waiting && this->loggedIn_ &&
        (this->channel_->isMod() || this->channel_->isBroadcaster());
    this->goButton_->setVisible(canAct);
    this->cancelButton_->setVisible(canAct);

    if (waiting && raid->remaining().count() > 0)
    {
        this->stopAutoHide();
        this->startCountdown();
    }
    else
    {
        this->stopCountdown();
        this->countdownLabel()->hide();
        this->setTimeShare(-1);
        this->startAutoHide(SHOW_RESULT_FOR);
    }

    this->showUnlessDismissed();
}

void RaidBannerWidget::tickCountdown()
{
    const auto *raid =
        this->channel_ != nullptr ? this->channel_->currentRaid() : nullptr;
    if (raid == nullptr || raid->remaining().count() <= 0)
    {
        this->countdownLabel()->hide();
        this->setTimeShare(-1);
        this->stopCountdown();
        return;
    }

    const auto left = raid->remaining();
    this->countdownLabel()->setText(formatCountdown(left.count()));
    this->countdownLabel()->show();
    // Twitch says how long it waits, so the bar knows its whole length
    const auto total = std::chrono::milliseconds{raid->waitSeconds * 1000LL};
    this->setTimeShare(total.count() > 0
                           ? double(left.count()) / double(total.count())
                           : -1);
}

void RaidBannerWidget::autoHide()
{
    this->setDismissed(true);
    this->hide();
}

void RaidBannerWidget::showEvent(QShowEvent *event)
{
    SplitBanner::showEvent(event);
    this->visibilityChanged.invoke();
}

void RaidBannerWidget::hideEvent(QHideEvent *event)
{
    SplitBanner::hideEvent(event);
    this->visibilityChanged.invoke();
}

void RaidBannerWidget::scaleChangedEvent(float newScale)
{
    SplitBanner::scaleChangedEvent(newScale);

    QFont footerFont = this->footerLabel_->font();
    footerFont.setPointSizeF(9.0F * newScale);
    this->footerLabel_->setFont(footerFont);
}

}  // namespace chatterino
