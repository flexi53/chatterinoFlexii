// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/ModChanges.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/moderation/ModHighlights.hpp"
#include "controllers/sound/ISoundController.hpp"
#include "messages/Link.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/CombinePath.hpp"
#include "util/OpenOwnTab.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUrl>

namespace chatterino {

namespace {

constexpr int CHECK_MINUTES = 15;

/// The tab's channel: nothing to write in, and called what it is
class ModChangesChannel : public Channel
{
public:
    ModChangesChannel()
        : Channel(QStringLiteral("/modchanges"), Type::Misc)
    {
    }

    bool isWritable() const override
    {
        return false;
    }

    const QString &getLocalizedName() const override
    {
        static const QString name = QStringLiteral("Mod-Änderungen");
        return name;
    }
};

QString historyPath()
{
    return combinePath(getApp()->getPaths().settingsDirectory,
                       QStringLiteral("mod-changes.json"));
}

}  // namespace

ModChanges &ModChanges::instance()
{
    static ModChanges changes;
    return changes;
}

ModChanges::ModChanges()
    : channel_(std::make_shared<ModChangesChannel>())
{
    // What was said before, so the tab is not empty after a restart
    this->load();
    for (const auto &change : this->history_)
    {
        this->channel_->addMessage(messageFor(change, colorFor(change.added)),
                                   MessageContext::Original);
    }

    std::ignore = ModHighlights::instance().modsChanged.connect(
        [this](const QString &channel, const QStringList &came,
               const QStringList &went) {
            if (getSettings()->modChangesEnabled)
            {
                this->changed(channel, came, went);
            }
        });

    this->timer_.setInterval(CHECK_MINUTES * 60 * 1000);
    QObject::connect(&this->timer_, &QTimer::timeout, this, [this] {
        this->checkNow();
    });
    getSettings()->modChangesEnabled.connect(
        [this](const bool enabled, auto) {
            if (enabled)
            {
                this->timer_.start();
                // A little after start, once the app has settled
                QTimer::singleShot(10000, this, [this] {
                    this->checkNow();
                });
            }
            else
            {
                this->timer_.stop();
            }
        },
        this->connections_);
}

ChannelPtr ModChanges::channel() const
{
    return this->channel_;
}

void ModChanges::checkNow()
{
    // Mod highlights asks whosthemod.xyz for every channel picked there and
    // says what differs from last time
    ModHighlights::instance().refresh();
}

void ModChanges::openTab()
{
    openOwnTab(instance().channel());
}

QJsonObject ModChanges::Change::toJson() const
{
    return {
        {"when", this->when.toUTC().toString(Qt::ISODate)},
        {"channel", this->channel},
        {"login", this->login},
        {"added", this->added},
    };
}

ModChanges::Change ModChanges::Change::fromJson(const QJsonObject &object)
{
    return {
        .when =
            QDateTime::fromString(object.value("when").toString(), Qt::ISODate),
        .channel = object.value("channel").toString(),
        .login = object.value("login").toString(),
        .added = object.value("added").toBool(true),
    };
}

QColor ModChanges::colorFor(bool added)
{
    const auto *s = getSettings();
    if (!s->modChangesColored)
    {
        return {};
    }
    return QColor(added ? s->modChangesColorAdded.getValue()
                        : s->modChangesColorRemoved.getValue());
}

MessagePtr ModChanges::messageFor(const Change &change,
                                  const QColor &background)
{
    MessageBuilder builder;
    builder->flags.set(MessageFlag::DoNotLog);
    // On its colour as a highlight is - without lighting the tab up as a
    // mention would
    if (background.isValid())
    {
        builder->flags.set(MessageFlag::Highlighted);
        builder->highlightColor = std::make_shared<QColor>(background);
    }
    const auto when = change.when.isValid() ? change.when.toLocalTime()
                                            : QDateTime::currentDateTime();
    builder->serverReceivedTime = when;
    builder.emplace<TimestampElement>(when.time());

    // The name opens their user card, the channel jumps to it
    builder
        .emplace<TextElement>(change.login, MessageElementFlag::Text,
                              MessageColor::Text, FontStyle::ChatMediumBold)
        ->setLink(Link(Link::UserInfo, change.login));
    const auto middle = change.added ? QStringLiteral("ist jetzt Mod in")
                                     : QStringLiteral("ist kein Mod mehr in");
    builder.emplace<TextElement>(middle, MessageElementFlag::Text,
                                 MessageColor::System);
    builder
        .emplace<TextElement>('#' + change.channel, MessageElementFlag::Text,
                              MessageColor::Link)
        ->setLink(Link(Link::JumpToChannel, change.channel));

    const auto text = change.login + ' ' + middle + " #" + change.channel;
    builder->messageText = text;
    builder->searchText = text;
    return builder.release();
}

void ModChanges::changed(const QString &channel, const QStringList &came,
                         const QStringList &went)
{
    const auto *s = getSettings();
    const auto ignored = s->modHighlightsIgnoredUsers.getValue();
    const bool ignoreBotNames = s->modHighlightsIgnoreBotNames;
    const auto now = QDateTime::currentDateTimeUtc();

    bool any = false;
    const auto add = [&](const QString &login, bool added) {
        // Bots come and go with every setup - left out as under Mod
        // highlights, when that is asked for
        if (s->modChangesHideBots &&
            ModHighlights::isExcludedMod(login, ignored, ignoreBotNames))
        {
            return;
        }
        this->show({now, channel, login, added});
        any = true;
    };
    for (const auto &login : came)
    {
        add(login, true);
    }
    for (const auto &login : went)
    {
        add(login, false);
    }

    if (!any)
    {
        return;
    }
    this->save();
    if (s->modChangesSound)
    {
        getApp()->getSound()->play(
            QUrl(QStringLiteral("qrc:/sounds/ping2.wav")));
    }
}

void ModChanges::show(const Change &change)
{
    this->channel_->addMessage(messageFor(change, colorFor(change.added)),
                               MessageContext::Original);
    this->history_.push_back(change);
    while (this->history_.size() > static_cast<size_t>(MOST_KEPT))
    {
        this->history_.erase(this->history_.begin());
    }
}

void ModChanges::load()
{
    QFile file(historyPath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    for (const auto &entry : QJsonDocument::fromJson(file.readAll())
                                 .object()
                                 .value("changes")
                                 .toArray())
    {
        auto change = Change::fromJson(entry.toObject());
        if (!change.channel.isEmpty() && !change.login.isEmpty())
        {
            this->history_.push_back(std::move(change));
        }
    }
    while (this->history_.size() > static_cast<size_t>(MOST_KEPT))
    {
        this->history_.erase(this->history_.begin());
    }
}

void ModChanges::save() const
{
    QJsonArray changes;
    for (const auto &change : this->history_)
    {
        changes.append(change.toJson());
    }
    QSaveFile file(historyPath());
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    file.write(QJsonDocument(QJsonObject{{"changes", changes}})
                   .toJson(QJsonDocument::Compact));
    file.commit();
}

}  // namespace chatterino
