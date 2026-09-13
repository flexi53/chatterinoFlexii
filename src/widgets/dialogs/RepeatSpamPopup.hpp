// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BasePopup.hpp"

#include <pajlada/signals/scoped-connection.hpp>
#include <QElapsedTimer>
#include <QPoint>
#include <QString>
#include <QTimer>

#include <memory>
#include <optional>

class QLabel;
class QPushButton;

namespace chatterino {

class Channel;
class ChannelView;
class Label;
class PixmapButton;

/// The repeated message alert, laid out like a user card: who the chatter is
/// and how old their account is, their recent messages as they appeared in
/// chat - Twitch's timeout notices included - and a bar running down to the
/// window closing by itself, above a button that hands out the timeout that
/// fits.
class RepeatSpamPopup : public BasePopup
{
    Q_OBJECT

public:
    RepeatSpamPopup(QString channel, QString login, QWidget *parent);

    /// Shows the chatter's recent messages from the channel and offers a
    /// timeout of @a seconds. @a timeoutsServed counts the timeouts they have
    /// already sat out for this message. Starts the countdown over.
    void setCase(const QString &displayName, int seconds, int timeoutsServed);

    /// Shows the window with made up messages - the first alert, or the one
    /// after a timeout - and buttons that send nothing
    void showTestCase(bool afterTimeout);

private:
    void setHeadline(int timeoutsServed);
    void setTimeoutSeconds(int seconds);
    void loadProfile();
    void restartCountdown();
    void tick();

    QString channel_;
    QString login_;
    int seconds_ = 30;
    bool test_ = false;
    bool profileLoaded_ = false;

    PixmapButton *avatar_{};
    Label *name_{};
    QLabel *details_{};
    QLabel *headline_{};
    ChannelView *messages_{};
    QLabel *testNote_{};
    QWidget *countdownBar_{};
    QPushButton *ignore_{};
    QPushButton *timeout_{};

    std::shared_ptr<Channel> view_;
    std::optional<pajlada::Signals::ScopedConnection> liveMessages_;

    QTimer countdown_;
    QElapsedTimer sinceTick_;
    qint64 totalMs_ = 0;
    qint64 remainingMs_ = 0;
    /// Where the pointer was when the countdown started. It only holds the
    /// window open once it has moved - a window that opens under a resting
    /// pointer would otherwise never close.
    QPoint pointerAtStart_;
    bool pointerMoved_ = false;

    /// Lets network replies tell whether the window is still around
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

}  // namespace chatterino
