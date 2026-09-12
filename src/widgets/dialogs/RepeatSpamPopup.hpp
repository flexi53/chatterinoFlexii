// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BasePopup.hpp"

#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>

class QLabel;
class QPushButton;

namespace chatterino {

/// Shows a chatter's repeated messages with their times, and a button that
/// hands out the timeout that fits.
class RepeatSpamPopup : public BasePopup
{
    Q_OBJECT

public:
    RepeatSpamPopup(QString channel, QString login, QWidget *parent);

    /// Fills in the messages and the timeout the button offers.
    /// @a timeoutsServed counts the timeouts the chatter has already sat out
    /// for this message.
    void setCase(const QString &displayName,
                 const QList<QPair<QDateTime, QString>> &messages, int seconds,
                 int timeoutsServed);

private:
    QString channel_;
    QString login_;
    int seconds_ = 30;

    QLabel *headline_{};
    QLabel *messages_{};
    QPushButton *timeout_{};
};

}  // namespace chatterino
