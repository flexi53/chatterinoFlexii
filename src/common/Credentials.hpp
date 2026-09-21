// SPDX-FileCopyrightText: 2019 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace chatterino {

class Credentials
{
public:
    static Credentials &instance();

    void get(const QString &provider, const QString &name, QObject *receiver,
             std::function<void(const QString &)> &&onLoaded);
    void set(const QString &provider, const QString &name,
             const QString &credential);
    void erase(const QString &provider, const QString &name);

    /// ChattiFlexii: whether what is set goes into the system's keychain -
    /// false in the portable version, which keeps it in a file instead
    static bool isSecure();

private:
    Credentials() = default;
};

}  // namespace chatterino
