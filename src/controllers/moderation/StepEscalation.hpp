// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>

namespace chatterino {

/// Which step of an alert to offer one chatter. The first offer comes when
/// they are flagged. After that the step goes up each time a moderator acts -
/// and also when nobody does: once as much again has come in as it took to be
/// flagged, the offer comes back a step higher, so ignoring an alert does not
/// keep it at the first step.
///
/// Used by the repeated message alert, where "as much again" is three more
/// repeats, and the emote spam alert, where it is as many emotes again as
/// the alert starts at.
struct StepEscalation {
    /// Timeouts or deletions they have had
    int actions = 0;
    /// The step on offer - actions taken plus offers nobody acted on
    int level = 0;
    /// Whether the current step has been offered yet
    bool offered = false;
    /// How much has come in since the last offer
    int sinceOffer = 0;

    /// @a amount more of what got them flagged - one repeat, or so many
    /// emotes. Returns the step to offer now, or nothing while it is too
    /// soon: an offer nobody acted on comes back once @a again more has come
    /// in.
    std::optional<int> more(int amount, int again);

    /// A moderator acted: the next of it is offered the next step at once
    void actedOn();
};

}  // namespace chatterino
