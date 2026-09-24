// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QRectF>

class QPainter;

namespace chatterino {

/// The chain the follow buttons are drawn as, filling @a box, in the pen the
/// painter holds. @a broken opens the link, which says the tab stays where
/// it is. Shared by the button in the input bar and anything else showing
/// that state.
void paintChain(QPainter &painter, const QRectF &box, bool broken);

/// Switches "Aussehen -> Tabs -> Dem Browser folgen" on and off. That holds
/// for the whole program, like the focus view, so one press in any split
/// counts everywhere. Drawn as a chain: closed while the tab follows, open
/// while it stays put.
class FollowBrowserButton : public Button
{
public:
    explicit FollowBrowserButton(BaseWidget *parent = nullptr);

protected:
    void paintContent(QPainter &painter) override;

private:
    void refreshTooltip();
    static bool following();

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
