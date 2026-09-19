// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

namespace chatterino {

/// Empties the chat of its split, here only - what the split menu's Clear
/// messages does, one click away in the input bar. Handy for mentions that
/// have been dealt with. Drawn as a bin, in the style of the icons next to it.
class ClearChatButton : public Button
{
public:
    explicit ClearChatButton(BaseWidget *parent = nullptr);

protected:
    void paintContent(QPainter &painter) override;
};

}  // namespace chatterino
