// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>

#include <optional>
#include <vector>

namespace chatterino {

struct Message;
class HighlightBadge;

/// Who wrote a message, as far as a role stripe tells it - see Look -> Chat
enum class ChatRole {
    None,
    Subscriber,
    Vip,
    Moderator,
    Broadcaster,
};

/// The highest role the badges of @a message show - a moderator who also
/// subscribes is a moderator
ChatRole roleOf(const Message &message);

/// The colours set for each role under Look -> Chat, invalid for none
struct RoleColors {
    QColor broadcaster;
    QColor moderator;
    QColor vip;
    QColor subscriber;
};

/// The colour of the stripe for @a message. A badge highlight comes first:
/// the one for the badge that gives the message its role - lead_moderator,
/// say - or else the one for the role's plain badge, moderator. Made solid,
/// as a highlight's colour is a see-through tint. Without one it is the
/// colour set for the role in @a colors; invalid for no stripe.
QColor stripeColor(const Message &message,
                   const std::vector<HighlightBadge> &highlights,
                   const RoleColors &colors);

/// The colour a badge highlight gives the stripe of @a role, if one does -
/// for the settings page, where the role's own colour then does not count
std::optional<QColor> badgeStripeColor(
    ChatRole role, const std::vector<HighlightBadge> &highlights);

}  // namespace chatterino
