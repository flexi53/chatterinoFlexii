// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "messages/layouts/MessageRole.hpp"
#include "singletons/Settings.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

#include <vector>

namespace chatterino {

/// Everything about how the app looks that is a choice rather than a detail,
/// in tabs: the classic/modern switch and the focus view, the tabs, the chat
/// and the colours. All of it starts out as Chatterino looks, and each part
/// has a "Standard" button that puts it back.
class LookPage : public SettingsPage
{
public:
    LookPage();

    bool filterElements(const QString &query) override;

    int changedSettings() override;
    void showOnlyChanged(bool only) override;

private:
    void buildStyleTab(GeneralPageView &layout);
    void buildTabsTab(GeneralPageView &layout);
    void buildChatTab(GeneralPageView &layout);
    void buildColorsTab(GeneralPageView &layout);
    /// A role's stripe colour - greyed out, showing the badge highlight's
    /// colour, when a badge highlight sets it
    void addRoleColor(GeneralPageView &layout, const QString &name,
                      ChatRole role, QStringSetting &setting);

    std::vector<GeneralPageView *> views_;
};

}  // namespace chatterino
