// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QString>

namespace chatterino {

/// Automatik -> Autonachrichten, switched on and off for the channel its
/// split shows: a speech bubble, coloured while your account answers here
/// by itself, struck through while it does not.
class AutoMessageButton : public Button
{
public:
    explicit AutoMessageButton(BaseWidget *parent = nullptr);

    /// The channel it switches - the one its split shows
    void setChannel(const QString &channel);

protected:
    void paintContent(QPainter &painter) override;

private:
    void refreshTooltip();

    QString channel_;

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
