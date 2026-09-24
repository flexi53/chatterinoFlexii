// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/people/WatchedPeople.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "messages/Message.hpp"
#include "singletons/Settings.hpp"
#include "util/OpenOwnTab.hpp"

#include <algorithm>

namespace chatterino {

namespace {

/// The tab's channel: nothing to write in, and called what it is
class WatchedChannel : public Channel
{
public:
    WatchedChannel()
        : Channel(QStringLiteral("/leute"), Type::Misc)
    {
    }

    bool isWritable() const override
    {
        return false;
    }

    const QString &getLocalizedName() const override
    {
        static const QString name = QStringLiteral("Leute im Blick");
        return name;
    }
};

}  // namespace

WatchedPeople &WatchedPeople::instance()
{
    static WatchedPeople watched;
    return watched;
}

WatchedPeople::WatchedPeople()
    : channel_(std::make_shared<WatchedChannel>())
{
    this->channel_->addSystemMessage(
        "Hier steht, was die Leute schreiben, die du unter Notizen → Leute "
        "im Blick ausgewählt hast - aus jedem Kanal, den du offen hast.");
}

ChannelPtr WatchedPeople::channel() const
{
    return this->channel_;
}

void WatchedPeople::openTab()
{
    openOwnTab(instance().channel());
}

QStringList WatchedPeople::read(const QString &written)
{
    QStringList people;
    for (const auto &part :
         written.split(QRegularExpression(uR"([,\n\r;\s]+)"_qs),
                       Qt::SkipEmptyParts))
    {
        auto name = part.trimmed().toLower();
        while (name.startsWith(u'@') || name.startsWith(u'#'))
        {
            name.remove(0, 1);
        }
        if (!name.isEmpty() && !people.contains(name))
        {
            people.append(name);
        }
    }
    return people;
}

QString WatchedPeople::write(const QStringList &people)
{
    return people.join(u'\n');
}

QStringList WatchedPeople::people()
{
    return read(getSettings()->watchedPeople.getValue());
}

bool WatchedPeople::watches(const QString &login, const QStringList &people)
{
    if (login.isEmpty())
    {
        return false;
    }
    const auto name = login.toLower();
    return std::any_of(people.begin(), people.end(),
                       [&name](const auto &watched) {
                           return watched == name;
                       });
}

bool WatchedPeople::watches(const QString &login)
{
    return watches(login, people());
}

bool WatchedPeople::toggle(const QString &login)
{
    auto name = login.trimmed().toLower();
    while (name.startsWith(u'@'))
    {
        name.remove(0, 1);
    }
    if (name.isEmpty())
    {
        return false;
    }

    auto chosen = people();
    const bool watched = chosen.contains(name);
    if (watched)
    {
        chosen.removeAll(name);
    }
    else
    {
        chosen.append(name);
    }
    getSettings()->watchedPeople.setValue(write(chosen));
    return !watched;
}

void WatchedPeople::onMessage(const QString &channelName,
                              const MessagePtr &message)
{
    (void)channelName;  // the message knows where it came from

    if (message == nullptr || !getSettings()->watchedPeopleEnabled)
    {
        return;
    }
    if (!watches(message->loginName))
    {
        return;
    }

    // The message as it is, so it keeps its badges, emotes and colours - the
    // channel stands beside it, as in the mentions tab
    this->channel_->addMessage(message, MessageContext::Original);
}

}  // namespace chatterino
