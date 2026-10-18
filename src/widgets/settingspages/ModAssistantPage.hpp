// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

/// Mod-Alarme: what the alert windows can be told - all of them, then
/// repeated messages, emote spam and words, each with its channels. Built
/// with Part::Suggestions it is instead the suggestions of the moderation
/// assistant, a tab of Automatik.
class ModAssistantPage : public SettingsPage
{
public:
    enum class Part {
        Alerts,
        Suggestions,
    };

    explicit ModAssistantPage(Part part = Part::Alerts);

    bool filterElements(const QString &query) override;
};

}  // namespace chatterino
