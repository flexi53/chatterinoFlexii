// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QWidget>

namespace chatterino {

/// A thin bar in an alert window saying how far past the number the alert
/// goes off at - reading "12 Emotes, Alarm ab 8" takes a moment,
/// seeing it does not. The mark sits where the alert starts; the bar runs
/// on beyond it.
class AlertLevelBar : public QWidget
{
public:
    explicit AlertLevelBar(QWidget *parent = nullptr);

    /// Shows @a value against the @a threshold it had to pass, in @a color.
    /// A value or threshold of nothing hides the bar.
    void show(int value, int threshold, const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int value_ = 0;
    int threshold_ = 0;
    QColor color_;
};

}  // namespace chatterino
