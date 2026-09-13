// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

/// Everything the moderation assistant and its alerts can be told: the alert
/// windows, suggestions, repeated messages and emote spam.
class ModAssistantPage : public SettingsPage
{
public:
    ModAssistantPage();

    bool filterElements(const QString &query) override;
};

}  // namespace chatterino
