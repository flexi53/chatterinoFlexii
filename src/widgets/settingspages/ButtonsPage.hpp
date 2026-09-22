// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

class QTabWidget;

namespace chatterino {

/// Which of the small buttons are there - the row under the chat and the
/// ones in the tab bar - and, in a tab of its own, what the title bar over
/// each chat holds. Everything they do can be reached another way, so
/// nothing is lost by switching one off; the row just gets quieter.
class ButtonsPage : public SettingsPage
{
public:
    ButtonsPage();

    bool filterElements(const QString &query) override;

private:
    void initLayout(GeneralPageView &layout);
    void initTitleBar(GeneralPageView &layout);

    QTabWidget *tabs_{};
    GeneralPageView *view_{};
    GeneralPageView *titleBar_{};
};

}  // namespace chatterino
