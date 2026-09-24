// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/saved/SavedMessages.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "messages/Link.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/CombinePath.hpp"
#include "util/OpenOwnTab.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>

namespace chatterino {

namespace {

/// The tab's channel: nothing to write in, and called what it is
class SavedChannel : public Channel
{
public:
    SavedChannel()
        : Channel(QStringLiteral("/gemerkt"), Type::Misc)
    {
    }

    bool isWritable() const override
    {
        return false;
    }

    const QString &getLocalizedName() const override
    {
        static const QString name = QStringLiteral("Gemerkt");
        return name;
    }
};

QString keptPath()
{
    return combinePath(getApp()->getPaths().settingsDirectory,
                       QStringLiteral("saved-messages.json"));
}

}  // namespace

SavedMessages &SavedMessages::instance()
{
    static SavedMessages saved;
    return saved;
}

SavedMessages::SavedMessages()
    : channel_(std::make_shared<SavedChannel>())
{
    // What was kept before, so the tab stands as it was after a restart
    this->load();
    this->fill();
}

ChannelPtr SavedMessages::channel() const
{
    return this->channel_;
}

void SavedMessages::openTab()
{
    openOwnTab(instance().channel());
}

QString SavedMessages::idFor(const QString &channelName,
                             const MessagePtr &message)
{
    if (message == nullptr)
    {
        return {};
    }

    // Twitch gives every message an id of its own; where there is none - a
    // system message, say - the channel, the writer and the text stand for it
    QString source = channelName + u'\n' + message->id;
    if (message->id.isEmpty())
    {
        source += message->loginName + u'\n' + message->messageText;
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha1)
            .toHex()
            .left(16));
}

QJsonObject SavedMessages::Entry::toJson() const
{
    return {
        {"id", this->id},
        {"when", this->when.toUTC().toString(Qt::ISODate)},
        {"channel", this->channel},
        {"displayName", this->displayName},
        {"login", this->login},
        {"text", this->text},
    };
}

SavedMessages::Entry SavedMessages::Entry::fromJson(const QJsonObject &object)
{
    return {
        .id = object.value("id").toString(),
        .when =
            QDateTime::fromString(object.value("when").toString(), Qt::ISODate),
        .channel = object.value("channel").toString(),
        .displayName = object.value("displayName").toString(),
        .login = object.value("login").toString(),
        .text = object.value("text").toString(),
    };
}

QColor SavedMessages::color()
{
    const auto *s = getSettings();
    if (!s->savedMessagesColored)
    {
        return {};
    }
    return QColor(s->savedMessagesColor.getValue());
}

MessagePtr SavedMessages::messageFor(const Entry &entry,
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

    const auto when = entry.when.isValid() ? entry.when.toLocalTime()
                                           : QDateTime::currentDateTime();
    builder->serverReceivedTime = when;
    builder.emplace<TimestampElement>(when.time());

    // Where it was written - a click jumps to that channel
    if (!entry.channel.isEmpty())
    {
        builder
            .emplace<TextElement>('#' + entry.channel,
                                  MessageElementFlag::Text, MessageColor::Link)
            ->setLink(Link(Link::JumpToChannel, entry.channel));
    }

    // Who wrote it - a click opens their card
    const auto name =
        entry.displayName.isEmpty() ? entry.login : entry.displayName;
    if (!name.isEmpty())
    {
        builder
            .emplace<TextElement>(name + u':', MessageElementFlag::Text,
                                  MessageColor::Text, FontStyle::ChatMediumBold)
            ->setLink(Link(Link::UserInfo, entry.login.isEmpty()
                                               ? entry.displayName
                                               : entry.login));
    }

    builder.emplace<TextElement>(entry.text, MessageElementFlag::Text,
                                 MessageColor::Text);

    // The way back out of the tab
    builder
        .emplace<TextElement>(QStringLiteral("[vergessen]"),
                              MessageElementFlag::Text, MessageColor::System,
                              FontStyle::ChatMediumSmall)
        ->setLink(Link(Link::ForgetSaved, entry.id))
        ->setTooltip("Diese Nachricht nicht mehr merken");

    const auto text = (entry.channel.isEmpty() ? QString()
                                               : '#' + entry.channel + ' ') +
                      (name.isEmpty() ? QString() : name + ": ") + entry.text;
    builder->messageText = text;
    builder->searchText = text;
    return builder.release();
}

void SavedMessages::remember(const QString &channelName,
                             const MessagePtr &message)
{
    if (message == nullptr)
    {
        return;
    }

    Entry entry{
        .id = idFor(channelName, message),
        .when = message->serverReceivedTime.isValid()
                    ? message->serverReceivedTime
                    : QDateTime::currentDateTimeUtc(),
        .channel = channelName,
        .displayName = message->displayName,
        .login = message->loginName,
        .text = message->messageText,
    };

    const auto already =
        std::find_if(this->kept_.begin(), this->kept_.end(),
                     [&entry](const auto &kept) {
                         return kept.id == entry.id;
                     });
    if (already != this->kept_.end())
    {
        // Already there - the tab still comes up, so it is clear where it went
        openTab();
        return;
    }

    this->kept_.push_back(std::move(entry));
    while (this->kept_.size() > static_cast<size_t>(MOST_KEPT))
    {
        this->kept_.erase(this->kept_.begin());
    }
    this->save();
    this->fill();
    openTab();
}

void SavedMessages::forget(const QString &id)
{
    const auto before = this->kept_.size();
    std::erase_if(this->kept_, [&id](const auto &entry) {
        return entry.id == id;
    });
    if (this->kept_.size() == before)
    {
        return;
    }
    this->save();
    this->fill();
}

void SavedMessages::forgetAll()
{
    if (this->kept_.empty())
    {
        return;
    }
    this->kept_.clear();
    this->save();
    this->fill();
}

std::vector<SavedMessages::Entry> SavedMessages::kept() const
{
    return this->kept_;
}

void SavedMessages::fill()
{
    // Thrown away and written again, so a forgotten one really is gone
    this->channel_->clearMessages();
    const auto background = color();
    for (const auto &entry : this->kept_)
    {
        this->channel_->addMessage(messageFor(entry, background),
                                   MessageContext::Original);
    }
    if (this->kept_.empty())
    {
        this->channel_->addSystemMessage(
            "Nichts gemerkt. Rechtsklick auf eine Nachricht → Merken legt "
            "sie hier ab.");
    }
}

void SavedMessages::load()
{
    QFile file(keptPath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    for (const auto &value : QJsonDocument::fromJson(file.readAll())
                                 .object()
                                 .value("messages")
                                 .toArray())
    {
        auto entry = Entry::fromJson(value.toObject());
        if (!entry.id.isEmpty())
        {
            this->kept_.push_back(std::move(entry));
        }
    }
    while (this->kept_.size() > static_cast<size_t>(MOST_KEPT))
    {
        this->kept_.erase(this->kept_.begin());
    }
}

void SavedMessages::save() const
{
    QJsonArray messages;
    for (const auto &entry : this->kept_)
    {
        messages.append(entry.toJson());
    }
    QSaveFile file(keptPath());
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    file.write(QJsonDocument(QJsonObject{{"messages", messages}})
                   .toJson(QJsonDocument::Compact));
    file.commit();
}

}  // namespace chatterino
