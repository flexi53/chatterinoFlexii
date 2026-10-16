// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/api/ModGql.hpp"
#include "widgets/BasePopup.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QPoint>
#include <QSize>
#include <QString>

#include <memory>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QVBoxLayout;
class QWidget;

namespace chatterino {

class Label;
class TwitchChannel;

/// ChattiFlexii: the window behind the points beside the input, laid out
/// like the alert windows - the channel at the top with your balance, its
/// rewards as tiles in their own colours, and for moderators the forms to
/// start a poll or a prediction. What is running is shown and handled in
/// the vote banner; this only starts things. It goes through the Twitch app
/// login under Twitch-Extras.
class TwitchActionsPopup : public BasePopup
{
    Q_OBJECT

public:
    TwitchActionsPopup(const std::shared_ptr<TwitchChannel> &channel,
                       QWidget *parent);

    const QString &channelLogin() const
    {
        return this->channelLogin_;
    }

    /// Puts the window just above @a anchor, its right edge with the
    /// anchor's, the way the emote window comes up by its button
    void placeAbove(const QWidget *anchor);

protected:
    void themeChangedEvent() override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void buildHead(QVBoxLayout *layout);
    QWidget *buildLoginPage();
    QWidget *buildRewardsPage();
    QWidget *buildPollPage();
    QWidget *buildPredictionPage();
    /// A box in the look of the alert windows' reason box, with @a title on
    /// top; the content goes into the layout handed back
    QVBoxLayout *addCard(QVBoxLayout *layout, const QString &title,
                         QWidget **card);
    /// A row of choices like "1 Minute" - one of them always picked
    QButtonGroup *addChips(QVBoxLayout *layout,
                           std::initializer_list<std::pair<const char *, int>>
                               choices,
                           int picked);

    /// Shows whether there is a login, and what may be started now
    void showState();
    void refreshPoints();
    void showPoints(const modgql::ChannelPoints &points);
    void redeem(const modgql::Reward &reward);
    /// The channel's sub emotes to pick one from, for Twitch's own reward
    void pickEmote(const modgql::Reward &reward);
    void sendRedeem(const modgql::Reward &reward, const QString &text,
                    const QString &emoteId);
    /// Colours that come from the theme, again after it changed
    void applyStyles();

    void createPoll();
    void createPrediction();

    /// Says how something went in the line at the bottom
    void say(const QString &text, bool error = false);

    std::weak_ptr<TwitchChannel> channel_;
    QString channelLogin_;
    QString channelId_;
    bool moderates_ = false;
    bool loggedIn_ = false;
    qint64 balance_ = 0;

    QLabel *avatar_{};
    Label *name_{};
    QLabel *details_{};
    QLabel *balanceLabel_{};
    QLabel *balanceCaption_{};

    QWidget *sections_{};
    QButtonGroup *sectionGroup_{};
    QStackedWidget *pages_{};
    QWidget *loginPage_{};
    QWidget *rewardsPage_{};
    QLabel *status_{};

    QGridLayout *rewardGrid_{};
    QLabel *rewardsEmpty_{};

    std::vector<QWidget *> cards_;
    std::vector<QPushButton *> quietButtons_;
    std::vector<QPushButton *> accentButtons_;

    QLabel *pollRunning_{};
    QWidget *pollCard_{};
    QLineEdit *pollTitle_{};
    std::vector<QLineEdit *> pollChoices_;
    QButtonGroup *pollDuration_{};
    QCheckBox *pollPointsOn_{};
    QSpinBox *pollPoints_{};

    QLabel *predictionRunning_{};
    QWidget *predictionCard_{};
    QLineEdit *predictionTitle_{};
    QVBoxLayout *predictionOutcomeBox_{};
    std::vector<QLineEdit *> predictionOutcomes_;
    QPushButton *addOutcome_{};
    QButtonGroup *predictionWindow_{};

    /// The size and place it came up with
    QSize shownSize_;
    QPoint shownPos_;

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
