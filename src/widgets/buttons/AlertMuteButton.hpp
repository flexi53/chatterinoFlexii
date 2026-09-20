// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

#include <pajlada/signals/signalholder.hpp>

namespace chatterino {

/// Silences the alert windows of the moderation assistant - they stop
/// popping up until it is pressed again. What the alerts watch for goes on
/// as before, and so does everything set per channel behind the shield next
/// to it; only the windows stay away. Drawn as a bell, struck through while
/// they are silenced. Stays on this computer.
class AlertMuteButton : public Button
{
public:
    explicit AlertMuteButton(BaseWidget *parent = nullptr);

protected:
    void paintContent(QPainter &painter) override;

private:
    void refreshTooltip();

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
