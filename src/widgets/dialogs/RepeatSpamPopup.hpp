// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BasePopup.hpp"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimer>

class QLabel;
class QPushButton;

namespace chatterino {

/// Shows a chatter's repeated messages and the timeouts in between, and a
/// button that hands out the timeout that fits. Closes by itself after a
/// while unless the pointer is resting on it.
class RepeatSpamPopup : public BasePopup
{
    Q_OBJECT

public:
    struct Entry {
        QDateTime time;
        QString text;
        /// -1 for a message, otherwise the length of a timeout, 0 for a ban
        int timeoutSeconds = -1;
    };

    RepeatSpamPopup(QString channel, QString login, QWidget *parent);

    /// Fills in the history and the timeout the button offers, and starts
    /// the countdown to closing again. @a timeoutsServed counts the timeouts
    /// the chatter has already sat out for this message.
    void setCase(const QString &displayName, const QList<Entry> &history,
                 int seconds, int timeoutsServed);

    /// A test alert looks and behaves like a real one, but its buttons send
    /// nothing
    void setTestMode(bool test);

private:
    void tick();

    QString channel_;
    QString login_;
    int seconds_ = 30;
    bool test_ = false;
    /// Seconds until it closes by itself, 0 when it does not
    int remaining_ = 0;

    QLabel *headline_{};
    QLabel *messages_{};
    QLabel *testNote_{};
    QPushButton *ignore_{};
    QPushButton *timeout_{};
    QTimer countdown_;
};

}  // namespace chatterino
