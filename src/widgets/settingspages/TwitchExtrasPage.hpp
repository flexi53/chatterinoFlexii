// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

#include <QTimer>

class QLabel;
class QPushButton;

namespace chatterino {

/// What twitch.tv lets you do beyond chatting, the way Moltorino brings it:
/// voting and betting in the banner, starting and resolving polls and
/// predictions as a moderator, and channel points. All of it needs a login
/// of its own as Twitch's TV app, which is made here.
class TwitchExtrasPage : public SettingsPage
{
public:
    TwitchExtrasPage();

    bool filterElements(const QString &query) override;

private:
    void initLogin(GeneralPageView &layout);
    void initBanner(GeneralPageView &layout);
    void initWindow(GeneralPageView &layout);

    void showLoginState();
    void startLogin();
    void waitForLogin(const QString &deviceCode, int intervalSeconds);

    GeneralPageView *view_{};
    QLabel *loginState_{};
    QPushButton *loginButton_{};
    QPushButton *logoutButton_{};
    QTimer loginPoll_;
    bool waiting_ = false;
};

}  // namespace chatterino
