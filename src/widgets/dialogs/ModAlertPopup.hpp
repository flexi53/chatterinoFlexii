// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BasePopup.hpp"

#include <pajlada/signals/scoped-connection.hpp>
#include <QColor>
#include <QElapsedTimer>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QFrame;
class QHBoxLayout;
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
/// The repeated message alert, the emote spam alert and the moderation
/// assistant's suggestions all use it, and a chatter never has more than one open at a time.
class AlertLevelBar;

class ModAlertPopup : public BasePopup
{
    Q_OBJECT

public:
    enum class Kind {
        RepeatedMessage,
        EmoteSpam,
        /// A word off the list under Mod-Assistent -> Wörter
        Word,
        Suggestion,
    };

    ModAlertPopup(QString channel, QString login, QWidget *parent);

    /// Which alert the window is showing
    Kind kind() const
    {
        return this->kind_;
    }

    /// The alert open for this chatter, if there is one
    static ModAlertPopup *openFor(const QString &channel, const QString &login);
    /// The alert for this chatter - the one already open, or a new one
    static ModAlertPopup *obtain(const QString &channel, const QString &login,
                                 QWidget *parent);
    static void closeFor(const QString &channel, const QString &login);
    /// How many alerts are open right now
    static int openCount();

    /// The colour an alert of @a kind sets its reason off in, lit up
    static QColor reasonColor(Kind kind);
    /// The colour an alert of @a kind starts out with
    static QColor defaultReasonColor(Kind kind);
    /// @a picked made bright and strong, so a reason in it always stands out.
    /// Grey, black and white have no hue to light up and give @a fallback.
    static QColor vividColor(const QColor &picked, const QColor &fallback);
    /// Style sheet for the "REASON" chip in @a color
    static QString reasonTagStyle(const QColor &color);

    /// The sounds that come with the app, as setting value and name
    static std::vector<std::pair<QString, QString>> builtInSounds();
    /// What a sound setting plays: the ping for an empty one or a file that
    /// is gone. The sound backend only plays files, so a built-in sound is
    /// copied out of the app the first time it is wanted.
    static QUrl soundUrl(const QString &choice);
    /// The sound an alert of @a kind plays
    static QUrl soundFor(Kind kind);

    /// Shows the window and brings it to the front. On macOS it is also made
    /// to show on every space, so it does not stay behind on a full screen
    /// app's space when macOS switches back to the app.
    void present();

    /// A repeated message alert offering a timeout of @a seconds, step
    /// @a step counted from 0. @a timeoutsServed counts the timeouts the
    /// chatter has already sat out for this message; a step higher than that
    /// means nobody acted on an earlier alert. Starts the countdown over.
    void setCase(const QString &displayName, int seconds, int step,
                 int timeoutsServed);

    /// A suggestion from the moderation assistant, with what it rests on.
    /// A suggested length of 0 offers a ban.
    void setSuggestion(const QString &displayName,
                       const ModSuggestion &suggestion);

    /// @a emotes emotes across @a messages messages within @a window seconds.
    /// @a action is EmoteSpamDetector::DELETE to delete @a messageIds,
    /// otherwise a timeout length, at step @a step of @a stepCount counted
    /// from 0. @a actionsServed counts the deletions and timeouts the chatter
    /// has had; a step higher than that means nobody acted on an earlier
    /// alert.
    void setEmoteSpam(const QString &displayName, int emotes, int messages,
                      int window, int action, const QStringList &messageIds,
                      int step, int actionsServed, int stepCount, int streak);

    /// The emote alert with made up lines, at step @a step
    void showTestEmoteSpam(int step);

    /// Shows that @a displayName said @a word - written as @a asWritten,
    /// which may be dressed up - with @a action on offer, deleting the
    /// message or a timeout, at @a step of @a stepCount
    void setWordAlert(const QString &displayName, const QString &word,
                      const QString &asWritten, int action,
                      const QStringList &messageIds, int step,
                      int actionsServed, int stepCount);

    /// The word alert with a made-up case, to see what it looks like
    void showTestWordAlert(int step);

    /// The repeated message alert with made up lines - the first one, or the
    /// one after a timeout - and buttons that send nothing
    void showTestCase(bool afterTimeout);
    /// A suggestion with made up lines and buttons that send nothing
    void showTestSuggestion();

protected:
    /// Puts the buttons back in the colours of the theme that just came
    void themeChangedEvent() override;

private:
    void showChatter(const QString &displayName);
    void showRecentLines();
    void showTestChatter(const QString &title);
    /// A button for each action there is, the one recommended - DELETE, a
    /// timeout length, or 0 for a ban - lit up in the alert's colour, so a
    /// special case can still get something else
    void setActions(int recommended);
    /// Carries out @a action and closes the window
    void act(int action);
    /// Warns as twitch.tv does, with a reason that fits the alert - and
    /// counts it as having been dealt with
    void warn();
    /// Headline, why line and button for a suggestion
    void applySuggestion(const ModSuggestion &suggestion);
    void applyEmoteSpam(int emotes, int messages, int window, int action,
                        int step, int actionsServed, int stepCount, int streak);
    /// How the buttons that are not the recommended one look: the rounded,
    /// quiet shape the rest of ChattiFlexii uses, rather than the system's
    QString quietButtonStyle() const;

    /// The reason box: what the alert is about in a few words, set off in the
    /// colour of its kind, with what backs it up underneath
    void setReason(const QString &reason, const QString &details,
                   const QString &tooltip = {});
    void loadProfile();
    void restartCountdown();
    /// Puts a window about to show for the first time where the moderator
    /// last moved one
    void placeWindow();
    /// Notes the size and place the window came up with, so a size or place
    /// the moderator gives it can be told apart
    void rememberShownGeometry();
    void tick();

    QString channel_;
    QString login_;
    /// The messages a delete takes down
    QStringList deleteIds_;
    Kind kind_ = Kind::Suggestion;
    bool test_ = false;
    /// Whether the window has come up once, so only that plays the ping
    bool announced_ = false;
    bool placed_ = false;
    bool profileLoaded_ = false;

    PixmapButton *avatar_{};
    Label *name_{};
    QLabel *details_{};
    QLabel *headline_{};
    QFrame *reasonBox_{};
    QLabel *reasonTag_{};
    QLabel *reason_{};
    QLabel *reasonDetails_{};
    /// ChattiFlexii: how far past the number the alert goes off at
    AlertLevelBar *reasonLevel_{};
    /// The colour of the reason shown, which the countdown bar takes too
    QColor reasonColor_;
    ChannelView *messages_{};
    QLabel *testNote_{};
    QWidget *countdownBar_{};
    QPushButton *ignore_{};
    /// ChattiFlexii: on a suggestion - says this does not fit here, so
    /// nothing like it is suggested again
    QPushButton *doesNotFit_{};
    /// What the suggestion was about, to turn it down by
    QString suggestedText_;
    /// The action buttons, rebuilt for each alert
    QHBoxLayout *actions_{};

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

};

}  // namespace chatterino
