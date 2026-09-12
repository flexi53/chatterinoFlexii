// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/RepeatSpamDetector.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/dialogs/RepeatSpamPopup.hpp"
#include "widgets/Window.hpp"

namespace {

using namespace chatterino;

/// This many identical messages in a row earn the first timeout
constexpr int STREAK = 3;
/// A longer pause between two of them and it no longer counts as in a row
constexpr int MAX_GAP_SECONDS = 300;
/// How many of their messages the window shows
constexpr int HISTORY = 5;
/// Quiet for this long and they start with a clean slate
constexpr int RESET_SECONDS = 30 * 60;

QString keyOf(const QString &channel, const QString &login)
{
    return channel + '\n' + login;
}

/// The message as the moderator reads it. Leaves out the invisible character
/// Chatterino and 7TV append to get past Twitch's duplicate check, other
/// formatting characters, case and extra spaces - which are exactly the
/// differences a repeated message is made with.
QString normalise(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const auto codePoint : text.toUcs4())
    {
        const auto cp = static_cast<char32_t>(codePoint);
        if ((cp >= 0xE0000 && cp <= 0xE007F) ||
            QChar::category(cp) == QChar::Other_Format)
        {
            continue;
        }
        out.append(QString::fromUcs4(&cp, 1));
    }
    return out.simplified().toCaseFolded();
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

RepeatSpamDetector &RepeatSpamDetector::instance()
{
    static RepeatSpamDetector detector;
    return detector;
}

bool RepeatSpamDetector::isEnabled(const QString &channel) const
{
    return parseChannels(getSettings()->repeatAlertChannels.getValue())
        .contains(channel.toLower());
}

void RepeatSpamDetector::setEnabled(const QString &channel, bool enabled)
{
    auto channels = parseChannels(getSettings()->repeatAlertChannels.getValue());
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
    getSettings()->repeatAlertChannels.setValue(names.join(','));
}

void RepeatSpamDetector::onMessage(const QString &channelName,
                                   const QString &loginName,
                                   const QString &displayName,
                                   const QString &text, const QString &badges,
                                   const QDateTime &messageTime)
{
    // Checked first and without anything else, so channels it is not switched
    // on for - nearly all of them - cost nothing
    const auto channel = channelName.toLower();
    if (!this->isEnabled(channel) || loginName.isEmpty())
    {
        return;
    }

    if (badges.contains("broadcaster/") || badges.contains("moderator/") ||
        badges.contains("vip/") || badges.contains("staff/") ||
        badges.contains("admin/"))
    {
        return;
    }

    const auto login = loginName.toLower();
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

    const auto time =
        messageTime.isValid() ? messageTime : QDateTime::currentDateTime();
    const auto key = keyOf(channel, login);

    // Keeps the map from growing for as long as the app runs
    if (this->users_.size() > 5000)
    {
        for (auto it = this->users_.begin(); it != this->users_.end();)
        {
            it = it->lastActivity.secsTo(time) > RESET_SECONDS
                     ? this->users_.erase(it)
                     : std::next(it);
        }
    }

    auto &state = this->users_[key];
    if (state.lastActivity.isValid() &&
        state.lastActivity.secsTo(time) > RESET_SECONDS)
    {
        state = UserState{};
    }
    state.lastActivity = time;

    const auto normalised = normalise(text);
    if (normalised.isEmpty())
    {
        return;
    }

    state.recent.append({time, text, normalised});
    while (state.recent.size() > HISTORY)
    {
        state.recent.removeFirst();
    }

    // The same message, back to back, each close enough to the one before
    int streak = 0;
    for (auto i = state.recent.size() - 1; i >= 0; i--)
    {
        const auto &said = state.recent[i];
        if (said.normalised != normalised)
        {
            break;
        }
        if (i < state.recent.size() - 1 &&
            said.time.secsTo(state.recent[i + 1].time) > MAX_GAP_SECONDS)
        {
            break;
        }
        streak++;
    }

    const bool sameAsFlagged =
        !state.flaggedText.isEmpty() && normalised == state.flaggedText;

    if (sameAsFlagged && state.timeouts > 0)
    {
        // Already sat out a timeout for this one and sent it again
        this->showAlert(channel, login, displayName, state, 60, true);
        return;
    }

    if (streak >= STREAK)
    {
        if (!sameAsFlagged)
        {
            state.flaggedText = normalised;
            state.timeouts = 0;
        }
        this->showAlert(channel, login, displayName, state, 30, false);
    }
}

void RepeatSpamDetector::onTimeout(const QString &channelName,
                                   const QString &loginName)
{
    const auto channel = channelName.toLower();
    if (!this->isEnabled(channel))
    {
        return;
    }

    const auto key = keyOf(channel, loginName.toLower());

    auto it = this->users_.find(key);
    if (it != this->users_.end() && !it->flaggedText.isEmpty())
    {
        it->timeouts++;
        // The messages are gone with the timeout; what counts from here on is
        // whether the flagged one comes back
        it->recent.clear();
    }

    // Someone has already dealt with it
    if (auto popup = this->popups_.value(key); !popup.isNull())
    {
        popup->close();
    }
}

void RepeatSpamDetector::showAlert(const QString &channel, const QString &login,
                                   const QString &displayName,
                                   const UserState &state, int seconds,
                                   bool again)
{
    const auto key = keyOf(channel, login);

    auto popup = this->popups_.value(key);
    if (popup.isNull())
    {
        popup = new RepeatSpamPopup(
            channel, login, &getApp()->getWindows()->getMainWindow());
        this->popups_.insert(key, popup);
    }

    QList<QPair<QDateTime, QString>> messages;
    for (const auto &said : state.recent)
    {
        messages.append({said.time, said.text});
    }

    popup->setCase(displayName.isEmpty() ? login : displayName, messages,
                   seconds, again);
    popup->show();
    popup->raise();
}

}  // namespace chatterino
