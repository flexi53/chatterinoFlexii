// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;
class QVBoxLayout;

namespace chatterino {

class ModAssistantPage;

/// Erweitert: the parts that belong to the account this was built for - the
/// moderation assistant with its alert windows, and the User tab that
/// gathers what watched people write. The page is only there while that
/// account is logged in, see util/Advanced.hpp.
class AdvancedPage : public SettingsPage
{
public:
    AdvancedPage();

    bool filterElements(const QString &query) override;
    void onShow() override;

private:
    /// The people whose messages land in their own tab
    void buildPeopleTab(QVBoxLayout *layout);
    /// How many people are watched, under the list
    void showPeople();

    QTabWidget *tabs_{};
    ModAssistantPage *assistant_{};
    QLabel *peopleStatus_{};
    QPlainTextEdit *peopleList_{};
    QLineEdit *peopleFilter_{};
    QLabel *filterStatus_{};
};

}  // namespace chatterino
