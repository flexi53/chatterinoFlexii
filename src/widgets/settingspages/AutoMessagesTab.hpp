// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/automessages/AutoMessages.hpp"

#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class QWidget;

namespace chatterino {

/// Erweitert -> Autonachrichten: the rules on the left, the chosen one to
/// edit below, and a test that shows what would be sent without sending it.
/// Saved half a second after the last change.
class AutoMessagesTab : public QWidget
{
public:
    AutoMessagesTab();

private:
    void showList();
    void showRule();
    /// Takes what the fields say into the chosen rule
    void takeFields();
    void save();
    void showTest();

    std::vector<AutoMessages::Rule> rules_;
    int current_ = -1;
    /// While the fields are being filled from a rule, their changes are not
    /// changes to it
    bool filling_ = false;

    QCheckBox *enabled_{};
    QListWidget *list_{};
    QPushButton *remove_{};
    QPushButton *up_{};
    QPushButton *down_{};
    QWidget *editor_{};
    QCheckBox *ruleOn_{};
    QComboBox *kind_{};
    QLineEdit *channel_{};
    QWidget *patternRow_{};
    QLineEdit *pattern_{};
    QLabel *patternStatus_{};
    QWidget *countRow_{};
    QLabel *countLabel_{};
    QSpinBox *count_{};
    QPlainTextEdit *replies_{};
    QDoubleSpinBox *delayMin_{};
    QDoubleSpinBox *delayMax_{};
    QSpinBox *cooldown_{};
    QLineEdit *testText_{};
    QSpinBox *testCount_{};
    QLabel *testResult_{};
    QTimer *saveTimer_{};
};

}  // namespace chatterino
