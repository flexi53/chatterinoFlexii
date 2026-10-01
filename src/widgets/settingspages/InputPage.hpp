// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

/// Eingabefeld: the line you write in and the small row of buttons beside
/// it, what is suggested while you type, and what the Enter key does.
class InputPage : public SettingsPage
{
public:
    InputPage();

    bool filterElements(const QString &query) override;

private:
    void initLayout(GeneralPageView &layout);

    GeneralPageView *view_{};
};

}  // namespace chatterino
