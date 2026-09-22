// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/badgealerts/BadgeAlerts.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/sound/ISoundController.hpp"
#include "messages/Image.hpp"
#include "messages/Link.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/CombinePath.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QSaveFile>
#include <QUrl>

#include <algorithm>

namespace chatterino {

namespace {

constexpr int CHECK_MINUTES = 15;
/// What is remembered at the most, so the file does not grow for ever
constexpr int MOST_REMEMBERED = 3000;

/// The tab's channel: nothing to write in, and called what it is
class BadgeChannel : public Channel
{
public:
    BadgeChannel()
        : Channel(QStringLiteral("/badges"), Type::Misc)
    {
    }

    bool isWritable() const override
    {
        return false;
    }

    const QString &getLocalizedName() const override
    {
        static const QString name = QStringLiteral("Neue Badges");
        return name;
    }
};

QString statePath()
{
    return combinePath(getApp()->getPaths().settingsDirectory,
                       QStringLiteral("badge-alerts.json"));
}

QString when(const QDateTime &time)
{
    static const QLocale german(QLocale::German);
    return german.toString(time.toLocalTime(),
                           QStringLiteral("ddd, dd.MM., HH:mm"));
}

}  // namespace

BadgeAlerts &BadgeAlerts::instance()
{
    static BadgeAlerts alerts;
    return alerts;
}

BadgeAlerts::BadgeAlerts()
    : channel_(std::make_shared<BadgeChannel>())
{
    this->load();

    this->timer_.setInterval(CHECK_MINUTES * 60 * 1000);
    QObject::connect(&this->timer_, &QTimer::timeout, this, [this] {
        this->check();
    });

    getSettings()->badgeAlertsEnabled.connect(
        [this](const bool enabled, auto) {
            if (enabled)
            {
                this->timer_.start();
                // A moment after starting, so the login is there to ask with
                QTimer::singleShot(8000, this, [this] {
                    this->check();
                });
            }
            else
            {
                this->timer_.stop();
            }
        },
        this->connections_);
}

ChannelPtr BadgeAlerts::channel() const
{
    return this->channel_;
}

void BadgeAlerts::checkNow()
{
    this->check();
}

void BadgeAlerts::openTab()
{
    const auto channel = instance().channel();
    auto &notebook = getApp()->getWindows()->getMainWindow().getNotebook();
    for (int i = 0; i < notebook.getPageCount(); i++)
    {
        auto *page = dynamic_cast<SplitContainer *>(notebook.getPageAt(i));
        if (page == nullptr)
        {
            continue;
        }
        for (auto *split : page->getSplits())
        {
            if (split->getChannel() == channel)
            {
                notebook.select(page);
                return;
            }
        }
    }
    // Its own channel - no dialog asking for a Twitch one
    notebook.addPage(true)->appendNewSplit(false)->setChannel(channel);
}

QString BadgeAlerts::Event::key() const
{
    const auto &id =
        this->badge.id.isEmpty() ? this->badge.setId : this->badge.id;
    switch (this->kind)
    {
        case Kind::Available:
            return QStringLiteral("available:") + id;
        case Kind::Upcoming:
            return QStringLiteral("upcoming:") + id;
        case Kind::Ending:
            return QStringLiteral("ending:") + id;
        case Kind::NewOnTwitch:
            return QStringLiteral("twitch:") + id;
    }
    return id;
}

std::vector<BadgeAlerts::Event> BadgeAlerts::events(
    const std::vector<badgebase::Badge> &claimable,
    const std::vector<badgebase::Badge> &upcoming,
    const std::optional<QSet<QString>> &missing, const QSet<QString> &reported,
    const QDateTime &now, const Options &options)
{
    std::vector<Event> found;
    const auto add = [&](Kind kind, const badgebase::Badge &badge) {
        const Event event{kind, badge};
        if (!reported.contains(event.key()))
        {
            found.push_back(event);
        }
    };

    for (const auto &badge : claimable)
    {
        // What they have already is nothing to say - when that is known
        if (options.onlyMissing && missing && !missing->contains(badge.id))
        {
            continue;
        }
        if (options.available)
        {
            add(Kind::Available, badge);
        }
        if (options.ending && badge.end.isValid() && now < badge.end &&
            now.secsTo(badge.end) <= ENDING_SECONDS)
        {
            add(Kind::Ending, badge);
        }
    }

    if (options.upcoming)
    {
        for (const auto &badge : upcoming)
        {
            add(Kind::Upcoming, badge);
        }
    }
    return found;
}

MessagePtr BadgeAlerts::messageFor(const Event &event, const QString &picture,
                                   bool twitchPicture)
{
    const auto &badge = event.badge;
    QString label;
    switch (event.kind)
    {
        case Kind::Available:
            label = QStringLiteral("Jetzt verfügbar:");
            break;
        case Kind::Upcoming:
            label = QStringLiteral("Kommt bald:");
            break;
        case Kind::Ending:
            label = QStringLiteral("Endet bald:");
            break;
        case Kind::NewOnTwitch:
            label = QStringLiteral("Neu bei Twitch:");
            break;
    }

    QStringList details;
    if (event.kind == Kind::Upcoming && badge.start.isValid())
    {
        details.append(QStringLiteral("ab ") + when(badge.start));
    }
    if (badge.end.isValid() && event.kind != Kind::NewOnTwitch)
    {
        details.append(QStringLiteral("bis ") + when(badge.end));
    }
    if (event.kind != Kind::NewOnTwitch)
    {
        details.append(badge.paid ? QStringLiteral("kostenpflichtig")
                                  : QStringLiteral("kostenlos"));
    }
    if (badge.holders >= 0)
    {
        details.append(QLocale(QLocale::German).toString(badge.holders) +
                       QStringLiteral(" haben es"));
    }

    MessageBuilder builder;
    builder->flags.set(MessageFlag::DoNotLog);
    builder.emplace<TimestampElement>(QTime::currentTime());
    if (!picture.isEmpty())
    {
        // Twitch's picture is twice the size it is shown at, BadgeBase's
        // four times
        builder
            .emplace<ImageElement>(
                Image::fromUrl(Url{picture}, twitchPicture ? 0.5 : 0.25),
                MessageElementFlag::AlwaysShow)
            ->setTooltip(badge.title);
    }
    builder.emplace<TextElement>(label, MessageElementFlag::Text,
                                 MessageColor::System);
    builder.emplace<TextElement>(badge.title, MessageElementFlag::Text,
                                 MessageColor::Text, FontStyle::ChatMediumBold);
    if (!details.isEmpty())
    {
        builder.emplace<TextElement>(
            QStringLiteral("- ") + details.join(QStringLiteral(" · ")),
            MessageElementFlag::Text, MessageColor::System);
    }
    if (!badge.url.isEmpty())
    {
        builder
            .emplace<TextElement>(QStringLiteral("mehr"),
                                  MessageElementFlag::Text, MessageColor::Link)
            ->setLink(Link(Link::Url, badge.url));
    }

    auto text = label + ' ' + badge.title;
    if (!details.isEmpty())
    {
        text += QStringLiteral(" - ") + details.join(QStringLiteral(" · "));
    }
    if (!badge.url.isEmpty())
    {
        text += ' ' + badge.url;
    }
    builder->messageText = text;
    builder->searchText = text;
    return builder.release();
}

void BadgeAlerts::check()
{
    if (!getSettings()->badgeAlertsEnabled || this->asking_)
    {
        return;
    }
    this->asking_ = true;
    this->checkTwitch();
}

void BadgeAlerts::checkTwitch()
{
    const auto continueWithBadgeBase = [this] {
        badgebase::loadKey(this, [this](const QString &key) {
            if (key.isEmpty())
            {
                this->asking_ = false;
                if (!this->noKeySaid_)
                {
                    this->noKeySaid_ = true;
                    this->note(QStringLiteral(
                        "Ohne BadgeBase-Schlüssel meldet dieser Tab nur neue "
                        "Badges von Twitch - ohne Termine und ohne „fehlt "
                        "dir noch“. Den Schlüssel trägst du unter "
                        "Einstellungen → Badges ein."));
                }
                return;
            }
            this->checkBadgeBase(key);
        });
    };

    getHelix()->getGlobalBadges(
        [this, continueWithBadgeBase](const HelixGlobalBadges &badges) {
            QSet<QString> now;
            std::vector<Event> fresh;
            for (const auto &set : badges.badgeSets)
            {
                for (const auto &version : set.versions)
                {
                    const auto id = set.setID + '/' + version.id;
                    now.insert(id);
                    if (!this->twitchPictures_.contains(set.setID))
                    {
                        this->twitchPictures_.insert(set.setID,
                                                     version.imageURL2x.string);
                    }
                    if (!this->twitchKnown_.isEmpty() &&
                        !this->twitchKnown_.contains(id))
                    {
                        badgebase::Badge badge;
                        badge.id = id;
                        badge.setId = set.setID;
                        badge.title = version.title;
                        badge.url = version.clickURL.string;
                        fresh.push_back({Kind::NewOnTwitch, badge});
                    }
                }
            }
            // The first time is only to know what there is
            this->twitchKnown_ = now;
            if (getSettings()->badgeAlertsTwitch)
            {
                this->say(fresh);
            }
            this->save();
            continueWithBadgeBase();
        },
        [continueWithBadgeBase](auto, const QString &) {
            continueWithBadgeBase();
        });
}

void BadgeAlerts::checkBadgeBase(const QString &key)
{
    auto claimable = std::make_shared<std::vector<badgebase::Badge>>();
    auto upcoming = std::make_shared<std::vector<badgebase::Badge>>();

    const auto failed = [this](const QString &why) {
        this->asking_ = false;
        if (why != this->lastProblem_)
        {
            this->lastProblem_ = why;
            this->note(why);
        }
    };

    const auto finish = [this, claimable,
                         upcoming](std::optional<QSet<QString>> missing) {
        auto *s = getSettings();
        const Options options{
            .available = s->badgeAlertsAvailable,
            .upcoming = s->badgeAlertsUpcoming,
            .ending = s->badgeAlertsEnding,
            .onlyMissing = s->badgeAlertsOnlyMissing,
        };
        auto found = events(*claimable, *upcoming, missing, this->reported_,
                            QDateTime::currentDateTimeUtc(), options);

        // Once each start, what can be had now - those need no message of
        // their own then
        if (!this->summarized_)
        {
            this->summarized_ = true;
            QStringList titles;
            for (const auto &badge : *claimable)
            {
                if (!missing || missing->contains(badge.id))
                {
                    titles.append(badge.title);
                }
            }
            if (claimable->empty())
            {
                this->note(
                    QStringLiteral("Gerade gibt es kein Badge zu holen."));
            }
            else if (missing && titles.isEmpty())
            {
                this->note(
                    QStringLiteral("Gerade %1 Badges zu holen - du hast sie "
                                   "alle.")
                        .arg(claimable->size()));
            }
            else
            {
                this->note(QStringLiteral("Gerade %1 Badges zu holen%2: %3")
                               .arg(claimable->size())
                               .arg(missing ? QStringLiteral(", dir fehlen %1")
                                                  .arg(titles.size())
                                            : QString())
                               .arg(titles.join(QStringLiteral(", "))));
            }
            found.erase(std::remove_if(found.begin(), found.end(),
                                       [this](const Event &event) {
                                           if (event.kind != Kind::Available)
                                           {
                                               return false;
                                           }
                                           this->reported_.insert(event.key());
                                           return true;
                                       }),
                        found.end());
        }

        this->say(found);
        this->save();
        this->asking_ = false;
        this->lastProblem_.clear();
    };

    badgebase::get(
        key, QStringLiteral("/badges?status=claimable"), this,
        [this, key, claimable, upcoming, finish,
         failed](const QJsonObject &answer) {
            *claimable = badgebase::badgesIn(answer);
            badgebase::get(
                key, QStringLiteral("/badges?status=upcoming"), this,
                [this, key, upcoming, finish](const QJsonObject &answer) {
                    *upcoming = badgebase::badgesIn(answer);

                    // What they still lack - only with a login to ask for
                    auto account = getApp()->getAccounts()->twitch.getCurrent();
                    if (account == nullptr || account->isAnon())
                    {
                        finish(std::nullopt);
                        return;
                    }
                    badgebase::get(
                        key,
                        QStringLiteral("/user/%1/missing")
                            .arg(QString::fromUtf8(QUrl::toPercentEncoding(
                                account->getUserName().toLower()))),
                        this,
                        [finish](const QJsonObject &answer) {
                            QSet<QString> ids;
                            for (const auto &badge :
                                 badgebase::badgesIn(answer))
                            {
                                ids.insert(badge.id);
                            }
                            finish(ids);
                        },
                        [finish](const QString &) {
                            finish(std::nullopt);
                        });
                },
                failed);
        },
        failed);
}

QString BadgeAlerts::pictureFor(const badgebase::Badge &badge,
                                bool &twitch) const
{
    if (auto it = this->twitchPictures_.constFind(badge.setId);
        it != this->twitchPictures_.constEnd() && !it->isEmpty())
    {
        twitch = true;
        return *it;
    }
    twitch = false;
    return badge.image;
}

void BadgeAlerts::say(const std::vector<Event> &events)
{
    for (const auto &event : events)
    {
        bool twitch = false;
        const auto picture = this->pictureFor(event.badge, twitch);
        this->channel_->addMessage(messageFor(event, picture, twitch),
                                   MessageContext::Original);
        this->reported_.insert(event.key());
    }
    if (!events.empty() && getSettings()->badgeAlertsSound)
    {
        getApp()->getSound()->play(
            QUrl(QStringLiteral("qrc:/sounds/ping2.wav")));
    }
}

void BadgeAlerts::note(const QString &text)
{
    MessageBuilder builder;
    builder->flags.set(MessageFlag::System);
    builder->flags.set(MessageFlag::DoNotLog);
    builder.emplace<TimestampElement>(QTime::currentTime());
    builder.emplace<TextElement>(text, MessageElementFlag::Text,
                                 MessageColor::System);
    builder->messageText = text;
    builder->searchText = text;
    this->channel_->addMessage(builder.release(), MessageContext::Original);
}

void BadgeAlerts::load()
{
    QFile file(statePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    const auto state = QJsonDocument::fromJson(file.readAll()).object();
    for (const auto &key : state.value("reported").toArray())
    {
        this->reported_.insert(key.toString());
    }
    for (const auto &id : state.value("twitch").toArray())
    {
        this->twitchKnown_.insert(id.toString());
    }
}

void BadgeAlerts::save() const
{
    QJsonArray reported;
    auto keys = this->reported_.values();
    keys.sort();
    // The newest are not known by order - what is kept is enough for years
    // of badges, so cutting is only a guard
    while (keys.size() > MOST_REMEMBERED)
    {
        keys.removeFirst();
    }
    for (const auto &key : keys)
    {
        reported.append(key);
    }
    QJsonArray twitch;
    for (const auto &id : this->twitchKnown_)
    {
        twitch.append(id);
    }

    QSaveFile file(statePath());
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    file.write(QJsonDocument(QJsonObject{
                                 {"reported", reported},
                                 {"twitch", twitch},
                             })
                   .toJson(QJsonDocument::Compact));
    file.commit();
}

}  // namespace chatterino
