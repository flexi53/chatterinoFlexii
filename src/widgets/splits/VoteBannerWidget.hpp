// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"
#include "widgets/splits/SplitBanner.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QColor>
#include <QString>

#include <chrono>
#include <functional>
#include <vector>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace chatterino {

class TwitchChannel;

/// One line of a vote: what it says on the left, its number on the right,
/// and how far it is ahead drawn behind both. ChattiFlexii: with a click of
/// its own where the banner offers one - voting or betting on it.
class VoteBarWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit VoteBarWidget(QWidget *parent = nullptr);

    /// @a share is between 0 and 1, @a leading makes it stand out
    void setBar(const QString &title, const QString &right, double share,
                const QColor &color, bool leading);

    QSize sizeHint() const override;

    /// What a click does; nothing takes the click away
    void setOnClick(std::function<void()> onClick);

protected:
    void paintEvent(QPaintEvent *event) override;
    void scaleChangedEvent(float newScale) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    std::function<void()> onClick_;
    bool hovered_ = false;
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
///
/// With the Twitch app login under Twitch-Extras a click on a line votes or
/// bets, and moderators get buttons to end, lock, call off and resolve.
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

    /// Whether a click may do something: switched on and signed in
    bool canAct() const;
    bool moderates() const;
    /// What a click on line @a index of the poll or prediction does
    void clickedPoll(size_t index);
    void clickedPrediction(size_t index, QWidget *bar);
    /// The buttons a moderator gets for what is shown
    void showModButtons();
    /// Says how an action went under the bars, until the next one
    void noteResult(const QString &text, bool error);
    /// Hands back what an action reports with
    std::pair<std::function<void()>, std::function<void(const QString &)>>
        reporter(const QString &done);

    TwitchChannel *channel_ = nullptr;
    QVBoxLayout *bars_ = nullptr;
    std::vector<VoteBarWidget *> bar_;
    QLabel *footerLabel_ = nullptr;
    QPushButton *endButton_ = nullptr;
    QPushButton *lockButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    /// Whether the Twitch app login is there
    bool loggedIn_ = false;
    /// How the last action went, for the vote it was about
    QString note_;
    bool noteIsError_ = false;
    QString noteFor_;
    /// The user brought it back by hand - then it stays
    bool userToggled_ = false;
    /// What is being shown, so a finished vote is not shown twice
    QString showing_;
    /// ChattiFlexii: Twitch says when a vote ends, never how long it ran -
    /// so the bar measures against the longest time ever seen left on this
    /// one, which is how long it had when it first turned up here
    QString timedId_;
    std::chrono::milliseconds timedTotal_{0};
    /// What is left of the time to vote, from 1 to 0 - below zero where
    /// there is nothing to wait for
    double timeShareOf(const QString &id, std::chrono::milliseconds left);

    pajlada::Signals::SignalHolder signalHolder_;
    /// What lives as long as the banner does
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
