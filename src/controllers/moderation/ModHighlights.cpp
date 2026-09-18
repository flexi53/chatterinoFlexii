// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/ModHighlights.hpp"

#include "Application.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"

#ifdef CHATTERINO_HAVE_PLUGINS
#    include "controllers/plugins/PluginController.hpp"
#endif

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTimer>

#include <chrono>

namespace {

using namespace chatterino;

const QString API_BASE = QStringLiteral("https://whosthemod.xyz/api/public");
/// How old the mod lists may get before they are fetched again
constexpr qint64 REFRESH_AFTER_SECONDS = 6 * 60 * 60;
constexpr int TIMEOUT_MS = 15000;

QString storePath()
{
    return QDir(getApp()->getPaths().miscDirectory)
        .absoluteFilePath(QStringLiteral("mod-highlights.json"));
}

/// The chosen channels, lower case, each once, in the order they were picked
QStringList chosenChannels()
{
    QStringList channels;
    for (const auto &channel : getSettings()->modHighlightChannels.getValue())
    {
        const auto login = channel.trimmed().toLower();
        if (!login.isEmpty() && !channels.contains(login))
        {
            channels.append(login);
        }
    }
    return channels;
}

/// The moderators left out of the marking: the names listed, and where the
/// user wants it any name ending in "bot"
struct Exclusions {
    QSet<QString> names;
    bool botNames = false;

    bool has(const QString &login) const
    {
        return this->names.contains(login) ||
               (this->botNames && login.endsWith(QStringLiteral("bot")));
    }
};

Exclusions exclusions()
{
    static const QRegularExpression separators(QStringLiteral("[\\s,;]+"));

    Exclusions result;
    const auto listed = getSettings()->modHighlightsIgnoredUsers.getValue();
    for (auto name : listed.split(separators, Qt::SkipEmptyParts))
    {
        name = name.toLower();
        while (name.startsWith(u'@') || name.startsWith(u'#'))
        {
            name.remove(0, 1);
        }
        if (!name.isEmpty())
        {
            result.names.insert(name);
        }
    }
    result.botNames = getSettings()->modHighlightsIgnoreBotNames.getValue();
    return result;
}

QStringList loginList(const QJsonArray &array)
{
    QStringList logins;
    for (const auto &value : array)
    {
        const auto login = value.toString().trimmed().toLower();
        if (!login.isEmpty())
        {
            logins.append(login);
        }
    }
    return logins;
}

}  // namespace

