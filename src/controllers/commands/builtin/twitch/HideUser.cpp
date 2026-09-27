// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/HideUser.hpp"

#include "common/Channel.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "controllers/moderation/HiddenUsers.hpp"

namespace chatterino::commands {

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

}  // namespace chatterino::commands
