// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

/// Which of the small buttons are there - the row under the chat and the
/// ones in the tab bar. Everything they do can be reached another way, so
/// nothing is lost by switching one off; the row just gets quieter.
class ButtonsPage : public SettingsPage
{
public:
    ButtonsPage();

    bool filterElements(const QString &query) override;

    int changedSettings() override;
    void showOnlyChanged(bool only) override;

private:
    void initLayout(GeneralPageView &layout);

    GeneralPageView *view_{};
};

}  // namespace chatterino
