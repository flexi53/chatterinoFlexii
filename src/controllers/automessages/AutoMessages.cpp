// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/automessages/AutoMessages.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "util/Advanced.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

namespace {

using namespace chatterino;

/// However the rules are set, one channel never gets two answers closer
/// together than this - so a gift train cannot turn into a wall of text
constexpr int MIN_GAP_MS = 2000;

const QStringList KIND_KEYS{"chat", "sub", "resub", "gift", "raid"};

QString keyOf(AutoMessages::Kind kind)
{
    return KIND_KEYS.at(static_cast<int>(kind));
}

AutoMessages::Kind kindOf(const QString &key)
{
    const auto index = KIND_KEYS.indexOf(key);
    return index < 0 ? AutoMessages::Kind::Chat
                     : static_cast<AutoMessages::Kind>(index);
}

/// Compiled once per pattern - chat goes by fast
const QRegularExpression &compiled(const QString &pattern)
{
    static QHash<QString, QRegularExpression> cache;
    auto it = cache.find(pattern);
    if (it == cache.end())
    {
        if (cache.size() > 200)
        {
            cache.clear();
        }
        it = cache.insert(
            pattern,
            QRegularExpression(pattern,
                               QRegularExpression::CaseInsensitiveOption |
                                   QRegularExpression::UseUnicodePropertiesOption));
    }
    return it.value();
}

QString ownLogin()
{
    return getApp()->getAccounts()->twitch.getCurrent()->getUserName().toLower();
}

}  // namespace

