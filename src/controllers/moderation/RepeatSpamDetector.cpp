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

#include <algorithm>
#include <QRegularExpression>

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

/// The step for someone who has served @a timeoutsServed timeouts. Past the
/// last step it stays at the last.
int stepFor(int timeoutsServed)
{
    const auto steps = RepeatSpamDetector::steps();
    return steps[std::min<size_t>(static_cast<size_t>(timeoutsServed),
                                  steps.size() - 1)];
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
        this->showAlert(channel, login, displayName, state,
                        stepFor(state.timeouts), state.timeouts);
        return;
    }

    if (streak >= STREAK)
    {
        if (!sameAsFlagged)
        {
            state.flaggedText = normalised;
            state.timeouts = 0;
        }
        this->showAlert(channel, login, displayName, state,
                        stepFor(state.timeouts), state.timeouts);
    }
}

void RepeatSpamDetector::onTimeout(const QString &channelName,
                                   const QString &loginName, int seconds)
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
        // The quiet time before a clean slate starts once the timeout is
        // over - otherwise a long one would wipe the very escalation it was
        // part of
        it->lastActivity = QDateTime::currentDateTime().addSecs(seconds);
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
                                   int timeoutsServed)
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
                   seconds, timeoutsServed);
    popup->show();
    popup->raise();
}

std::vector<int> RepeatSpamDetector::steps()
{
    auto steps = parseSteps(getSettings()->repeatAlertSteps.getValue());
    if (steps.empty())
    {
        // The rule this started out with, rather than no alert at all
        steps = {30, 60};
    }
    return steps;
}

std::vector<int> RepeatSpamDetector::parseSteps(const QString &text)
{
    static const QRegularExpression separators(QStringLiteral(R"([,;>\s]+)"));
    static const QRegularExpression part(
        QStringLiteral(R"((\d+)([smhdw]?))"),
        QRegularExpression::CaseInsensitiveOption);

    // Twitch does not time anyone out for longer than two weeks
    constexpr qint64 maxSeconds = 14 * 24 * 60 * 60;

    std::vector<int> steps;
    for (const auto &token : text.split(separators, Qt::SkipEmptyParts))
    {
        qint64 seconds = 0;
        qsizetype consumed = 0;

        auto it = part.globalMatch(token);
        while (it.hasNext())
        {
            const auto match = it.next();
            if (match.capturedStart() != consumed)
            {
                return {};
            }
            consumed = match.capturedEnd();

            const qint64 value = match.captured(1).toLongLong();
            const auto unit = match.captured(2).toLower();
            switch (unit.isEmpty() ? 's' : unit.at(0).toLatin1())
            {
                case 'w':
                    seconds += value * 7 * 24 * 60 * 60;
                    break;
                case 'd':
                    seconds += value * 24 * 60 * 60;
                    break;
                case 'h':
                    seconds += value * 60 * 60;
                    break;
                case 'm':
                    seconds += value * 60;
                    break;
                default:
                    seconds += value;
                    break;
            }
        }

        if (consumed != token.size() || seconds <= 0)
        {
            return {};
        }
        steps.push_back(static_cast<int>(std::min(seconds, maxSeconds)));
    }
    return steps;
}

}  // namespace chatterino
