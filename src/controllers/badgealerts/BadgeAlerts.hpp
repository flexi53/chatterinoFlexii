// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/badgebase/BadgeBase.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QColor>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <memory>
#include <optional>
#include <vector>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;
struct Message;
using MessagePtr = std::shared_ptr<const Message>;

/// Badges -> New badges: a channel of its own, opened as a tab, that says
/// when a badge can be had - from BadgeBase, with each user's own key, and
/// from Twitch, which needs none. Asks every quarter hour while it is
/// switched on, and remembers what it said, so nothing comes twice.
class BadgeAlerts : public QObject
{
public:
    static BadgeAlerts &instance();

    /// The channel its tab shows, "/badges"
    ChannelPtr channel() const;

    /// Asks now rather than at the next quarter hour
    void checkNow();

    /// Opens the tab in the main window, or shows it when it is there
    static void openTab();

    enum class Kind {
        Available,
        Upcoming,
        Ending,
        NewOnTwitch,
    };

    struct Event {
        Kind kind;
        badgebase::Badge badge;

        /// What it is remembered by
        QString key() const;
    };

    struct Options {
        bool available = true;
        bool upcoming = true;
        bool ending = true;
        bool onlyMissing = true;
    };

    /// What is to be said, from what BadgeBase knows. @a missing is what the
    /// user still lacks, when known; @a reported what was said before.
    static std::vector<Event> events(
        const std::vector<badgebase::Badge> &claimable,
        const std::vector<badgebase::Badge> &upcoming,
        const std::optional<QSet<QString>> &missing,
        const QSet<QString> &reported, const QDateTime &now,
        const Options &options);

    /// How long before its end a badge is said to end soon
    static constexpr int ENDING_SECONDS = 24 * 60 * 60;

    /// The message for @a event, with the picture at @a picture - Twitch's
    /// own at twice the size when @a twitchPicture, BadgeBase's otherwise -
    /// on @a background when that is a colour
    static MessagePtr messageFor(const Event &event, const QString &picture,
                                 bool twitchPicture,
                                 const QColor &background = {});

    /// The colour picked for @a kind, or none when colours are off
    static QColor colorFor(Kind kind);

private:
    BadgeAlerts();

    void check();
    void checkBadgeBase(const QString &key);
    void checkTwitch();
    void say(const std::vector<Event> &events);
    void note(const QString &text);
    QString pictureFor(const badgebase::Badge &badge, bool &twitch) const;

    void load();
    void save() const;

    ChannelPtr channel_;
    QTimer timer_;
    QSet<QString> reported_;
    /// Every global badge Twitch had when last asked - "set/version"
    QSet<QString> twitchKnown_;
    /// Twitch's own pictures, by badge set
    QHash<QString, QString> twitchPictures_;
    /// Whether this start has said what can be had now
    bool summarized_ = false;
    bool noKeySaid_ = false;
    /// The last thing that went wrong - said once, not every quarter hour
    QString lastProblem_;
    bool asking_ = false;
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
