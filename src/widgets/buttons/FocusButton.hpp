// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

#include <pajlada/signals/signalholder.hpp>

namespace chatterino {

/// Steps in and out of the focus view (Look -> Stil) - it sits in the input
/// bar of every split, between the moderation assistant and the emotes, so
/// it stays in reach when the tabs and split headers are gone. Drawn as the
/// corners of a frame: pointing out to step in, pointing in to step out.
class FocusButton : public Button
{
public:
    explicit FocusButton(BaseWidget *parent = nullptr);

protected:
    void paintContent(QPainter &painter) override;

private:
    void updateTooltip();

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
