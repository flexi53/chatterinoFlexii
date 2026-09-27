// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

#include <optional>
#include <vector>

namespace chatterino {

struct CommandContext;
struct HelixBlockedTerm;

}  // namespace chatterino

/// The words AutoMod holds back in a channel - kept on Twitch's side, one
/// list per channel, and until now only to be seen on the website. Every
/// moderator of the channel may read and change them.
namespace chatterino::commands {

/// /blockterm <word or phrase>
QString blockTerm(const CommandContext &ctx);
/// /unblockterm <word or phrase>
QString unblockTerm(const CommandContext &ctx);
/// /blockterms - what is on the list
QString listBlockedTerms(const CommandContext &ctx);

/// The id Twitch keeps @a text under, as far as it is on @a terms - what
/// taking a term away needs. However it is written.
std::optional<QString> idOfTerm(const std::vector<HelixBlockedTerm> &terms,
                                const QString &text);

}  // namespace chatterino::commands
