// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <pajlada/signals/signal.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>
#include <mutex>

class QObject;

namespace chatterino {

/// Where someone moderates, as whosthemod.xyz knows it
struct ModChannelsOfUser {
    QStringList channels;
    /// How many there are, which can be more than are listed
    int total = 0;
    QStringList former;
    int formerTotal = 0;
};

/// Marks messages from moderators of the channels the user picked with those
/// channels' profile pictures - someone who moderates two of them gets both.
///
/// The mod lists come from whosthemod.xyz, the site behind the WhoseTheMod
/// plugin - the same public list its /modcheck shows - and the feature only
/// runs while that plugin is switched on. They are kept on disk and fetched
/// again every few hours.
class ModHighlights
{
public:
    static ModHighlights &instance();

    /// Whether plugins are on and WhoseTheMod is among them
    static bool pluginAvailable();

    /// The chosen channels @a login moderates, as a caption of @names - empty
    /// when there are none, or the feature is off. Safe from any thread.
    QString captionFor(const QString &login) const;

    /// Loads the saved lists and starts keeping them up to date. Only the
    /// first call does anything; it has to come from the GUI thread.
    void start();
    /// Fetches the chosen channels' mod lists again now
    void refresh();

    /// When a mod list was last fetched; invalid if never
    QDateTime lastUpdated() const;
    /// How many mods @a channel has as last fetched, -1 when not fetched
    int modCount(const QString &channel) const;
    /// How many different people moderate the chosen channels
    int markedCount() const;

    /// Asks the public mod list of @a channel, and keeps it. Calls back with
    /// the number of mods, or -1 when it could not be asked.
    void checkChannel(const QString &channel, QObject *caller,
                      std::function<void(int)> done);

    /// Where @a login moderates, as /wtm in the plugin shows it. Answers are
    /// kept for a few minutes, so opening a user card again asks nobody.
    /// Calls back only when there is an answer. GUI thread only.
    void lookUpModChannels(const QString &login, QObject *caller,
                           std::function<void(const ModChannelsOfUser &)> done);

    /// The mod lists or the chosen channels changed
    pajlada::Signals::NoArgSignal updated;

private:
    ModHighlights() = default;

    void load();
    void save() const;
    void refreshIfStale();
    void channelsChanged();
    /// The bots to leave out changed - the captions are made again, nothing
    /// is fetched
    void exclusionsChanged();
    void fetch(const QStringList &channels);
    void store(const QString &channel, const QStringList &mods);
    /// Takes in what was fetched, a moment later so answers arriving one
    /// channel at a time are taken in together
    void listsChanged();
    void rebuildCaptionsLocked(const QStringList &chosen);

    mutable std::mutex mutex_;
    /// Each channel's mods, as last fetched
    QHash<QString, QStringList> mods_;
    /// For each mod, the chosen channels they moderate as a caption
    QHash<QString, QString> captions_;
    QDateTime updated_;

    /// Owns the timers and network callbacks; made on the GUI thread by start
    QObject *context_ = nullptr;
    bool changePending_ = false;
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
