// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/StepEscalation.hpp"

#include <algorithm>

namespace chatterino {

std::optional<int> StepEscalation::more(int amount, int again)
{
    if (!this->offered)
    {
        this->offered = true;
        this->sinceOffer = 0;
        return this->level;
    }

    // Offered already and nobody has acted: back a step higher once they
    // have kept at it as long as it took to be flagged in the first place
    this->sinceOffer += std::max(0, amount);
    if (this->sinceOffer < std::max(1, again))
    {
        return std::nullopt;
    }
    this->sinceOffer = 0;
    return ++this->level;
}

void StepEscalation::actedOn()
{
    this->actions++;
    this->level++;
    this->offered = false;
    this->sinceOffer = 0;
}

}  // namespace chatterino
