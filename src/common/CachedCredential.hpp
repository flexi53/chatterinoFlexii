// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QPointer>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

class QObject;

namespace chatterino {

/// One entry of the system's keychain, read once per start and then kept in
/// memory - never on disk - until the app ends. Each read can make macOS
/// ask for the password; asked for again and again, by every badge button
/// at once, it would ask again and again.
class CachedCredential
{
public:
    CachedCredential(QString provider, QString name);

    /// Hands the entry to @a done - an empty one when there is none. Asked
    /// for while it is still being read, the one read serves them all.
    void get(QObject *receiver, std::function<void(const QString &)> done);
    void set(const QString &value);
    void erase();

private:
    struct Waiting {
        QPointer<QObject> receiver;
        /// Without a receiver it is always told
        bool guarded;
        std::function<void(const QString &)> done;
    };

    QString provider_;
    QString name_;
    std::optional<QString> value_;
    bool reading_ = false;
    std::vector<Waiting> waiting_;
};

}  // namespace chatterino
