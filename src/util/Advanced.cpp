// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/Advanced.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/TwitchAccount.hpp"

#include <optional>

namespace chatterino::advanced {

namespace {

std::optional<bool> &pretend()
{
    static std::optional<bool> pretending;
    return pretending;
}

}  // namespace

QString owner()
{
    return QStringLiteral("fx_flexii");
}

bool unlocked()
{
    if (pretend())
    {
        return *pretend();
    }

    auto *app = tryGetApp();
    if (app == nullptr)
    {
        return false;
    }
    // A program without accounts is one of the tests, or one on its way out
    auto *accounts = app->getAccounts();
    if (accounts == nullptr)
    {
        return false;
    }
    auto account = accounts->twitch.getCurrent();
    if (account == nullptr || account->isAnon())
    {
        return false;
    }
    return account->getUserName().compare(owner(), Qt::CaseInsensitive) == 0;
}

void pretendUnlocked(bool unlocked)
{
    pretend() = unlocked;
}

void stopPretending()
{
    pretend().reset();
}

}  // namespace chatterino::advanced