namespace chatterino {

AutoMessages &AutoMessages::instance()
{
    static AutoMessages self;
    return self;
}

void AutoMessages::onChat(const QString &channel, const QString &login,
                          const QString &displayName, const QString &text)
{
    if (login.compare(ownLogin(), Qt::CaseInsensitive) == 0)
    {
        return;  // never answer yourself, or one rule could feed itself
    }
    this->onEvent({
        .kind = Kind::Chat,
        .channel = channel,
        .login = login,
        .displayName = displayName,
        .text = text,
    });
}

void AutoMessages::onEvent(const Event &event)
{
    auto &s = *getSettings();
    if (!advanced::unlocked() || !isOn(event.channel))
    {
        return;
    }

    // Your own gift or sub is not something to thank yourself for
    if (!event.login.isEmpty() &&
        event.login.compare(ownLogin(), Qt::CaseInsensitive) == 0)
    {
        return;
    }

    const auto now = QDateTime::currentDateTimeUtc();
    const auto channel = event.channel.toLower();
    if (auto last = this->lastSent_.find(channel);
        last != this->lastSent_.end() &&
        last.value().msecsTo(now) < MIN_GAP_MS)
    {
        return;
    }

    const auto rules = read(s.autoMessagesRules.getValue());
    const auto index = pick(rules, event, this->lastFired_, now);
    if (!index)
    {
        return;
    }
    const auto &rule = rules[*index];
    const auto reply = fill(
        rule.replies.at(QRandomGenerator::global()->bounded(
            static_cast<int>(rule.replies.size()))),
        event);
    if (reply.trimmed().isEmpty())
    {
        return;
    }

    // Counted from now, so the waiting time cannot let a second one through
    this->lastFired_[cooldownKey(*index, channel)] = now;
    this->lastSent_[channel] = now;

    const auto low = std::max(0.0, std::min(rule.delayMin, rule.delayMax));
    const auto high = std::max(low, std::max(rule.delayMin, rule.delayMax));
    const auto delayMs = static_cast<int>(
        (low + QRandomGenerator::global()->generateDouble() * (high - low)) *
        1000);

    QTimer::singleShot(delayMs, [channel, reply] {
        // Switched off or logged out in the meantime: say nothing
        if (!advanced::unlocked() || !isOn(channel))
        {
            return;
        }
        auto target = getApp()->getTwitch()->getChannelOrEmpty(channel);
        if (auto *twitch = dynamic_cast<TwitchChannel *>(target.get()))
        {
            twitch->sendMessage(reply);
        }
    });
}

QStringList AutoMessages::onChannels()
{
    return getSettings()->autoMessagesChannels.getValue().split(
        u';', Qt::SkipEmptyParts);
}

bool AutoMessages::isOn(const QString &channel)
{
    return !channel.isEmpty() && onChannels().contains(channel.toLower());
}

void AutoMessages::setOn(const QString &channel, bool on)
{
    if (channel.isEmpty())
    {
        return;
    }
    auto channels = onChannels();
    channels.removeAll(channel.toLower());
    if (on)
    {
        channels.append(channel.toLower());
    }
    channels.sort();
    getSettings()->autoMessagesChannels = channels.join(u';');
}

std::vector<AutoMessages::Rule> AutoMessages::read(const QString &json)
{
    std::vector<Rule> rules;
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    for (const auto &value : doc.array())
    {
        const auto o = value.toObject();
        Rule rule;
        rule.enabled = o.value("enabled").toBool(true);
        rule.kind = kindOf(o.value("kind").toString());
        rule.channel = o.value("channel").toString().trimmed().toLower();
        rule.pattern = o.value("pattern").toString();
        rule.minCount = std::max(1, o.value("minCount").toInt(1));
        for (const auto &reply : o.value("replies").toArray())
        {
            if (!reply.toString().trimmed().isEmpty())
            {
                rule.replies.append(reply.toString());
            }
        }
        rule.delayMin = std::max(0.0, o.value("delayMin").toDouble(0.5));
        rule.delayMax = std::max(0.0, o.value("delayMax").toDouble(3.0));
        rule.cooldown = std::max(0, o.value("cooldown").toInt(10));
        rules.push_back(rule);
    }
    return rules;
}

QString AutoMessages::write(const std::vector<Rule> &rules)
{
    QJsonArray array;
    for (const auto &rule : rules)
    {
        QJsonArray replies;
        for (const auto &reply : rule.replies)
        {
            replies.append(reply);
        }
        array.append(QJsonObject{
            {"enabled", rule.enabled},
            {"kind", keyOf(rule.kind)},
            {"channel", rule.channel.trimmed().toLower()},
            {"pattern", rule.pattern},
            {"minCount", rule.minCount},
            {"replies", replies},
            {"delayMin", rule.delayMin},
            {"delayMax", rule.delayMax},
            {"cooldown", rule.cooldown},
        });
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

std::optional<size_t> AutoMessages::pick(
    const std::vector<Rule> &rules, const Event &event,
    const QHash<QString, QDateTime> &lastFired, const QDateTime &now)
{
    const auto channel = event.channel.toLower();
    std::optional<size_t> best;
    for (size_t i = 0; i < rules.size(); i++)
    {
        const auto &rule = rules[i];
        if (!rule.enabled || rule.kind != event.kind || rule.replies.isEmpty())
        {
            continue;
        }
        if (!rule.channel.isEmpty() && rule.channel != channel)
        {
            continue;
        }
        if (rule.kind == Kind::Chat)
        {
            if (rule.pattern.isEmpty() || !compiled(rule.pattern).isValid() ||
                !compiled(rule.pattern).match(event.text).hasMatch())
            {
                continue;
            }
        }
        if ((rule.kind == Kind::Gift || rule.kind == Kind::Raid) &&
            event.count < rule.minCount)
        {
            continue;
        }

        // Gifts and raids: the highest threshold reached is the one that
        // speaks, so 10 subs do not get the answer meant for 1. Everything
        // else: the first one in the list.
        const bool better =
            !best || ((rule.kind == Kind::Gift || rule.kind == Kind::Raid) &&
                      rule.minCount > rules[*best].minCount);
        if (better)
        {
            best = i;
        }
    }
    if (!best)
    {
        return std::nullopt;
    }

    // The winner cooling down keeps the others quiet too - a smaller gift
    // rule stepping in for it would answer the same gift twice over
    const auto &rule = rules[*best];
    if (auto last = lastFired.find(cooldownKey(*best, channel));
        last != lastFired.end() && rule.cooldown > 0 &&
        last.value().secsTo(now) < rule.cooldown)
    {
        return std::nullopt;
    }
    return best;
}

QString AutoMessages::fill(const QString &reply, const Event &event)
{
    auto out = reply;
    out.replace("{user}", event.displayName.isEmpty() ? event.login
                                                      : event.displayName,
                Qt::CaseInsensitive);
    out.replace("{anzahl}", QString::number(event.count), Qt::CaseInsensitive);
    out.replace("{monate}", QString::number(event.months),
                Qt::CaseInsensitive);
    out.replace("{kanal}", event.channel, Qt::CaseInsensitive);
    return out;
}

QString AutoMessages::problemWith(const QString &pattern)
{
    if (pattern.trimmed().isEmpty())
    {
        return QStringLiteral("Ohne Muster antwortet die Regel nie.");
    }
    const QRegularExpression regex(pattern);
    return regex.isValid() ? QString() : regex.errorString();
}

QString AutoMessages::nameOf(Kind kind)
{
    switch (kind)
    {
        case Kind::Chat:
            return QStringLiteral("Chat-Nachricht (Regex)");
        case Kind::Sub:
            return QStringLiteral("Sub");
        case Kind::Resub:
            return QStringLiteral("Resub");
        case Kind::Gift:
            return QStringLiteral("Sub-Gifts");
        case Kind::Raid:
            return QStringLiteral("Raid");
    }
    return {};
}

QString AutoMessages::cooldownKey(size_t rule, const QString &channel)
{
    return QString::number(rule) + '/' + channel.toLower();
}

}  // namespace chatterino
