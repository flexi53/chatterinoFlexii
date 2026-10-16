// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/api/ModGql.hpp"
#include "widgets/BasePopup.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QString>

#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;
class QVBoxLayout;
class QWidget;

namespace chatterino {

class TwitchChannel;

/// ChattiFlexii: the window behind the three bars in the input bar - the
/// channel's points, with the rewards to redeem, and for moderators the
/// forms to start a poll or a prediction. What is running is shown and
/// handled in the vote banner; this only starts things. It goes through the
/// Twitch app login under Twitch-Extras.
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

private:
    QWidget *buildPollTab();
    QWidget *buildPredictionTab();
    QWidget *buildPointsTab();

    /// Shows whether there is a login, and what may be started now
    void showState();
    void refreshPoints();
    void showPoints(const modgql::ChannelPoints &points);

    void createPoll();
    void createPrediction();

    /// Says how something went in the line under the tabs
    void say(const QString &text, bool error = false);

    std::weak_ptr<TwitchChannel> channel_;
    QString channelLogin_;
    QString channelId_;
    bool moderates_ = false;
    bool loggedIn_ = false;

    QWidget *loginNote_{};
    QLabel *status_{};
    QTabWidget *tabs_{};

    QLabel *pollRunning_{};
    QWidget *pollForm_{};
    QLineEdit *pollTitle_{};
    QPlainTextEdit *pollChoices_{};
    QComboBox *pollDuration_{};
    QCheckBox *pollPointsOn_{};
    QSpinBox *pollPoints_{};

    QLabel *predictionRunning_{};
    QWidget *predictionForm_{};
    QLineEdit *predictionTitle_{};
    QPlainTextEdit *predictionOutcomes_{};
    QComboBox *predictionWindow_{};

    QLabel *pointsBalance_{};
    QVBoxLayout *pointsList_{};

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
