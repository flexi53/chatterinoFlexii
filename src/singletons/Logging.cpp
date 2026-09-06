// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/Logging.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "messages/Message.hpp"
#include "singletons/helper/LoggingChannel.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>

#include <memory>
#include <utility>

namespace chatterino {

namespace {

/// How long log files are kept. The user card looks 7 days back, so a bit of
/// headroom on top of that is plenty.
constexpr int LOG_RETENTION_DAYS = 14;

}  // namespace

void Logging::cleanUpOldLogs()
{
    auto baseDirectory = getSettings()->logPath.getValue();
    if (baseDirectory.isEmpty())
    {
        baseDirectory = getApp()->getPaths().messageLogDirectory;
    }

    QDir root(baseDirectory);
    if (!root.exists())
    {
        return;
    }

    const auto cutoff = QDate::currentDate().addDays(-LOG_RETENTION_DAYS);

    // Log files are named "<channel>-<yyyy-MM-dd>.log". Only files matching
    // that shape are ever touched, and only below the log directory.
    static const QRegularExpression logFileName(
        QStringLiteral(R"(^.+-(\d{4}-\d{2}-\d{2})\.log$)"));

    int removed = 0;
    QDirIterator it(root.absolutePath(), QStringList{"*.log"}, QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        const auto path = it.next();
        const auto match = logFileName.match(it.fileName());
        if (!match.hasMatch())
        {
            continue;
        }

        const auto date =
            QDate::fromString(match.captured(1), QStringLiteral("yyyy-MM-dd"));
        if (!date.isValid() || date >= cutoff)
        {
            continue;
        }

        if (QFile::remove(path))
        {
            removed++;
        }
    }

    if (removed > 0)
    {
        qCDebug(chatterinoApp) << "Removed" << removed << "log files older than"
                               << LOG_RETENTION_DAYS << "days";
    }
}

Logging::Logging(Settings &settings)
{
    if (settings.enableLogging)
    {
        Logging::cleanUpOldLogs();
    }

    // We can safely ignore this signal connection since settings are only-ever destroyed
    // on application exit
    // NOTE: SETTINGS_LIFETIME
    std::ignore = settings.loggedChannels.delayedItemsChanged.connect(
        [this, &settings]() {
            this->threadGuard.guard();

            this->onlyLogListedChannels.clear();

            for (const auto &loggedChannel :
                 *settings.loggedChannels.readOnly())
            {
                this->onlyLogListedChannels.insert(loggedChannel.channelName());
            }
        });
}

void Logging::addMessage(const QString &channelName, MessagePtr message,
                         const QString &platformName, const QString &streamID)
{
    if (platformName.isEmpty())
    {
        return;
    }

    this->threadGuard.guard();

    if (!getSettings()->enableLogging)
    {
        return;
    }

    if (getSettings()->onlyLogListedChannels)
    {
        if (!this->onlyLogListedChannels.contains(channelName))
        {
            return;
        }
    }

    auto platIt = this->loggingChannels_.find(platformName);
    if (platIt == this->loggingChannels_.end())
    {
        auto *channel = new LoggingChannel(channelName, platformName);
        channel->addMessage(message, streamID);
        auto map = std::map<QString, std::unique_ptr<LoggingChannel>>();
        this->loggingChannels_[platformName] = std::move(map);
        auto &ref = this->loggingChannels_.at(platformName);
        ref.emplace(channelName, channel);
        return;
    }
    auto chanIt = platIt->second.find(channelName);
    if (chanIt == platIt->second.end())
    {
        auto *channel = new LoggingChannel(channelName, platformName);
        channel->addMessage(message, streamID);
        platIt->second.emplace(channelName, channel);
    }
    else
    {
        chanIt->second->addMessage(message, streamID);
    }
}

void Logging::closeChannel(const QString &channelName,
                           const QString &platformName)
{
    if (platformName.isEmpty())
    {
        return;
    }

    auto platIt = this->loggingChannels_.find(platformName);
    if (platIt == this->loggingChannels_.end())
    {
        return;
    }
    platIt->second.erase(channelName);
}

}  // namespace chatterino
