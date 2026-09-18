// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/EmoteSpamDetector.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "messages/MessageElement.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"
#include "widgets/Window.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace {

using namespace chatterino;

/// Quiet for this long and they start with a clean slate
constexpr int RESET_SECONDS = 30 * 60;
/// The most messages one alert deletes
constexpr qsizetype MAX_DELETED = 30;

QString keyOf(const QString &channel, const QString &login)
{
    return channel + '\n' + login;
}

QHash<QString, bool> parseChannels(const QString &value)
{
    QHash<QString, bool> channels;
    for (const auto &channel : value.split(',', Qt::SkipEmptyParts))
    {
        channels.insert(channel, true);
    }
    return channels;
}

}  // namespace

namespace chatterino {

EmoteSpamDetector &EmoteSpamDetector::instance()
{
    static EmoteSpamDetector detector;
    return detector;
}

bool EmoteSpamDetector::isEnabled(const QString &channel) const
{
    return parseChannels(getSettings()->emoteAlertChannels.getValue())
        .contains(channel.toLower());
}

void EmoteSpamDetector::setEnabled(const QString &channel, bool enabled)
{
    auto channels = parseChannels(getSettings()->emoteAlertChannels.getValue());
    if (enabled)
    {
        channels.insert(channel.toLower(), true);
    }
    else
    {
        channels.remove(channel.toLower());
    }

    auto names = channels.keys();
    names.sort();
    getSettings()->emoteAlertChannels.setValue(names.join(','));
}

void EmoteSpamDetector::onMessage(const QString &channelName,
                                  const MessagePtr &message,
                                  const QString &badges)
{
    // Checked first and without anything else, so channels it is not switched
    // on for - nearly all of them - cost nothing
    const auto channel = channelName.toLower();
    if (!this->isEnabled(channel) || message->loginName.isEmpty())
    {
        return;
    }

    if (badges.contains("broadcaster/") || badges.contains("moderator/") ||
        badges.contains("vip/") || badges.contains("staff/") ||
        badges.contains("admin/"))
    {
        return;
    }

    const auto login = message->loginName.toLower();
    if (login ==
        getApp()->getAccounts()->twitch.getCurrent()->getUserName().toLower())
    {
        return;
    }

    // Without moderator rights here the button could not do anything
    auto *twitch = dynamic_cast<TwitchChannel *>(
        getApp()->getTwitch()->getChannelOrEmpty(channel).get());
    if (twitch == nullptr || !(twitch->isMod() || twitch->isBroadcaster()))
    {
        return;
    }

    const auto emotes = emoteCount(*message);
    if (emotes == 0)
    {
        return;
    }

    const auto *settings = getSettings();
    const auto threshold = std::max(1, settings->emoteAlertMinEmotes.getValue());
    const auto window =
        std::max(1, settings->emoteAlertWindowSeconds.getValue());
    const auto now = QDateTime::currentDateTime();

    // Keeps the map from growing for as long as the app runs
    if (this->users_.size() > 5000)
    {
        for (auto it = this->users_.begin(); it != this->users_.end();)
        {
            it = it->lastActivity.secsTo(now) > RESET_SECONDS
                     ? this->users_.erase(it)
                     : std::next(it);
        }
    }

    auto &state = this->users_[keyOf(channel, login)];
    if (state.lastActivity.isValid() &&
        state.lastActivity.secsTo(now) > RESET_SECONDS)
    {
        state = UserState{};
    }
    state.lastActivity = now;

    state.recent.append({now, emotes, message->id});
    while (!state.recent.isEmpty() &&
           state.recent.first().time.secsTo(now) > window)
    {
        state.recent.removeFirst();
    }

    int total = 0;
    for (const auto &counted : state.recent)
    {
        total += counted.emotes;
    }

    // A repeated message alert already open for them says more
    auto *open = ModAlertPopup::openFor(channel, login);
    if (open != nullptr && open->kind() != ModAlertPopup::Kind::EmoteSpam)
    {
        return;
    }

    // Until the first offer it takes the full amount within the window; after
    // it, what they send on counts towards the next step up
    if (!state.escalation.offered)
    {
        if (total < threshold)
        {
            return;
        }
        state.pendingIds.clear();
    }

    for (const auto &counted : state.recent)
    {
        if (!counted.id.isEmpty() && !state.pendingIds.contains(counted.id))
        {
            state.pendingIds.append(counted.id);
        }
    }
    // Deleting a whole afternoon of them one by one would take a while
    while (state.pendingIds.size() > MAX_DELETED)
    {
        state.pendingIds.removeFirst();
    }

    const auto offer = state.escalation.more(emotes, threshold);
    if (!offer && open == nullptr)
    {
        // Offered and let go - back once they have flooded as much again
        return;
    }

    const auto steps = EmoteSpamDetector::steps();
    const auto level = state.escalation.level;
    const auto action =
        steps[std::min<size_t>(static_cast<size_t>(level), steps.size() - 1)];

    auto *popup = ModAlertPopup::obtain(
        channel, login, &getApp()->getWindows()->getMainWindow());
    popup->setEmoteSpam(
        message->displayName.isEmpty() ? login : message->displayName, total,
        static_cast<int>(state.recent.size()), window, action,
        state.pendingIds, level, state.escalation.actions,
        static_cast<int>(steps.size()));
    if (offer)
    {
        popup->present();
    }
}

void EmoteSpamDetector::onAction(const QString &channelName,
                                 const QString &loginName)
{
    const auto channel = channelName.toLower();
    if (!this->isEnabled(channel))
    {
        return;
    }

    const auto login = loginName.toLower();
    auto it = this->users_.find(keyOf(channel, login));
    if (it != this->users_.end())
    {
        // One step per alert, however many messages the action took down
        if (it->escalation.offered)
        {
            it->escalation.actedOn();
        }
        // Dealt with - counting starts over from here
        it->recent.clear();
        it->pendingIds.clear();
        it->lastActivity = QDateTime::currentDateTime();
    }

    if (auto *open = ModAlertPopup::openFor(channel, login);
        open != nullptr && open->kind() == ModAlertPopup::Kind::EmoteSpam)
    {
        open->close();
    }
}

int EmoteSpamDetector::emoteCount(const Message &message)
{
    int emotes = 0;
    int words = 0;
    for (const auto &element : message.elements)
    {
        const auto flags = element->getFlags();

        if (dynamic_cast<LayeredEmoteElement *>(element.get()) != nullptr)
        {
            emotes++;
            continue;
        }

        if (dynamic_cast<EmoteElement *>(element.get()) != nullptr)
        {
            if (flags.hasAny({MessageElementFlag::BitsStatic,
                              MessageElementFlag::BitsAnimated}))
            {
                return 0;
            }
            if (flags.hasAny({MessageElementFlag::EmoteImage,
                              MessageElementFlag::EmojiImage}))
            {
                emotes++;
            }
            continue;
        }

        if (auto *text = dynamic_cast<TextElement *>(element.get()))
        {
            // What the chatter wrote, as opposed to their name, a caption and
            // the like
            const bool content =
                flags.has(MessageElementFlag::Text) ||
                dynamic_cast<LinkElement *>(text) != nullptr ||
                dynamic_cast<MentionElement *>(text) != nullptr;
            if (!content)
            {
                continue;
            }
            for (const auto &word : text->words())
            {
                if (std::any_of(word.begin(), word.end(), [](QChar ch) {
                        return ch.isLetterOrNumber();
                    }))
                {
                    words++;
                }
            }
        }
    }
    return emotes > 0 && emotes >= words ? emotes : 0;
}

std::vector<int> EmoteSpamDetector::steps()
{
    auto steps = parseSteps(getSettings()->emoteAlertSteps.getValue());
    if (steps.empty())
    {
        steps = {DELETE, DELETE, 30};
    }
    return steps;
}

std::vector<int> EmoteSpamDetector::parseSteps(const QString &text)
{
    static const QRegularExpression separators(QStringLiteral(R"([,;>\s]+)"));

    std::vector<int> steps;
    for (const auto &token : text.split(separators, Qt::SkipEmptyParts))
    {
        const auto lower = token.toLower();
        if (lower == QStringLiteral("delete") || lower == QStringLiteral("del") ||
            lower == QStringLiteral("löschen") ||
            lower == QStringLiteral("loeschen"))
        {
            steps.push_back(DELETE);
            continue;
        }

        const auto duration = RepeatSpamDetector::parseSteps(token);
        if (duration.size() != 1)
        {
            return {};
        }
        steps.push_back(duration.front());
    }
    return steps;
}

}  // namespace chatterino
