// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"

#include <QTimer>

#include <chrono>

namespace chatterino {

/// A thin bar under the input that runs out while a channel makes you wait
/// - slow mode, or a timeout - the way Twitch shows it. Off until it is
/// switched on under Look -> Chat.
class SendWaitBar : public BaseWidget
{
public:
    explicit SendWaitBar(QWidget *parent);

    /// Runs out over @a remaining, drawn as the share of @a total that is
    /// left. A wait that is already over hides the bar.
    void run(std::chrono::milliseconds remaining,
             std::chrono::milliseconds total);

    /// Hides the bar - there is nothing to wait for
    void stop();

protected:
    void paintEvent(QPaintEvent *event) override;
    void scaleChangedEvent(float scale) override;

private:
    /// How much of the wait is left, from 1 to 0
    double share() const;

    std::chrono::steady_clock::time_point end_;
    std::chrono::milliseconds total_{0};
    /// Redraws while the bar runs, often enough to look smooth
    QTimer tick_;
};

}  // namespace chatterino
