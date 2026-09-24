// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/people/WatchedPeople.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/filters/lang/Filter.hpp"
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
    this->rebuildFilter();
    // Typed on the settings page, it counts from the next message on
    getSettings()->watchedPeopleFilter.connect(
        [this](const auto &, auto) {
            this->rebuildFilter();
        },
        false);

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

QString WatchedPeople::expressionFor(const QStringList &people)
{
    QStringList parts;
    for (const auto &name : people)
    {
        parts.append(QStringLiteral(R"((author.name == "%1"))").arg(name));
    }
    return parts.join(QStringLiteral(" || "));
}

QString WatchedPeople::problemWith(const QString &expression)
{
    const auto written = expression.trimmed();
    if (written.isEmpty())
    {
        return {};
    }

    auto result = filters::Filter::fromString(written);
    if (std::holds_alternative<filters::FilterError>(result))
    {
        return std::get<filters::FilterError>(result).message;
    }
    if (std::get<filters::Filter>(result).returnType() != filters::Type::Bool)
    {
        return QStringLiteral(
            "Der Filter muss ja oder nein ergeben, etwa "
            R"(author.name == "name".)");
    }
    return {};
}

void WatchedPeople::rebuildFilter()
{
    this->filter_.reset();

    const auto written = getSettings()->watchedPeopleFilter.getValue().trimmed();
    if (written.isEmpty())
    {
        return;
    }

    auto result = filters::Filter::fromString(written);
    if (!std::holds_alternative<filters::Filter>(result))
    {
        return;
    }
    auto filter =
        std::make_unique<filters::Filter>(std::move(std::get<filters::Filter>(result)));
    if (filter->returnType() != filters::Type::Bool)
    {
        return;
    }
    this->filter_ = std::move(filter);
}

void WatchedPeople::onMessage(Channel *channel, const MessagePtr &message)
{
    if (message == nullptr || !getSettings()->watchedPeopleEnabled)
    {
        return;
    }

    if (this->filter_)
    {
        // A filter of your own decides on its own - the list stays where it
        // is, for when the filter is taken out again
        const auto context = filters::buildContextMap(message, channel);
        if (!this->filter_->execute(context).toBool())
        {
            return;
        }
    }
    else if (!watches(message->loginName))
    {
        return;
    }

    // The message as it is, so it keeps its badges, emotes and colours - the
    // channel stands beside it, as in the mentions tab
    this->channel_->addMessage(message, MessageContext::Original);
}

}  // namespace chatterino
