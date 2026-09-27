// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/BlockedTerms.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"

namespace {

using namespace chatterino;

/// What went wrong, said the way a moderator can act on it
QString sayError(HelixBlockedTermsError error, const QString &message)
{
    switch (error)
    {
        case HelixBlockedTermsError::UserMissingScope:
            return "Dafür fehlt deinem Login die Berechtigung - melde dich "
                   "unter Einstellungen -> Konten neu an.";
        case HelixBlockedTermsError::MissingPermission:
            return "Das geht nur, wo du Moderator bist.";
        case HelixBlockedTermsError::Forwarded:
            return message;
        case HelixBlockedTermsError::Unknown:
            break;
    }
    return QStringLiteral("Twitch mochte nicht: %1").arg(message);
}

/// The channel and your own id, or nothing with a word said in the chat
struct Where {
    QString broadcasterID;
    QString moderatorID;
    ChannelPtr channel;
};

std::optional<Where> whereWeAre(const CommandContext &ctx,
                                const QString &command)
{
    if (ctx.channel == nullptr)
    {
        return {};
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            QStringLiteral("%1 geht nur in einem Twitch-Kanal.").arg(command));
        return {};
    }

    auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (account->isAnon())
    {
        ctx.channel->addSystemMessage(
            QStringLiteral("Dafür musst du angemeldet sein."));
        return {};
    }

    return Where{
        .broadcasterID = ctx.twitchChannel->roomId(),
        .moderatorID = account->getUserId(),
        .channel = ctx.channel,
    };
}

/// Everything after the command, as it was written - a blocked term may
/// hold spaces
QString wordsAfter(const CommandContext &ctx)
{
    return ctx.words.mid(1).join(' ').trimmed();
}

}  // namespace

namespace chatterino::commands {

std::optional<QString> idOfTerm(const std::vector<HelixBlockedTerm> &terms,
                                const QString &text)
{
    for (const auto &term : terms)
    {
        if (term.text.compare(text, Qt::CaseInsensitive) == 0)
        {
            return term.id;
        }
    }
    return {};
}

QString blockTerm(const CommandContext &ctx)
{
    const auto text = wordsAfter(ctx);
    if (text.isEmpty())
    {
        if (ctx.channel != nullptr)
        {
            ctx.channel->addSystemMessage(
                "So geht es: /blockterm <Wort oder Satz> - AutoMod hält das "
                "dann in diesem Kanal zurück. Ein * steht für beliebige "
                "Zeichen.");
        }
        return "";
    }

    const auto where = whereWeAre(ctx, "/blockterm");
    if (!where)
    {
        return "";
    }

    auto channel = where->channel;
    getHelix()->addBlockedTerm(
        where->broadcasterID, where->moderatorID, text,
        [channel, text](const auto &term) {
            channel->addSystemMessage(
                QStringLiteral("„%1“ wird ab jetzt zurückgehalten.")
                    .arg(term.text.isEmpty() ? text : term.text));
        },
        [channel](auto error, const QString &message) {
            channel->addSystemMessage(
                QStringLiteral("Nicht gesperrt: %1")
                    .arg(sayError(error, message)));
        });
    return "";
}

QString unblockTerm(const CommandContext &ctx)
{
    const auto text = wordsAfter(ctx);
    if (text.isEmpty())
    {
        if (ctx.channel != nullptr)
        {
            ctx.channel->addSystemMessage(
                "So geht es: /unblockterm <Wort oder Satz> - nimmt es wieder "
                "von der Liste. /blockterms zeigt, was drauf steht.");
        }
        return "";
    }

    const auto where = whereWeAre(ctx, "/unblockterm");
    if (!where)
    {
        return "";
    }

    auto channel = where->channel;
    const auto broadcasterID = where->broadcasterID;
    const auto moderatorID = where->moderatorID;

    // Twitch takes terms away by their id, so the list has to be asked for
    getHelix()->getBlockedTerms(
        broadcasterID, moderatorID,
        [channel, broadcasterID, moderatorID, text](const auto &terms) {
            const auto id = idOfTerm(terms, text);
            if (id)
            {
                getHelix()->removeBlockedTerm(
                    broadcasterID, moderatorID, *id,
                    [channel, text] {
                        channel->addSystemMessage(
                            QStringLiteral("„%1“ ist wieder frei.").arg(text));
                    },
                    [channel](auto error, const QString &message) {
                        channel->addSystemMessage(
                            QStringLiteral("Nicht entfernt: %1")
                                .arg(sayError(error, message)));
                    });
                return;
            }

            channel->addSystemMessage(
                QStringLiteral("„%1“ steht hier nicht auf der Liste.")
                    .arg(text));
        },
        [channel](auto error, const QString &message) {
            channel->addSystemMessage(
                QStringLiteral("Die Liste war nicht zu haben: %1")
                    .arg(sayError(error, message)));
        });
    return "";
}

QString listBlockedTerms(const CommandContext &ctx)
{
    const auto where = whereWeAre(ctx, "/blockterms");
    if (!where)
    {
        return "";
    }

    auto channel = where->channel;
    getHelix()->getBlockedTerms(
        where->broadcasterID, where->moderatorID,
        [channel](const auto &terms) {
            if (terms.empty())
            {
                channel->addSystemMessage(
                    "Hier hält AutoMod nichts Eigenes zurück.");
                return;
            }

            QStringList words;
            for (const auto &term : terms)
            {
                words.append(term.text);
            }
            channel->addSystemMessage(
                QStringLiteral("Zurückgehalten (%1): %2")
                    .arg(words.size())
                    .arg(words.join(", ")));
        },
        [channel](auto error, const QString &message) {
            channel->addSystemMessage(
                QStringLiteral("Die Liste war nicht zu haben: %1")
                    .arg(sayError(error, message)));
        });
    return "";
}

}  // namespace chatterino::commands
