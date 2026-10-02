// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

class QVBoxLayout;

namespace chatterino {

/// WhosTheMod: who became a moderator of the chosen channels, and who is no
/// longer one. The plugin of that name keeps the lists; this page says
/// whether to watch them, how loudly, and opens the tab that collects what
/// changed.
class ModChangesPage : public SettingsPage
{
public:
    ModChangesPage();

    bool filterElements(const QString &query) override;
};

}  // namespace chatterino
