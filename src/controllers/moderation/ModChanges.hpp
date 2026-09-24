// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <pajlada/signals/signalholder.hpp>
#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <vector>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;
struct Message;
using MessagePtr = std::shared_ptr<const Message>;

/// Mod highlights -> Mod changes: a channel of its own, opened as a tab,
/// that says who became a mod of the channels picked under Mod highlights,
/// or stopped being one. The mod lists are those Mod highlights fetches
/// from whosthemod.xyz anyway - every quarter hour while this is on. What
/// was said is kept, so the tab shows it again after a restart.
class ModChanges : public QObject
{
public:
    static ModChanges &instance();

    /// The channel its tab shows, "/modchanges"
    ChannelPtr channel() const;

    /// Fetches the mod lists now rather than at the next quarter hour
    void checkNow();

    /// Opens the tab in the main window, or shows it when it is there
    static void openTab();

    struct Change {
        QDateTime when;
        QString channel;
        QString login;
        /// Became a mod - or, false, stopped being one
        bool added = true;

        QJsonObject toJson() const;
        static Change fromJson(const QJsonObject &object);
    };

    /// The message for @a change, on @a background when that is a colour
    static MessagePtr messageFor(const Change &change,
                                 const QColor &background = {});

    /// The colour picked for a mod who came or went, none when colours are
    /// off
    static QColor colorFor(bool added);

    /// The sound picked for a mod who came or went - empty for the usual ping
    static QString soundFor(bool added);

    /// How many changes are kept to show again after a restart
    static constexpr int MOST_KEPT = 300;

private:
    ModChanges();

    void changed(const QString &channel, const QStringList &came,
                 const QStringList &went);
    void show(const Change &change);

    void load();
    void save() const;

    ChannelPtr channel_;
    QTimer timer_;
    std::vector<Change> history_;
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
