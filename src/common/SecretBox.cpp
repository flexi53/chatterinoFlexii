// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "common/SecretBox.hpp"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSysInfo>

namespace chatterino::secretbox {

namespace {

/// What every locked value starts with, so it is clear at a glance that it
/// is not a secret lying there in the clear
const QByteArray MARK = QByteArrayLiteral("cf1:");

constexpr int SALT_BYTES = 16;
constexpr int CHECK_BYTES = 8;

/// The stream the value is folded with: SHA-256 over key, salt and the
/// number of the block, as long as the value needs
QByteArray streamFor(const QByteArray &key, const QByteArray &salt,
                     const int length)
{
    QByteArray stream;
    int block = 0;
    while (stream.size() < length)
    {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(key);
        hash.addData(salt);
        hash.addData(QByteArray::number(block));
        stream.append(hash.result());
        block++;
    }
    stream.truncate(length);
    return stream;
}

QByteArray fold(const QByteArray &data, const QByteArray &stream)
{
    QByteArray folded = data;
    for (int i = 0; i < folded.size(); i++)
    {
        folded[i] = static_cast<char>(folded[i] ^ stream[i]);
    }
    return folded;
}

/// Says whether what came back out is what went in
QByteArray checkOf(const QByteArray &value, const QByteArray &key,
                   const QByteArray &salt)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(key);
    hash.addData(salt);
    hash.addData(value);
    return hash.result().left(CHECK_BYTES);
}

}  // namespace

QByteArray machineKey()
{
    auto id = QSysInfo::machineUniqueId();
    if (id.isEmpty())
    {
        // Nothing to tie it to - then the salt alone has to do
        id = QByteArrayLiteral("chattiflexii");
    }
    return QCryptographicHash::hash(id, QCryptographicHash::Sha256);
}

QByteArray freshSalt()
{
    QByteArray salt(SALT_BYTES, Qt::Uninitialized);
    QRandomGenerator::system()->generate(salt.begin(), salt.end());
    return salt;
}

QString hide(const QString &value, const QByteArray &salt,
             const QByteArray &key)
{
    if (value.isEmpty())
    {
        return {};
    }

    const auto clear = value.toUtf8();
    const auto folded = fold(clear, streamFor(key, salt, clear.size()));
    return QString::fromLatin1(
        MARK + (checkOf(clear, key, salt) + folded).toBase64());
}

QString reveal(const QString &hidden, const QByteArray &salt,
               const QByteArray &key)
{
    if (hidden.isEmpty())
    {
        return {};
    }

    const auto raw = hidden.toLatin1();
    if (!raw.startsWith(MARK))
    {
        // Written before this was locked away - taken as it stands
        return hidden;
    }

    const auto body =
        QByteArray::fromBase64(raw.mid(MARK.size()), QByteArray::AbortOnBase64DecodingErrors);
    if (body.size() <= CHECK_BYTES)
    {
        return {};
    }

    const auto check = body.left(CHECK_BYTES);
    const auto folded = body.mid(CHECK_BYTES);
    const auto clear = fold(folded, streamFor(key, salt, folded.size()));
    if (checkOf(clear, key, salt) != check)
    {
        // Another computer, another salt, or a file someone meddled with
        return {};
    }
    return QString::fromUtf8(clear);
}

}  // namespace chatterino::secretbox
