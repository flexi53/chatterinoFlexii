// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/buttons/Button.hpp"

#include <QRectF>
#include <QString>

#include <pajlada/signals/signalholder.hpp>

class QPainter;

namespace chatterino {

/// The bell the alert buttons are drawn as, filling @a box, in the pen the
/// painter holds. @a struck adds the line through it that says the alerts
/// are silenced. Shared by the bell in the input bar and the mark on a tab.
void paintBell(QPainter &painter, const QRectF &box, bool struck);

/// Silences the alert windows of the moderation assistant - they stop
/// popping up until it is pressed again. What the alerts watch for goes on
/// as before, and so does everything set per channel behind the shield next
/// to it; only the windows stay away. Drawn as a bell, struck through while
/// they are silenced. Stays on this computer.
class AlertMuteButton : public Button
{
public:
    explicit AlertMuteButton(BaseWidget *parent = nullptr);

    /// The channel it switches - the one its split shows
    void setChannel(const QString &channel);

protected:
    void paintContent(QPainter &painter) override;

private:
    void refreshTooltip();
    bool muted() const;

    QString channel_;

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
