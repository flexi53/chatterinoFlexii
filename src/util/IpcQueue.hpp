// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

class QByteArray;
class QString;

namespace chatterino {

class Paths;

}  // namespace chatterino

namespace chatterino::ipc {

void initPaths(const Paths *paths);

void sendMessage(const char *name, const QByteArray &data);

class IpcQueuePrivate;
class IpcQueue
{
public:
    ~IpcQueue();

    static std::pair<std::unique_ptr<IpcQueue>, QString> tryReplaceOrCreate(
        const char *name, size_t maxMessages, size_t maxMessageSize);

    static bool remove(const char *name);

    /// Where the queue of that name lies - a file another start of the
    /// program replaces, which is worth noticing
    static QString path(const char *name);

    // TODO: use std::expected
    /// Try to receive a message.
    /// In the case of an error, the buffer is empty.
    QByteArray receive();

    /// Waits at most @a timeout for a message. Nothing means the time was
    /// up or something went wrong - the caller can then look whether the
    /// queue on disk is still the one in hand.
    std::optional<QByteArray> receiveFor(std::chrono::milliseconds timeout);

private:
    IpcQueue(IpcQueuePrivate *priv);

    std::unique_ptr<IpcQueuePrivate> private_;

    friend class IpcQueuePrivate;
};

}  // namespace chatterino::ipc
