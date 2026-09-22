// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

/// Everything about badges: a tab that says when a new one can be had, and
/// choosing which one you wear. Both need something of your own - a
/// BadgeBase key, the login of your browser - kept in the system's keychain.
class BadgesPage : public SettingsPage
{
public:
    BadgesPage();

    bool filterElements(const QString &query) override;

private:
    void initAlerts(GeneralPageView &layout);
    void initSwitching(GeneralPageView &layout);

    GeneralPageView *view_{};
};

}  // namespace chatterino
