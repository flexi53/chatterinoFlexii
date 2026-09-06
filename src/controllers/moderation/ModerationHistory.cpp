// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/moderation/ModerationHistory.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "singletons/Paths.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace chatterino {

namespace {

/// Writing on every action would hit the disk far more often than needed, so
/// saves are throttled to this interval. The destructor flushes whatever is
/// still pending.
constexpr auto SAVE_INTERVAL = std::chrono::seconds{15};

}  // namespace

QString ModerationCounts::toShortString() const
{
    return QStringLiteral("%1/%2/%3")
        .arg(QString::number(this->warnings), QString::number(this->timeouts),
             QString::number(this->bans));
}

ModerationHistory::ModerationHistory()
    : lastSave_(std::chrono::steady_clock::now())
{
    this->load();
}

ModerationHistory::~ModerationHistory()
{
    this->save();
}

QString ModerationHistory::filePath()
{
    return getApp()->getPaths().miscDirectory + QDir::separator() +
           "moderation-history.json";
}

void ModerationHistory::load()
{
    QFile file(ModerationHistory::filePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return;  // nothing recorded yet
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
    {
        qCWarning(chatterinoApp) << "Moderation history file is malformed";
        return;
    }

    std::unique_lock lock(this->mutex_);

    const auto root = document.object();
    for (auto channelIt = root.begin(); channelIt != root.end(); ++channelIt)
    {
        const auto users = channelIt.value().toObject();
        auto &channelData = this->data_[channelIt.key()];

        for (auto userIt = users.begin(); userIt != users.end(); ++userIt)
        {
            const auto entry = userIt.value().toObject();
            channelData[userIt.key()] = ModerationCounts{
                .warnings = entry.value("w").toInt(),
                .timeouts = entry.value("t").toInt(),
                .bans = entry.value("b").toInt(),
            };
        }
    }
}

void ModerationHistory::save()
{
    QJsonObject root;

    {
        std::shared_lock lock(this->mutex_);

        if (!this->dirty_)
        {
            return;
        }

        for (const auto &[channelID, users] : this->data_)
        {
            QJsonObject channelObject;
            for (const auto &[userID, counts] : users)
            {
                if (counts.isEmpty())
                {
                    continue;
                }

                channelObject[userID] = QJsonObject{
                    {"w", counts.warnings},
                    {"t", counts.timeouts},
                    {"b", counts.bans},
                };
            }

            if (!channelObject.isEmpty())
            {
                root[channelID] = channelObject;
            }
        }
    }

    QSaveFile file(ModerationHistory::filePath());
    if (!file.open(QIODevice::WriteOnly))
    {
        qCWarning(chatterinoApp)
            << "Failed to write moderation history:" << file.errorString();
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit())
    {
        qCWarning(chatterinoApp)
            << "Failed to commit moderation history:" << file.errorString();
        return;
    }

    std::unique_lock lock(this->mutex_);
    this->dirty_ = false;
    this->lastSave_ = std::chrono::steady_clock::now();
}

void ModerationHistory::record(const QString &channelID, const QString &userID,
                               Action action)
{
    if (channelID.isEmpty() || userID.isEmpty())
    {
        return;
    }

    bool shouldSave = false;

    {
        std::unique_lock lock(this->mutex_);

        auto &counts = this->data_[channelID][userID];
        switch (action)
        {
            case Action::Warning:
                counts.warnings++;
                break;
            case Action::Timeout:
                counts.timeouts++;
                break;
            case Action::Ban:
                counts.bans++;
                break;
        }

        this->dirty_ = true;
        shouldSave =
            std::chrono::steady_clock::now() - this->lastSave_ > SAVE_INTERVAL;
    }

    if (shouldSave)
    {
        this->save();
    }
}

ModerationCounts ModerationHistory::counts(const QString &channelID,
                                           const QString &userID) const
{
    std::shared_lock lock(this->mutex_);

    auto channelIt = this->data_.find(channelID);
    if (channelIt == this->data_.end())
    {
        return {};
    }

    auto userIt = channelIt->second.find(userID);
    if (userIt == channelIt->second.end())
    {
        return {};
    }

    return userIt->second;
}

}  // namespace chatterino
