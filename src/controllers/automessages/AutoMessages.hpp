// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace chatterino {

/// Erweitert -> Autonachrichten: your own account answers by itself when
/// something happens in a channel - a chat message matching a pattern, a
/// sub, a resub, a gift of so many subs, a raid. Each rule has its answers
/// (one is picked at random), a delay drawn anew each time between a least
/// and a most, and a cooldown. Only live messages count, never what is
/// loaded from history, and never your own.
class AutoMessages
{
public:
    enum class Kind {
        Chat,
        Sub,
        Resub,
        Gift,
        Raid,
    };

    struct Rule {
        bool enabled = true;
        Kind kind = Kind::Gift;
        /// Login of the channel, empty for every channel
        QString channel;
        /// Kind::Chat: a regular expression the message has to match
        QString pattern;
        /// Kind::Gift and Kind::Raid: from how many subs or viewers on.
        /// Of several matching rules the one with the highest wins.
        int minCount = 1;
        /// One per line in the settings, one picked at random
        QStringList replies;
        /// Seconds; the wait is drawn between the two each time
        double delayMin = 0.5;
        double delayMax = 3.0;
        /// Seconds before the same rule answers again in the same channel
        int cooldown = 10;
    };

    struct Event {
        Kind kind = Kind::Chat;
        QString channel;
        QString login;
        QString displayName;
        /// Kind::Chat: what was written
        QString text;
        /// Gifted subs, raiders
        int count = 0;
        /// Resub: months in total
        int months = 0;
    };

    static AutoMessages &instance();

    /// Whether they are switched on in @a channel
    static bool isOn(const QString &channel);
    static void setOn(const QString &channel, bool on);
    /// Where they are switched on
    static QStringList onChannels();

    /// From a live chat message; leaves out your own
    void onChat(const QString &channel, const QString &login,
                const QString &displayName, const QString &text);
    /// From a live sub, resub, gift or raid
    void onEvent(const Event &event);

    // The parts below are free of the program around them, for the page and
    // the tests

    static std::vector<Rule> read(const QString &json);
    static QString write(const std::vector<Rule> &rules);

    /// The rule that answers @a event, if any: the right kind and channel,
    /// for chat the pattern, for gifts and raids the highest threshold
    /// reached. Rules still cooling down (@a lastFired by index, per
    /// channel) are passed over.
    static std::optional<size_t> pick(
        const std::vector<Rule> &rules, const Event &event,
        const QHash<QString, QDateTime> &lastFired, const QDateTime &now);

    /// The answer with {user}, {anzahl}, {monate} and {kanal} filled in
    static QString fill(const QString &reply, const Event &event);

    /// Why @a pattern is no valid regular expression, empty when it is
    static QString problemWith(const QString &pattern);

    /// What the kinds are called on the page
    static QString nameOf(Kind kind);

    /// Key for lastFired
    static QString cooldownKey(size_t rule, const QString &channel);

private:
    AutoMessages() = default;

    QHash<QString, QDateTime> lastFired_;
    /// No two answers in one channel closer than this, whatever the rules
    QHash<QString, QDateTime> lastSent_;
};

}  // namespace chatterino
