// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BasePopup.hpp"

#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace chatterino {

/// The moderation assistant for one channel: whether it runs, what it has
/// collected so far, and a way to look through and prune the cases.
class ModerationAssistantPopup : public BasePopup
{
    Q_OBJECT

public:
    ModerationAssistantPopup(const QString &channel, QWidget *parent);

private:
    void refresh();

    QString channel_;
    QComboBox *mode_{};
    QLabel *status_{};
    QLineEdit *search_{};
    QTableWidget *table_{};
};

}  // namespace chatterino
