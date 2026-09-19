// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

namespace chatterino {

/// Steps its window in and out of the focus view (Look -> Stil) - it sits in
/// the input bar of every split, between the moderation assistant and the
/// emotes, so it stays in reach when the tabs and split headers are gone.
/// Only its own window changes, so a second window stays as it is. Drawn as
/// the corners of a frame: pointing out to step in, pointing in to step out.
class FocusButton : public Button
{
public:
    explicit FocusButton(BaseWidget *parent = nullptr);

    /// Shows whether its window is in the focus view now
    void refresh();

protected:
    void paintContent(QPainter &painter) override;

private:
    /// Whether its window shows only the chats
    bool active() const;
};

}  // namespace chatterino
