// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/Window.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"
#include "singletons/WindowManager.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/moderation/ModerationAssistant.hpp"

#include "Application.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/FormatTime.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>

namespace {

using namespace chatterino;

/// Oldest cases fall away past this, so matching stays quick
constexpr int MAX_CASES = 2000;
/// How many of a user's messages before an action make up a case
constexpr int MESSAGES_PER_CASE = 3;
/// Messages older than this before the action say nothing about it
constexpr int CONTEXT_SECONDS = 600;
/// The same action seen twice - live and again in the logs - lands this
/// close together
constexpr int DUPLICATE_SECONDS = 5;
/// A burst of lookalike messages - a raid, say - should not bury the screen
/// in windows
constexpr int MAX_SUGGESTION_WINDOWS = 3;

QString storeDirectory()
{
    auto *app = tryGetApp();
    if (app == nullptr)
    {
        return {};
    }

    QDir misc(app->getPaths().miscDirectory);
    misc.mkpath(QStringLiteral("moderation-assistant"));
    return misc.absoluteFilePath(QStringLiteral("moderation-assistant"));
}

/// Mirrors the layout the chat logger writes to
QString logDirectory(const QString &channel)
{
    auto *app = tryGetApp();
    if (app == nullptr)
    {
        return {};
    }

    auto base = getSettings()->logPath.getValue();
    if (base.isEmpty())
    {
        base = app->getPaths().messageLogDirectory;
    }

    return QStringList{base, QStringLiteral("Twitch"),
                       QStringLiteral("Channels"), channel}
        .join(QDir::separator());
}

QHash<QString, ModAssistMode> parseModes(const QString &value)
{
    QHash<QString, ModAssistMode> modes;
    for (const auto &entry : value.split(',', Qt::SkipEmptyParts))
    {
        const auto parts = entry.split(':');
        if (parts.size() != 2)
        {
            continue;
        }

        if (parts[1] == QStringLiteral("learn"))
        {
            modes.insert(parts[0], ModAssistMode::Learn);
        }
        else if (parts[1] == QStringLiteral("suggest"))
        {
            modes.insert(parts[0], ModAssistMode::Suggest);
        }
    }
    return modes;
}

QString formatModes(const QHash<QString, ModAssistMode> &modes)
{
    QStringList entries;
    for (auto it = modes.begin(); it != modes.end(); ++it)
    {
        entries.append(it.key() + ':' +
                       (it.value() == ModAssistMode::Suggest
                            ? QStringLiteral("suggest")
                            : QStringLiteral("learn")));
    }
    entries.sort();
    return entries.join(',');
}

/// The words of a message, in a form where small differences - case, links,
/// stretched letters, punctuation - do not keep two messages apart
QSet<QString> wordsOf(const QString &text)
{
    static const QRegularExpression url(
        QStringLiteral(R"(https?://\S+|www\.\S+)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression stretched(QStringLiteral(R"((.)\1{2,})"));
    static const QRegularExpression separators(
        QStringLiteral(R"([^\p{L}\p{N}]+)"));

    auto normalised = text.toLower();
    normalised.replace(url, QStringLiteral(" linkplaceholder "));
    normalised.replace(stretched, QStringLiteral("\\1\\1"));

    QSet<QString> words;
    for (const auto &word : normalised.split(separators, Qt::SkipEmptyParts))
    {
        if (word.size() >= 2)
        {
            words.insert(word);
        }
    }
    return words;
}

double similarity(const QSet<QString> &a, const QSet<QString> &b)
{
    if (a.isEmpty() || b.isEmpty())
    {
        return 0;
    }

    const auto &smaller = a.size() < b.size() ? a : b;
    const auto &larger = a.size() < b.size() ? b : a;

    qsizetype shared = 0;
    for (const auto &word : smaller)
    {
        if (larger.contains(word))
        {
            shared++;
        }
    }

    return double(shared) / double(a.size() + b.size() - shared);
}

/// "1h 30m" -> 5400
int parseDuration(const QString &text)
{
    static const QRegularExpression part(QStringLiteral(R"((\d+)\s*([dhms]))"));

    int seconds = 0;
    auto it = part.globalMatch(text);
    while (it.hasNext())
    {
        const auto match = it.next();
        const int value = match.captured(1).toInt();
        switch (match.captured(2).at(0).toLatin1())
        {
            case 'd':
                seconds += value * 86400;
                break;
            case 'h':
                seconds += value * 3600;
                break;
            case 'm':
                seconds += value * 60;
                break;
            default:
                seconds += value;
                break;
        }
    }
    return seconds;
}

QString describe(int seconds)
{
    return seconds > 0 ? formatTime(seconds) : QStringLiteral("ban");
}

}  // namespace

namespace chatterino {

ModerationAssistant &ModerationAssistant::instance()
{
    static ModerationAssistant assistant;
    return assistant;
}

ModAssistMode ModerationAssistant::mode(const QString &channel) const
{
    return parseModes(getSettings()->modAssistModes.getValue())
        .value(channel.toLower(), ModAssistMode::Off);
}

void ModerationAssistant::setMode(const QString &channel, ModAssistMode mode)
{
    auto modes = parseModes(getSettings()->modAssistModes.getValue());
    if (mode == ModAssistMode::Off)
    {
        modes.remove(channel.toLower());
    }
    else
    {
        modes.insert(channel.toLower(), mode);
    }
    getSettings()->modAssistModes.setValue(formatModes(modes));
}

void ModerationAssistant::record(const QString &channelName, ModCase modCase)
{
    const auto channel = channelName.toLower();
    if (this->mode(channel) == ModAssistMode::Off || modCase.messages.isEmpty())
    {
        return;
    }

    std::lock_guard lock(this->mutex_);
    auto &channelData = this->data(channel);
    if (add(channelData, std::move(modCase)))
    {
        this->save(channel, channelData);
    }
}

std::vector<ModCase> ModerationAssistant::cases(const QString &channel) const
{
    std::lock_guard lock(this->mutex_);
    const auto &channelData = this->data(channel.toLower());

    std::vector<ModCase> result;
    result.reserve(channelData.entries.size());
    for (auto it = channelData.entries.rbegin();
         it != channelData.entries.rend(); ++it)
    {
        result.push_back(it->modCase);
    }
    return result;
}

void ModerationAssistant::removeCase(const QString &channelName,
                                     const QDateTime &time, const QString &user)
{
    const auto channel = channelName.toLower();

    std::lock_guard lock(this->mutex_);
    auto &channelData = this->data(channel);
    const auto removed =
        std::erase_if(channelData.entries, [&](const Entry &entry) {
            return entry.modCase.time == time && entry.modCase.user == user;
        });
    if (removed > 0)
    {
        this->save(channel, channelData);
    }
}

std::optional<ModSuggestion> ModerationAssistant::suggest(
    const QString &channelName, const QString &text) const
{
    // Checked first and without touching the disk, so channels that are not
    // suggesting - nearly all of them - cost nothing
    const auto channel = channelName.toLower();
    if (this->mode(channel) != ModAssistMode::Suggest)
    {
        return std::nullopt;
    }

    const auto words = wordsOf(text);
    if (words.isEmpty())
    {
        return std::nullopt;
    }

    const auto *settings = getSettings();
    const auto threshold =
        std::clamp(settings->modAssistSimilarity.getValue(), 1, 100) / 100.0;

    std::lock_guard lock(this->mutex_);
    const auto &channelData = this->data(channel);
    if (static_cast<int>(channelData.entries.size()) <
        settings->modAssistMinCases.getValue())
    {
        return std::nullopt;
    }

    QHash<int, int> outcomes;
    int similar = 0;
    for (const auto &entry : channelData.entries)
    {
        double best = 0;
        for (const auto &messageWords : entry.words)
        {
            best = std::max(best, similarity(words, messageWords));
        }

        if (best >= threshold)
        {
            outcomes[entry.modCase.seconds]++;
            similar++;
        }
    }

    if (similar < std::max(1, settings->modAssistMinSimilar.getValue()))
    {
        return std::nullopt;
    }

    // Most common outcome first; on a tie the milder one, so the suggestion
    // never reaches further than the moderators did
    std::vector<std::pair<int, int>> ranked;
    for (auto it = outcomes.cbegin(); it != outcomes.cend(); ++it)
    {
        ranked.emplace_back(it.key(), it.value());
    }
    std::ranges::sort(ranked, [](const auto &a, const auto &b) {
        if (a.second != b.second)
        {
            return a.second > b.second;
        }
        const auto severity = [](int seconds) {
            return seconds == 0 ? INT_MAX : seconds;
        };
        return severity(a.first) < severity(b.first);
    });

    QStringList spread;
    for (size_t i = 0; i < ranked.size() && i < 3; i++)
    {
        spread.append(QStringLiteral("%1 ×%2").arg(describe(ranked[i].first),
                                                   QString::number(
                                                       ranked[i].second)));
    }

    return ModSuggestion{
        .seconds = ranked.front().first,
        .similarCases = similar,
        .spread = spread.join(QStringLiteral(", ")),
    };
}

void ModerationAssistant::onMessage(const QString &channelName,
                                    const QString &loginName,
                                    const QString &displayName,
                                    const QString &text, const QString &badges)
{
    // Checked first and without anything else, so channels that are not
    // suggesting - nearly all of them - cost nothing
    const auto channel = channelName.toLower();
    if (this->mode(channel) != ModAssistMode::Suggest || loginName.isEmpty())
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

    // An alert already open for them - the repeated message one, say - says
    // enough, and a flood of lookalikes gets a few windows rather than dozens
    if (ModAlertPopup::openFor(channel, login) != nullptr ||
        ModAlertPopup::openCount() >= MAX_SUGGESTION_WINDOWS)
    {
        return;
    }

    const auto suggestion = this->suggest(channel, text);
    if (!suggestion)
    {
        return;
    }

    auto *popup = ModAlertPopup::obtain(
        channel, login, &getApp()->getWindows()->getMainWindow());
    popup->setSuggestion(displayName.isEmpty() ? login : displayName,
                         suggestion->seconds, suggestion->similarCases,
                         suggestion->spread);
    popup->show();
    popup->raise();
}

int ModerationAssistant::importFromLogs(const QString &channelName)
{
    const auto channel = channelName.toLower();
    const QDir directory(logDirectory(channel));
    if (channel.isEmpty() || !directory.exists())
    {
        return -1;
    }

    const auto files = directory.entryList(
        {channel + QStringLiteral("-*.log")}, QDir::Files, QDir::Name);
    if (files.isEmpty())
    {
        return -1;
    }

    // "[timestamp] name: text" - the name part may be "Localized login"
    static const QRegularExpression chatLine(
        QStringLiteral(R"(^\[([^\]]*)\]\s+(\S.*?):\s(.*)$)"));
    // Twitch's own notice. It names neither moderator nor reason, but it is
    // the line every timeout leaves behind, so most cases come from it.
    static const QRegularExpression noticeLine(QStringLiteral(
        R"(^\[([^\]]*)\]\s+(\S+) has been (?:timed out for ((?:\d+[dhms] ?)+?)|permanently banned)\.$)"));
    // A moderator's own line, logged under their name. Actions that came in
    // through shared chat add " in <channel>".
    static const QRegularExpression timeoutText(QStringLiteral(
        R"(^(\S+) timed out (\S+) for ((?:\d+[dhms] ?)+?)(?: in \S+?)?(?:\.|: (.*))$)"));
    static const QRegularExpression banText(QStringLiteral(
        R"(^(\S+) banned (.+?)(?: in \S+?)?(?:\.|: (.*))$)"));

    const auto timestampFormat = getSettings()->logTimestampFormat.getValue();

    struct Said {
        QDateTime time;
        QString text;
    };
    QHash<QString, QList<Said>> recent;
    std::vector<ModCase> found;

    const auto takeAction = [&](ModCase modCase) {
        // The notice and the moderator's line describe the same action -
        // whichever comes second only fills in what the first lacked
        for (auto &known : found)
        {
            if (known.user == modCase.user &&
                std::abs(known.time.secsTo(modCase.time)) <= DUPLICATE_SECONDS)
            {
                if (known.moderator.isEmpty())
                {
                    known.moderator = modCase.moderator;
                }
                if (known.reason.isEmpty())
                {
                    known.reason = modCase.reason;
                }
                return;
            }
        }

        for (const auto &said : recent.value(modCase.user))
        {
            if (said.time.secsTo(modCase.time) <= CONTEXT_SECONDS)
            {
                modCase.messages.append(said.text);
            }
        }
        recent.remove(modCase.user);

        if (!modCase.messages.isEmpty())
        {
            found.push_back(std::move(modCase));
        }
    };

    for (const auto &fileName : files)
    {
        const auto date = QDate::fromString(fileName.mid(channel.size() + 1, 10),
                                            QStringLiteral("yyyy-MM-dd"));
        QFile file(directory.absoluteFilePath(fileName));
        if (!date.isValid() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            continue;
        }

        QTextStream stream(&file);
        while (!stream.atEnd())
        {
            // System lines are logged with a space after the final full stop
            const auto line = stream.readLine().trimmed();

            const auto notice = noticeLine.match(line);
            if (notice.hasMatch())
            {
                const QDateTime time(date, QTime::fromString(notice.captured(1),
                                                             timestampFormat));
                if (time.isValid())
                {
                    ModCase modCase;
                    modCase.time = time;
                    modCase.user = notice.captured(2).toLower();
                    modCase.seconds = notice.captured(3).isEmpty()
                                          ? 0
                                          : parseDuration(notice.captured(3));
                    takeAction(std::move(modCase));
                }
                continue;
            }

            const auto match = chatLine.match(line);
            if (!match.hasMatch())
            {
                continue;
            }

            const QDateTime time(
                date, QTime::fromString(match.captured(1), timestampFormat));
            if (!time.isValid())
            {
                continue;
            }

            const auto name =
                match.captured(2).split(' ', Qt::SkipEmptyParts).last().toLower();
            const auto text = match.captured(3);

            // A moderator's line names them again at the start - a chatter
            // typing the same words would have to fake both
            const auto timeout = timeoutText.match(text);
            const auto ban = timeout.hasMatch() ? QRegularExpressionMatch()
                                                : banText.match(text);
            const auto &action = timeout.hasMatch() ? timeout : ban;
            if (action.hasMatch() &&
                action.captured(1).compare(name, Qt::CaseInsensitive) == 0)
            {
                ModCase modCase;
                modCase.time = time;
                modCase.moderator = name;
                if (timeout.hasMatch())
                {
                    modCase.user = timeout.captured(2).toLower();
                    modCase.seconds = parseDuration(timeout.captured(3));
                    modCase.reason = timeout.captured(4).trimmed();
                }
                else
                {
                    // Bans name the user as "Display" or "Localized (login)"
                    auto user = ban.captured(2);
                    const auto open = user.lastIndexOf('(');
                    if (open >= 0 && user.endsWith(')'))
                    {
                        user = user.mid(open + 1, user.size() - open - 2);
                    }
                    modCase.user = user.trimmed().toLower();
                    modCase.reason = ban.captured(3).trimmed();
                }
                takeAction(std::move(modCase));
                continue;
            }

            auto &said = recent[name];
            said.append({time, text});
            while (said.size() > MESSAGES_PER_CASE)
            {
                said.removeFirst();
            }
        }
    }

    std::lock_guard lock(this->mutex_);
    auto &channelData = this->data(channel);
    int added = 0;
    for (auto &modCase : found)
    {
        if (add(channelData, std::move(modCase)))
        {
            added++;
        }
    }
    if (added > 0)
    {
        this->save(channel, channelData);
    }
    return added;
}

ModerationAssistant::ChannelData &ModerationAssistant::data(
    const QString &channel) const
{
    auto &channelData = this->channels_[channel];
    if (channelData.loaded)
    {
        return channelData;
    }
    channelData.loaded = true;

    const auto directory = storeDirectory();
    if (directory.isEmpty())
    {
        return channelData;
    }

    QFile file(QDir(directory).absoluteFilePath(channel + ".json"));
    if (!file.open(QIODevice::ReadOnly))
    {
        return channelData;
    }

    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    for (const auto value : root.value(QStringLiteral("cases")).toArray())
    {
        const auto object = value.toObject();

        ModCase modCase;
        modCase.time = QDateTime::fromString(
            object.value(QStringLiteral("time")).toString(), Qt::ISODate);
        modCase.moderator = object.value(QStringLiteral("moderator")).toString();
        modCase.user = object.value(QStringLiteral("user")).toString();
        modCase.seconds = object.value(QStringLiteral("seconds")).toInt();
        modCase.reason = object.value(QStringLiteral("reason")).toString();
        for (const auto message :
             object.value(QStringLiteral("messages")).toArray())
        {
            modCase.messages.append(message.toString());
        }

        add(channelData, std::move(modCase));
    }

    return channelData;
}

void ModerationAssistant::save(const QString &channel,
                               const ChannelData &channelData) const
{
    const auto directory = storeDirectory();
    if (directory.isEmpty())
    {
        return;
    }

    QJsonArray cases;
    for (const auto &entry : channelData.entries)
    {
        const auto &modCase = entry.modCase;
        cases.append(QJsonObject{
            {QStringLiteral("time"), modCase.time.toString(Qt::ISODate)},
            {QStringLiteral("moderator"), modCase.moderator},
            {QStringLiteral("user"), modCase.user},
            {QStringLiteral("seconds"), modCase.seconds},
            {QStringLiteral("reason"), modCase.reason},
            {QStringLiteral("messages"),
             QJsonArray::fromStringList(modCase.messages)},
        });
    }

    QSaveFile file(QDir(directory).absoluteFilePath(channel + ".json"));
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("cases"), cases}})
                   .toJson(QJsonDocument::Compact));
    file.commit();
}

bool ModerationAssistant::add(ChannelData &channelData, ModCase modCase)
{
    if (!modCase.time.isValid() || modCase.user.isEmpty())
    {
        return false;
    }

    for (const auto &entry : channelData.entries)
    {
        if (entry.modCase.user == modCase.user &&
            std::abs(entry.modCase.time.secsTo(modCase.time)) <=
                DUPLICATE_SECONDS)
        {
            return false;
        }
    }

    Entry entry;
    for (const auto &message : modCase.messages)
    {
        entry.words.append(wordsOf(message));
    }
    entry.modCase = std::move(modCase);

    // Kept in time order, since the log import can hand in older cases after
    // newer ones already arrived live
    const auto position = std::ranges::upper_bound(
        channelData.entries, entry.modCase.time, {},
        [](const Entry &e) {
            return e.modCase.time;
        });
    channelData.entries.insert(position, std::move(entry));

    if (static_cast<int>(channelData.entries.size()) > MAX_CASES)
    {
        channelData.entries.erase(channelData.entries.begin(),
                                  channelData.entries.end() - MAX_CASES);
    }
    return true;
}

}  // namespace chatterino
