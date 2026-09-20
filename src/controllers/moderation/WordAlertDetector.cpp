// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/WordAlertDetector.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/FormatTime.hpp"
#include "util/SpellingVariants.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"
#include "widgets/Window.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace {

using namespace chatterino;

/// Quiet for this long and they start with a clean slate
constexpr int RESET_SECONDS = 30 * 60;
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

WordAlertDetector &WordAlertDetector::instance()
{
    static WordAlertDetector detector;
    return detector;
}

bool WordAlertDetector::isEnabled(const QString &channel) const
{
    return parseChannels(getSettings()->wordAlertChannels.getValue())
        .contains(channel.toLower());
}

void WordAlertDetector::setEnabled(const QString &channel, bool enabled)
{
    auto channels = parseChannels(getSettings()->wordAlertChannels.getValue());
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
    getSettings()->wordAlertChannels.setValue(names.join(','));
}

std::vector<WordAlertDetector::Watched> WordAlertDetector::parseWords(
    const QString &text, bool variants, bool wholeWord)
{
    spelling::Options options{
        .leet = variants,
        .lookalikes = variants,
        .stretched = variants,
        .separated = variants,
        .wholeWord = wholeWord,
    };

    std::vector<Watched> words;
    for (const auto &line : text.split('\n'))
    {
        auto word = line.trimmed();
        // A line starting with # is a note to self, not a word to watch for
        if (word.isEmpty() || word.startsWith('#'))
        {
            continue;
        }

        // What this word alone offers stands behind an equals sign
        std::vector<int> ownSteps;
        const auto equals = word.indexOf('=');
        if (equals > 0)
        {
            ownSteps = parseSteps(word.mid(equals + 1));
            word = word.left(equals).trimmed();
            if (word.isEmpty())
            {
                continue;
            }
        }

        const auto pattern = spelling::pattern(word, options);
        if (pattern.isEmpty())
        {
            continue;
        }

        const auto compiled = spelling::compile(pattern);
        if (!compiled.isValid())
        {
            continue;
        }
        words.push_back({
            .word = word,
            .pattern = compiled,
            .steps = ownSteps,
        });
    }
    return words;
}

QString WordAlertDetector::writeWords(const std::vector<Watched> &list)
{
    QStringList lines;
    for (const auto &watched : list)
    {
        if (watched.word.trimmed().isEmpty())
        {
            continue;
        }
        if (watched.steps.empty())
        {
            lines.append(watched.word);
            continue;
        }

        QStringList steps;
        for (const auto step : watched.steps)
        {
            if (step == DELETE)
            {
                steps.append(QStringLiteral("löschen"));
            }
            else if (step == 0)
            {
                steps.append(QStringLiteral("bann"));
            }
            else
            {
                steps.append(formatTime(step));
            }
        }
        lines.append(watched.word + QStringLiteral(" = ") +
                     steps.join(QStringLiteral(", ")));
    }
    return lines.join('\n');
}

const std::vector<int> &WordAlertDetector::palette()
{
    // What a word can be given on its own, shortest first - deleting
    // before the timeouts, a ban last
    static const std::vector<int> buttons{
        DELETE, 60, 300, 600, 1800, 3600, 86400, 604800, 0,
    };
    return buttons;
}

int WordAlertDetector::deleteLimit(int setting)
{
    if (setting <= 0)
    {
        return MOST_DELETED;
    }
    return std::min(setting, MOST_DELETED);
}

const std::vector<WordAlertDetector::Watched> &WordAlertDetector::words()
{
    // Building them for every message would be a waste; the list changes
    // only when someone edits it
    static QString builtFrom;
    static bool builtVariants = false;
    static bool builtWholeWord = false;
    static std::vector<Watched> built;

    const auto *settings = getSettings();
    const auto text = settings->wordAlertWords.getValue();
    const auto variants = settings->wordAlertVariants.getValue();
    const auto wholeWord = settings->wordAlertWholeWord.getValue();

    if (text != builtFrom || variants != builtVariants ||
        wholeWord != builtWholeWord || (built.empty() && !text.isEmpty()))
    {
        built = parseWords(text, variants, wholeWord);
        builtFrom = text;
        builtVariants = variants;
        builtWholeWord = wholeWord;
    }
    return built;
}

std::optional<WordAlertDetector::Match> WordAlertDetector::find(
    const QString &text, const std::vector<Watched> &list)
{
    for (const auto &watched : list)
    {
        const auto match = watched.pattern.match(text);
        if (match.hasMatch())
        {
            return Match{
                .word = watched.word,
                .asWritten = match.captured().trimmed(),
                .steps = watched.steps,
            };
        }
    }
    return std::nullopt;
}

void WordAlertDetector::onMessage(const QString &channelName,
                                  const QString &loginName,
                                  const QString &displayName,
                                  const QString &text, const QString &badges,
                                  const QString &messageId)
{
    // Checked first and without anything else, so channels it is not
    // switched on for - nearly all of them - cost nothing
    const auto channel = channelName.toLower();
    if (!this->isEnabled(channel) || loginName.isEmpty() || text.isEmpty())
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

    const auto match = find(text, words());
    if (!match)
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

    // An alert of another kind already open for them says more
    auto *open = ModAlertPopup::openFor(channel, login);
    if (open != nullptr && open->kind() != ModAlertPopup::Kind::Word)
    {
        return;
    }

    if (!messageId.isEmpty() && !state.pendingIds.contains(messageId))
    {
        state.pendingIds.append(messageId);
    }
    const auto keep =
        deleteLimit(getSettings()->wordAlertDeleteCount.getValue());
    while (state.pendingIds.size() > keep)
    {
        state.pendingIds.removeFirst();
    }

    // Every word said is one more; the step goes up once they say another
    // after an offer nobody acted on
    const auto offer = state.escalation.more(1, 1);
    if (!offer && open == nullptr)
    {
        return;
    }

    // What this word alone offers, or the steps set for all of them
    const auto steps =
        match->steps.empty() ? WordAlertDetector::steps() : match->steps;
    const auto level = state.escalation.level;
    const auto action =
        steps[std::min<size_t>(static_cast<size_t>(level), steps.size() - 1)];

    auto *popup = ModAlertPopup::obtain(
        channel, login, &getApp()->getWindows()->getMainWindow());
    popup->setWordAlert(displayName.isEmpty() ? login : displayName,
                        match->word, match->asWritten, action,
                        state.pendingIds, level, state.escalation.actions,
                        static_cast<int>(steps.size()));
    if (offer)
    {
        popup->present();
    }
}

void WordAlertDetector::onAction(const QString &channelName,
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
        if (it->escalation.offered)
        {
            it->escalation.actedOn();
        }
        it->pendingIds.clear();
        it->lastActivity = QDateTime::currentDateTime();
    }

    if (auto *open = ModAlertPopup::openFor(channel, login);
        open != nullptr && open->kind() == ModAlertPopup::Kind::Word)
    {
        open->close();
    }
}

std::vector<int> WordAlertDetector::steps()
{
    auto steps = parseSteps(getSettings()->wordAlertSteps.getValue());
    if (steps.empty())
    {
        steps = {300, 600, 1800, 3600, 86400};
    }
    return steps;
}

std::vector<int> WordAlertDetector::parseSteps(const QString &text)
{
    static const QRegularExpression separators(QStringLiteral(R"([,;>\s]+)"));

    std::vector<int> steps;
    for (const auto &token : text.split(separators, Qt::SkipEmptyParts))
    {
        const auto lower = token.toLower();
        if (lower == QStringLiteral("delete") ||
            lower == QStringLiteral("del") ||
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
