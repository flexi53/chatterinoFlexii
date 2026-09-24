// SPDX-FileCopyrightText: 2019 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "common/Credentials.hpp"

#include "Application.hpp"
#include "common/Modes.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/CombinePath.hpp"
#include "common/SecretBox.hpp"
#include "util/Variant.hpp"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringBuilder>

#include <variant>

#ifndef NO_QTKEYCHAIN
#    ifdef CMAKE_BUILD
#        if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#            include "qt6keychain/keychain.h"
#        else
#            include "qt5keychain/keychain.h"
#        endif
#    else
#        include <qtkeychain/keychain.h>
#    endif
#endif

namespace {

using namespace chatterino;

QString formatName(const QString &provider, const QString &name)
{
    assert(!provider.contains(":"));
    return u"chatterino:" % provider % u':' % name;
}

bool useKeyring()
{
#ifdef NO_QTKEYCHAIN
    return false;
#endif
    if (Modes::instance().isPortable)
    {
        return false;
    }
    // ChattiFlexii: Badges -> "ohne Schlüsselbund". Then the secrets lie
    // beside the settings, locked to this computer, and macOS never asks
    // for the password again.
    if (getSettings()->keepSecretsLocally)
    {
        return false;
    }

#ifdef Q_OS_LINUX
    return getSettings()->useKeyring;
#else
    return true;
#endif
}

}  // namespace

namespace chatterino {

bool Credentials::isSecure()
{
    return useKeyring();
}

bool Credentials::canKeep()
{
    // Either the system's keychain, or the file beside the settings that is
    // locked to this computer - but never a portable copy, which may well
    // sit on a stick
    return useKeyring() || (getSettings()->keepSecretsLocally &&
                            !Modes::instance().isPortable);
}

}  // namespace chatterino

namespace {

// Insecure storage:
QString insecurePath()
{
    return combinePath(getApp()->getPaths().settingsDirectory,
                       "credentials.json");
}

QJsonDocument loadInsecure()
{
    QFile file(insecurePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    return QJsonDocument::fromJson(file.readAll());
}

void storeInsecure(const QJsonDocument &doc)
{
    QSaveFile file(insecurePath());
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    // ChattiFlexii: nobody else on this computer gets to read it
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(doc.toJson());
    file.commit();
}

/// ChattiFlexii: the salt this file's secrets are locked with - made once
/// and kept in the file itself
QByteArray saltOf(QJsonDocument &doc)
{
    auto object = doc.object();
    const auto kept =
        QByteArray::fromBase64(object.value("salt").toString().toLatin1());
    if (kept.size() >= 8)
    {
        return kept;
    }

    const auto salt = secretbox::freshSalt();
    object["salt"] = QString::fromLatin1(salt.toBase64());
    doc.setObject(object);
    return salt;
}

QJsonDocument &insecureInstance()
{
    static auto store = loadInsecure();
    return store;
}

void queueInsecureSave()
{
    static bool isQueued = false;

    if (!isQueued)
    {
        isQueued = true;
        QTimer::singleShot(200, QApplication::instance(), [] {
            storeInsecure(insecureInstance());
            isQueued = false;
        });
    }
}

// QKeychain runs jobs asyncronously, so we have to assure that set/erase
// jobs gets executed in order.
struct SetJob {
    QString name;
    QString credential;
};

struct EraseJob {
    QString name;
};

using Job = std::variant<SetJob, EraseJob>;

std::queue<Job> &jobQueue()
{
    static std::queue<Job> jobs;
    return jobs;
}

void runNextJob()
{
#ifndef NO_QTKEYCHAIN
    auto &&queue = jobQueue();

    if (!queue.empty())
    {
        // we were gonna use std::visit here but macos is shit

        auto &&item = queue.front();

        std::visit(
            variant::Overloaded{
                [](const SetJob &set) {
                    auto *job = new QKeychain::WritePasswordJob("chatterino");
                    job->setAutoDelete(true);
                    job->setKey(set.name);
                    job->setTextData(set.credential);
                    QObject::connect(job, &QKeychain::Job::finished,
                                     QApplication::instance(), [](auto) {
                                         runNextJob();
                                     });
                    job->start();
                },
                [](const EraseJob &erase) {
                    auto *job = new QKeychain::DeletePasswordJob("chatterino");
                    job->setAutoDelete(true);
                    job->setKey(erase.name);
                    QObject::connect(job, &QKeychain::Job::finished,
                                     QApplication::instance(), [](auto) {
                                         runNextJob();
                                     });
                    job->start();
                },
            },
            item);

        queue.pop();
    }
#endif
}

void queueJob(Job &&job)
{
    auto &&queue = jobQueue();

    queue.push(std::move(job));
    if (queue.size() == 1)
    {
        runNextJob();
    }
}

}  // namespace

namespace chatterino {

Credentials &Credentials::instance()
{
    static Credentials creds;
    return creds;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Credentials::get(const QString &provider, const QString &name_,
                      QObject *receiver,
                      std::function<void(const QString &)> &&onLoaded)
{
    assertInGuiThread();

    auto name = formatName(provider, name_);

    if (useKeyring())
    {
#ifndef NO_QTKEYCHAIN
        // if NO_QTKEYCHAIN is set, then this code is never used either way
        auto *job = new QKeychain::ReadPasswordJob("chatterino");
        job->setAutoDelete(true);
        job->setKey(name);
        QObject::connect(
            job, &QKeychain::Job::finished, receiver,
            [job, onLoaded = std::move(onLoaded)](auto) mutable {
                onLoaded(job->textData());
            },
            Qt::DirectConnection);
        job->start();
#endif
    }
    else
    {
        auto &instance = insecureInstance();

        onLoaded(secretbox::reveal(instance[name].toString(),
                                   saltOf(instance), secretbox::machineKey()));
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Credentials::set(const QString &provider, const QString &name_,
                      const QString &credential)
{
    assertInGuiThread();

    /// On linux, we try to use a keychain but show a message to disable it when it fails.
    /// XXX: add said message

    auto name = formatName(provider, name_);

    if (useKeyring())
    {
        queueJob(SetJob{name, credential});
    }
    else
    {
        auto &instance = insecureInstance();
        const auto salt = saltOf(instance);

        auto obj = instance.object();
        obj[name] =
            secretbox::hide(credential, salt, secretbox::machineKey());
        instance.setObject(obj);

        queueInsecureSave();
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Credentials::erase(const QString &provider, const QString &name_)
{
    assertInGuiThread();

    auto name = formatName(provider, name_);

    if (useKeyring())
    {
        queueJob(EraseJob{name});
    }
    else
    {
        auto &instance = insecureInstance();

        if (auto it = instance.object().find(name);
            it != instance.object().end())
        {
            instance.object().erase(it);
        }

        queueInsecureSave();
    }
}

}  // namespace chatterino
