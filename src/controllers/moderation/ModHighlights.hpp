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
#include <optional>
#include <vector>

class QObject;

namespace chatterino {

/// A channel as whosthemod.xyz lists it
struct ModHighlightChannel {
    QString login;
    QString displayName;
    QString profileImageUrl;
    /// -1 when not known
    int modCount = -1;
    bool live = false;
};

/// Marks messages from moderators of the channels the user picked with those
/// channels' profile pictures - someone who moderates two of them gets both.
///
/// The mod lists come from whosthemod.xyz, the site behind the WhoseTheMod
/// plugin, and the feature only runs while that plugin is switched on. They
/// are kept on disk and fetched again every few hours.
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

    using SearchCallback =
        std::function<void(std::optional<std::vector<ModHighlightChannel>>,
                            const QString &nextCursor)>;
    /// The channels whosthemod.xyz knows, matching @a query where it is not
    /// empty, a page at a time. Calls back with nothing while the site offers
    /// no public channel list.
    void searchChannels(const QString &query, const QString &cursor,
                        QObject *caller, SearchCallback done);

    /// Asks the public mod list of @a channel, and keeps it. Calls back with
    /// the number of mods, or -1 when it could not be asked.
    void checkChannel(const QString &channel, QObject *caller,
                      std::function<void(int)> done);

    /// The mod lists or the chosen channels changed
    pajlada::Signals::NoArgSignal updated;

private:
    ModHighlights() = default;

    void load();
    void save() const;
    void refreshIfStale();
    void channelsChanged();
    void fetch(const QStringList &channels);
    void fetchEach(const QStringList &channels);
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
    /// Set once the site turned out to have no bulk mod list, so this run
    /// asks channel by channel
    bool bulkMissing_ = false;
    bool changePending_ = false;
    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
