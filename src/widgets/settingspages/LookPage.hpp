// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

/// Everything about how the app looks that is a choice rather than a detail -
/// currently the classic/modern switch.
class LookPage : public SettingsPage
{
public:
    LookPage();

    bool filterElements(const QString &query) override;

private:
    void initLayout(GeneralPageView &layout);

    GeneralPageView *view{};
};

}  // namespace chatterino
