// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

#include <QTimer>

class QTabWidget;
class QPushButton;
class QLabel;

namespace chatterino {

class ModerationPage : public SettingsPage
{
public:
    ModerationPage();

    void selectModerationActions();

    /// The settings dialog is created once and reused, so the log size has to
    /// be recomputed every time the page is shown - otherwise it keeps
    /// displaying whatever it was when the dialog was first opened.
    void onShow() override;

private:
    void addModerationButtonSettings(QTabWidget *);

    void refreshLogDirectorySize();

    QLabel *logsPathSizeLabel_{};

    QTimer itemsChangedTimer_;
    QTabWidget *tabWidget_{};

    std::vector<QLineEdit *> durationInputs_;
    std::vector<QComboBox *> unitInputs_;
};

}  // namespace chatterino
