// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

namespace chatterino {

/// Opens the window with the channel's points and, for moderators, the forms
/// to start a poll or a prediction: three bars of a poll, drawn in the
/// colour of the icons beside it
class TwitchActionsButton : public Button
{
public:
    explicit TwitchActionsButton(BaseWidget *parent = nullptr);

protected:
    void paintContent(QPainter &painter) override;
};

}  // namespace chatterino
