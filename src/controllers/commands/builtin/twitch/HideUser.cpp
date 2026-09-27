// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/HideUser.hpp"

#include "common/Channel.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/moderation/HiddenUsers.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "widgets/dialogs/NukePopup.hpp"

namespace chatterino::commands {

namespace {

/// How far back /nuke looks and how long the timeout lasts, without being
/// told otherwise
constexpr int NUKE_MINUTES = 10;
constexpr int NUKE_SECONDS = 60;

}  // namespace

QString hideUser(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    const auto names = hiddenusers::read(ctx.words.mid(1).join(' '));
    if (names.isEmpty())
    {
        ctx.channel->addSystemMessage(
            "So geht es: /hide <Name> - seine Nachrichten kommen dann nicht "
            "mehr an, nur bei dir. Auf Twitch merkt das niemand: weder "
            "geblockt noch gebannt. Mehrere Namen gehen auch.");
        return "";
    }

    QStringList done;
    for (const auto &name : names)
    {
        if (hiddenusers::add(name))
        {
            done.append(name);
        }
    }

    ctx.channel->addSystemMessage(
        done.isEmpty()
            ? QStringLiteral("War schon versteckt.")
            : QStringLiteral("Versteckt: %1. Was schon im Chat steht, bleibt "
                             "stehen.")
                  .arg(done.join(", ")));
    return "";
}

QString unhideUser(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    const auto names = hiddenusers::read(ctx.words.mid(1).join(' '));
    if (names.isEmpty())
    {
        ctx.channel->addSystemMessage(
            "So geht es: /unhide <Name> - er kommt wieder durch. /hidden "
            "zeigt, wer versteckt ist.");
        return "";
    }

    QStringList done;
    for (const auto &name : names)
    {
        if (hiddenusers::remove(name))
        {
            done.append(name);
        }
    }

    ctx.channel->addSystemMessage(
        done.isEmpty() ? QStringLiteral("Der war nicht versteckt.")
                       : QStringLiteral("Kommt wieder durch: %1")
                             .arg(done.join(", ")));
    return "";
}

QString listHiddenUsers(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    const auto names = hiddenusers::all();
    ctx.channel->addSystemMessage(
        names.isEmpty()
            ? QStringLiteral("Niemand versteckt.")
            : QStringLiteral("Versteckt (%1): %2")
                  .arg(names.size())
                  .arg(names.join(", ")));
    return "";
}

QString nuke(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage("/nuke geht nur in einem Twitch-Kanal.");
        return "";
    }

    // The last argument may be a length, the one before it the stretch of
    // chat to look at - both plain numbers, so a phrase ending in one is
    // taken as a phrase
    auto words = ctx.words.mid(1);
    int minutes = NUKE_MINUTES;
    int seconds = NUKE_SECONDS;
    const auto number = [](const QString &word, int &into) {
        bool ok = false;
        const auto value = word.toInt(&ok);
        if (ok && value > 0)
        {
            into = value;
        }
        return ok && value > 0;
    };
    if (words.size() >= 3 && number(words.back(), seconds))
    {
        words.removeLast();
        if (number(words.back(), minutes))
        {
            words.removeLast();
        }
    }

    const auto phrase = words.join(' ').trimmed();
    if (phrase.isEmpty())
    {
        ctx.channel->addSystemMessage(
            QStringLiteral(
                "So geht es: /nuke <Ausdruck> [Minuten] [Sekunden] - zeigt, "
                "wer den Ausdruck in den letzten %1 Minuten geschrieben hat, "
                "und bietet einen Timeout von %2 Sekunden für alle davon an. "
                "Gedrückt werden muss noch selbst.")
                .arg(NUKE_MINUTES)
                .arg(NUKE_SECONDS));
        return "";
    }

    auto account = getApp()->getAccounts()->twitch.getCurrent();
    const auto caught = NukePopup::whoWrote(ctx.channel, phrase, minutes,
                                            account->getUserName());
    if (caught.empty())
    {
        ctx.channel->addSystemMessage(
            QStringLiteral("„%1“ hat in den letzten %2 Minuten niemand "
                           "geschrieben - jedenfalls nicht, solange der Tab "
                           "offen war.")
                .arg(phrase)
                .arg(minutes));
        return "";
    }

    auto *popup = new NukePopup(ctx.channel, phrase, seconds, caught,
                                nullptr);
    popup->show();
    return "";
}

}  // namespace chatterino::commands
