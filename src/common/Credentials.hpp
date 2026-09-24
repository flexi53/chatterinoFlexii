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

    /// Whether there is anywhere to keep a secret - the keychain, or the
    /// file locked to this computer when that was asked for
    static bool canKeep();

    /// Takes @a name out of the system's keychain, whatever the settings
    /// say - used when the secrets are moved over to the local store
    static void eraseFromKeychain(const QString &provider,
                                  const QString &name);

    /// The same for the store beside the settings
    static void eraseFromLocal(const QString &provider, const QString &name);

private:
    Credentials() = default;
};

}  // namespace chatterino
