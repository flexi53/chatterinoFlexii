// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

class QTabWidget;

namespace chatterino {

/// Titelleiste: everything the bar over each chat is made of - which parts
/// it has and in what order, what the title says, what colour each piece
/// carries, and the activity curve with its marks. One page for one thing,
/// instead of half of it standing among the buttons.
class TitleBarPage : public SettingsPage
{
public:
    TitleBarPage();

    bool filterElements(const QString &query) override;

private:
    void initTitleBar(GeneralPageView &layout);
    /// Everything about the activity curve in one place
    void initCurve(GeneralPageView &layout);

    QTabWidget *tabs_{};
    GeneralPageView *titleBar_{};
    GeneralPageView *curve_{};
};

}  // namespace chatterino
