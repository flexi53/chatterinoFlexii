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
struct ModSuggestion;
class ChannelView;
class Label;
class PixmapButton;

/// An alert about one chatter, laid out like a user card: who they are and
/// how old their account is, their recent lines as they appeared in chat -
/// Twitch's timeout notices included - and a bar running down to the window
/// closing by itself, above a button that hands out the action on offer.
///
/// The repeated message alert and the moderation assistant's suggestions
/// both use it, and a chatter never has more than one open at a time.
class ModAlertPopup : public BasePopup
{
    Q_OBJECT

public:
    ModAlertPopup(QString channel, QString login, QWidget *parent);

    /// The alert open for this chatter, if there is one
    static ModAlertPopup *openFor(const QString &channel, const QString &login);
    /// The alert for this chatter - the one already open, or a new one
    static ModAlertPopup *obtain(const QString &channel, const QString &login,
                                 QWidget *parent);
    static void closeFor(const QString &channel, const QString &login);
    /// How many alerts are open right now
    static int openCount();

    /// A repeated message alert offering a timeout of @a seconds.
    /// @a timeoutsServed counts the timeouts the chatter has already sat out
    /// for this message. Starts the countdown over.
    void setCase(const QString &displayName, int seconds, int timeoutsServed);

    /// A suggestion from the moderation assistant, with what it rests on.
    /// A suggested length of 0 offers a ban.
    void setSuggestion(const QString &displayName,
                       const ModSuggestion &suggestion);

    /// The repeated message alert with made up lines - the first one, or the
    /// one after a timeout - and buttons that send nothing
    void showTestCase(bool afterTimeout);
    /// A suggestion with made up lines and buttons that send nothing
    void showTestSuggestion();

private:
    void showChatter(const QString &displayName);
    void showRecentLines();
    void showTestChatter(const QString &title);
    void setAction(int seconds);
    /// Headline, why line and button for a suggestion
    void applySuggestion(const ModSuggestion &suggestion);
    void setWhy(const QString &html, const QString &tooltip = {});
    void loadProfile();
    void restartCountdown();
    void tick();

    QString channel_;
    QString login_;
    /// Length of the timeout on offer, 0 for a ban
    int seconds_ = 30;
    bool test_ = false;
    bool profileLoaded_ = false;

    PixmapButton *avatar_{};
    Label *name_{};
    QLabel *details_{};
    QLabel *headline_{};
    QLabel *why_{};
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
