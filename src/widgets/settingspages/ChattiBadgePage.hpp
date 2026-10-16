// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

namespace chatterino {

/// ChattiFlexii's own badge: a picture of one's choosing next to one's name,
/// for everyone with ChattiFlexii - uploading it, and for admins approving
/// what others uploaded. See providers/chattibadges.
class ChattiBadgePage : public SettingsPage
{
public:
    ChattiBadgePage();

    bool filterElements(const QString &query) override;

private:
    void initOwnBadge(GeneralPageView &layout);

    GeneralPageView *view_{};
};

}  // namespace chatterino