namespace chatterino {

ModHighlights &ModHighlights::instance()
{
    // Never destroyed, so nothing is torn down after the settings at exit
    static auto *highlights = new ModHighlights;
    return *highlights;
}

bool ModHighlights::pluginAvailable()
{
#ifdef CHATTERINO_HAVE_PLUGINS
    return getSettings()->pluginsEnabled.getValue() &&
           PluginController::isPluginEnabled(QStringLiteral("WhoseTheMod"));
#else
    return false;
#endif
}

QString ModHighlights::captionFor(const QString &login) const
{
    if (!getSettings()->modHighlightsEnabled.getValue() || !pluginAvailable())
    {
        return {};
    }

    std::lock_guard lock(this->mutex_);
    return this->captions_.value(login.toLower());
}

void ModHighlights::start()
{
    if (this->context_ != nullptr)
    {
        return;
    }
    this->context_ = new QObject;

    this->load();

    // Looked at every hour, fetched when the lists have grown old
    auto *timer = new QTimer(this->context_);
    timer->setInterval(std::chrono::hours(1));
    QObject::connect(timer, &QTimer::timeout, this->context_, [this] {
        this->refreshIfStale();
    });
    timer->start();

    getSettings()->modHighlightChannels.connect(
        [this](const auto &, auto) {
            this->channelsChanged();
        },
        this->connections_, false);
    getSettings()->modHighlightsEnabled.connect(
        [this](const auto &, auto) {
            this->refreshIfStale();
        },
        this->connections_, false);
    getSettings()->modHighlightsIgnoredUsers.connect(
        [this](const auto &, auto) {
            this->exclusionsChanged();
        },
        this->connections_, false);
    getSettings()->modHighlightsIgnoreBotNames.connect(
        [this](const auto &, auto) {
            this->exclusionsChanged();
        },
        this->connections_, false);

    // A little after start, once the app has settled and logged in
    QTimer::singleShot(5000, this->context_, [this] {
        this->refreshIfStale();
    });
}

void ModHighlights::refresh()
{
    this->start();
    this->fetch(chosenChannels());
}

QDateTime ModHighlights::lastUpdated() const
{
    std::lock_guard lock(this->mutex_);
    return this->updated_;
}

int ModHighlights::modCount(const QString &channel) const
{
    std::lock_guard lock(this->mutex_);
    const auto it = this->mods_.constFind(channel.toLower());
    return it == this->mods_.constEnd() ? -1 : static_cast<int>(it->size());
}

int ModHighlights::markedCount() const
{
    std::lock_guard lock(this->mutex_);
    return static_cast<int>(this->captions_.size());
}

void ModHighlights::checkChannel(const QString &channel, QObject *caller,
                                 std::function<void(int)> done)
{
    this->start();

    NetworkRequest(API_BASE + QStringLiteral("/channel-mods?channel=") + channel,
                   NetworkRequestType::Get)
        .timeout(TIMEOUT_MS)
        .caller(caller)
        .onSuccess([this, channel, done](const NetworkResult &result) {
            const auto root = result.parseJson();
            if (!root.contains(QStringLiteral("mods")))
            {
                done(-1);
                return;
            }

            const auto mods =
                loginList(root.value(QStringLiteral("mods")).toArray());
            this->store(channel, mods);
            this->listsChanged();
            done(static_cast<int>(mods.size()));
        })
        .onError([done](const NetworkResult &) {
            done(-1);
        })
        .execute();
}

void ModHighlights::load()
{
    QFile file(storePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }

    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    const auto chosen = chosenChannels();

    std::lock_guard lock(this->mutex_);
    this->updated_ = QDateTime::fromString(
        root.value(QStringLiteral("updated")).toString(), Qt::ISODate);
    const auto channels = root.value(QStringLiteral("channels")).toObject();
    for (auto it = channels.begin(); it != channels.end(); ++it)
    {
        this->mods_.insert(it.key(), loginList(it.value().toArray()));
    }
    this->rebuildCaptionsLocked(chosen);
}

void ModHighlights::save() const
{
    QJsonObject root;
    QJsonObject channels;
    {
        std::lock_guard lock(this->mutex_);
        for (auto it = this->mods_.constBegin(); it != this->mods_.constEnd();
             ++it)
        {
            channels.insert(it.key(), QJsonArray::fromStringList(it.value()));
        }
        root.insert(QStringLiteral("updated"),
                    this->updated_.toString(Qt::ISODate));
    }
    root.insert(QStringLiteral("channels"), channels);

    QSaveFile file(storePath());
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void ModHighlights::refreshIfStale()
{
    if (!getSettings()->modHighlightsEnabled.getValue() || !pluginAvailable())
    {
        return;
    }

    const auto chosen = chosenChannels();
    QStringList missing;
    bool stale = false;
    {
        std::lock_guard lock(this->mutex_);
        stale = !this->updated_.isValid() ||
                this->updated_.secsTo(QDateTime::currentDateTimeUtc()) >=
                    REFRESH_AFTER_SECONDS;
        for (const auto &channel : chosen)
        {
            if (!this->mods_.contains(channel))
            {
                missing.append(channel);
            }
        }
    }
    this->fetch(stale ? chosen : missing);
}

void ModHighlights::channelsChanged()
{
    const auto chosen = chosenChannels();
    {
        std::lock_guard lock(this->mutex_);
        // Lists of channels no longer chosen are let go
        for (auto it = this->mods_.begin(); it != this->mods_.end();)
        {
            it = chosen.contains(it.key()) ? std::next(it)
                                           : this->mods_.erase(it);
        }
        this->rebuildCaptionsLocked(chosen);
    }
    this->save();
    this->updated.invoke();
    this->refreshIfStale();
}

void ModHighlights::exclusionsChanged()
{
    const auto chosen = chosenChannels();
    {
        std::lock_guard lock(this->mutex_);
        this->rebuildCaptionsLocked(chosen);
    }
    this->updated.invoke();
}

void ModHighlights::fetch(const QStringList &channels)
{
    if (channels.isEmpty())
    {
        return;
    }
    this->start();

    for (const auto &channel : channels)
    {
        NetworkRequest(
            API_BASE + QStringLiteral("/channel-mods?channel=") + channel,
            NetworkRequestType::Get)
            .timeout(TIMEOUT_MS)
            .caller(this->context_)
            .onSuccess([this, channel](const NetworkResult &result) {
                const auto root = result.parseJson();
                if (!root.contains(QStringLiteral("mods")))
                {
                    return;
                }
                this->store(channel, loginList(root.value(QStringLiteral("mods"))
                                                   .toArray()));
                this->listsChanged();
            })
            .execute();
    }
}

void ModHighlights::store(const QString &channel, const QStringList &mods)
{
    std::lock_guard lock(this->mutex_);
    this->mods_.insert(channel, mods);
    this->updated_ = QDateTime::currentDateTimeUtc();
}

void ModHighlights::listsChanged()
{
    if (this->changePending_ || this->context_ == nullptr)
    {
        return;
    }
    this->changePending_ = true;

    QTimer::singleShot(300, this->context_, [this] {
        this->changePending_ = false;
        const auto chosen = chosenChannels();
        {
            std::lock_guard lock(this->mutex_);
            this->rebuildCaptionsLocked(chosen);
        }
        this->save();
        this->updated.invoke();
    });
}

void ModHighlights::rebuildCaptionsLocked(const QStringList &chosen)
{
    const auto excluded = exclusions();

    this->captions_.clear();
    for (const auto &channel : chosen)
    {
        for (const auto &mod : this->mods_.value(channel))
        {
            // Bots are mods everywhere and would carry every picture
            if (excluded.has(mod))
            {
                continue;
            }
            auto &caption = this->captions_[mod];
            if (!caption.isEmpty())
            {
                caption += u' ';
            }
            caption += u'@' + channel;
        }
    }
}

}  // namespace chatterino
