// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "common/CachedCredential.hpp"

#include "common/Credentials.hpp"

#include <QCoreApplication>

#include <utility>

namespace chatterino {

CachedCredential::CachedCredential(QString provider, QString name)
    : provider_(std::move(provider))
    , name_(std::move(name))
{
}

void CachedCredential::get(QObject *receiver,
                           std::function<void(const QString &)> done)
{
    if (this->value_)
    {
        done(*this->value_);
        return;
    }

    this->waiting_.push_back({receiver, receiver != nullptr, std::move(done)});
    if (this->reading_)
    {
        return;
    }
    this->reading_ = true;

    // Read on behalf of the app, not of whoever asked first - they may be
    // gone by the time the keychain answers, the others not
    Credentials::instance().get(
        this->provider_, this->name_, QCoreApplication::instance(),
        [this](const QString &value) {
            // An empty answer can also mean the read did not work - the
            // keychain gives nothing back when it was denied. Kept only
            // when there is something, so a later try can still find it.
            if (!value.isEmpty())
            {
                this->value_ = value;
            }
            this->reading_ = false;
            auto waiting = std::move(this->waiting_);
            this->waiting_.clear();
            for (const auto &entry : waiting)
            {
                if (!entry.guarded || !entry.receiver.isNull())
                {
                    entry.done(value);
                }
            }
        });
}

void CachedCredential::set(const QString &value)
{
    Credentials::instance().set(this->provider_, this->name_, value);
    this->value_ = value;
}

void CachedCredential::erase()
{
    Credentials::instance().erase(this->provider_, this->name_);
    this->value_ = QString();
}

}  // namespace chatterino
