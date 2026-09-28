// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"
#include "widgets/splits/SplitBanner.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QColor>
#include <QString>

#include <vector>

class QLabel;
class QVBoxLayout;

namespace chatterino {

class TwitchChannel;

/// One line of a vote: what it says on the left, its number on the right,
/// and how far it is ahead drawn behind both.
class VoteBarWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit VoteBarWidget(QWidget *parent = nullptr);

    /// @a share is between 0 and 1, @a leading makes it stand out
    void setBar(const QString &title, const QString &right, double share,
                const QColor &color, bool leading);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void scaleChangedEvent(float newScale) override;

private:
    QString title_;
    QString right_;
    double share_ = 0.0;
    QColor color_;
    bool leading_ = false;
};

/// ChattiFlexii: what the channel is voting on, under the split header - a
/// poll or a prediction, whichever is running. Twitch tells every viewer
/// about both, so this shows in every channel, not only where you are a
/// moderator.
class VoteBannerWidget : public SplitBanner
{
    Q_OBJECT

public:
    explicit VoteBannerWidget(QWidget *parent = nullptr);

    void setChannel(TwitchChannel *channel);

    /// Shows it again after it hid itself, or hides it - the button in the
    /// split header
    void toggleUserPinned();

    /// Emitted whenever this widget becomes shown or hidden
    pajlada::Signals::NoArgSignal visibilityChanged;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void scaleChangedEvent(float newScale) override;

    void tickCountdown() override;
    void autoHide() override;

private:
    /// Draws whatever the channel is voting on now, or hides
    void refresh();
    void showPoll();
    void showPrediction();
    /// Keeps as many bars as @a count, made once and used again after
    void useBars(size_t count);
    /// What stands under the bars: votes, points, or how it ended
    void setFooter(const QString &text);

    TwitchChannel *channel_ = nullptr;
    QVBoxLayout *bars_ = nullptr;
    std::vector<VoteBarWidget *> bar_;
    QLabel *footerLabel_ = nullptr;
    /// The user brought it back by hand - then it stays
    bool userToggled_ = false;
    /// What is being shown, so a finished vote is not shown twice
    QString showing_;

    pajlada::Signals::SignalHolder signalHolder_;
    /// What lives as long as the banner does
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
