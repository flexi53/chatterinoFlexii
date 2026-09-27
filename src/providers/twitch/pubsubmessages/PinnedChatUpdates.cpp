// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/twitch/pubsubmessages/PinnedChatUpdates.hpp"

#include "util/QMagicEnum.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimeZone>

namespace chatterino {

using namespace Qt::StringLiterals;

namespace {

/// A time as the pin event writes it: seconds or milliseconds since the
/// epoch, or already a date. Empty where it says nothing.
QString asIsoTime(const QJsonValue &value)
{
    if (value.isString())
    {
        const auto text = value.toString();
        if (text.isEmpty())
        {
            return {};
        }
        // Already a date - hand it on as it came
        if (QDateTime::fromString(text, Qt::ISODate).isValid())
        {
            return text;
        }
        // A number in quotation marks
        bool ok = false;
        const auto number = text.toLongLong(&ok);
        if (!ok)
        {
            return {};
        }
        return asIsoTime(QJsonValue(static_cast<double>(number)));
    }

    if (!value.isDouble())
    {
        return {};
    }

    auto number = static_cast<qint64>(value.toDouble());
    if (number <= 0)
    {
        return {};
    }
    // Seconds where the number is small enough to be seconds
    constexpr qint64 SECONDS_UNTIL_THE_YEAR_2300 = 10'000'000'000LL;
    if (number < SECONDS_UNTIL_THE_YEAR_2300)
    {
        number *= 1000;
    }
    return QDateTime::fromMSecsSinceEpoch(number, QTimeZone::UTC)
        .toString(Qt::ISODate);
}

/// The first of @a keys @a object has something under
QJsonValue firstOf(const QJsonObject &object,
                   std::initializer_list<QLatin1String> keys)
{
    for (const auto key : keys)
    {
        const auto value = object.value(key);
        if (!value.isUndefined() && !value.isNull())
        {
            return value;
        }
    }
    return {};
}

/// What the pin event calls a user, written the way Helix does. @a prefix
/// is what the three keys start with, as in "sender_user".
void writeUser(QJsonObject &out, const QString &prefix,
               const QJsonObject &user)
{
    const auto id = firstOf(user, {"id"_L1, "user_id"_L1}).toString();
    auto login = firstOf(user, {"login"_L1, "user_login"_L1}).toString();
    const auto name =
        firstOf(user, {"display_name"_L1, "user_display_name"_L1, "name"_L1})
            .toString();
    if (login.isEmpty())
    {
        // Every display name but a CJK one is the login with capitals
        login = name.toLower();
    }

    out[prefix + "_id"] = id;
    out[prefix + "_login"] = login;
    out[prefix + "_name"] = name.isEmpty() ? login : name;
}

/// What the message says, however the event packs it
QString textOf(const QJsonObject &message)
{
    const auto content = message.value("content"_L1);
    if (content.isObject())
    {
        const auto object = content.toObject();
        const auto text = object.value("text"_L1).toString();
        if (!text.isEmpty())
        {
            return text;
        }
        // Otherwise it is written in pieces
        QString pieces;
        for (const auto &piece : object.value("fragments"_L1).toArray())
        {
            pieces += piece.toObject().value("text"_L1).toString();
        }
        if (!pieces.isEmpty())
        {
            return pieces;
        }
    }
    if (content.isString())
    {
        return content.toString();
    }
    return firstOf(message, {"text"_L1, "body"_L1}).toString();
}

}  // namespace

std::optional<QJsonObject> pinnedMessageAsHelix(const QJsonObject &data)
{
    // The message sits in "message"; where it does not, the payload itself
    // may be the message
    auto message = data.value("message"_L1).toObject();
    if (message.isEmpty())
    {
        message = data;
    }

    const auto text = textOf(message);
    if (text.isEmpty())
    {
        return std::nullopt;
    }

    QJsonObject out;
    writeUser(out, "sender_user", message.value("sender"_L1).toObject());
    writeUser(out, "pinned_by_user",
              firstOf(data, {"pinned_by"_L1, "pinned_by_user"_L1}).toObject());

    out["message_id"] = firstOf(message, {"id"_L1, "message_id"_L1}).toString();
    out["message"] = QJsonObject{{"text", text}};

    auto starts =
        asIsoTime(firstOf(message, {"starts_at"_L1, "sent_at"_L1, "pinned_at"_L1}));
    if (starts.isEmpty())
    {
        starts = asIsoTime(
            firstOf(data, {"starts_at"_L1, "sent_at"_L1, "pinned_at"_L1}));
    }
    if (starts.isEmpty())
    {
        // Without a time the banner would say nothing sensible under it
        starts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    out["starts_at"] = starts;

    auto ends = asIsoTime(firstOf(message, {"ends_at"_L1}));
    if (ends.isEmpty())
    {
        ends = asIsoTime(firstOf(data, {"ends_at"_L1}));
    }
    if (!ends.isEmpty())
    {
        out["ends_at"] = ends;
    }

    if (out["sender_user_name"].toString().isEmpty())
    {
        // Nobody to name as the writer - better ask Helix
        return std::nullopt;
    }

    return out;
}

PubSubPinnedChatUpdatesV1Message::PubSubPinnedChatUpdatesV1Message(
    const QJsonObject &root)
    : typeString(root.value("type").toString())
    , data(root.value("data").toObject())
{
    auto oType = qmagicenum::enumCast<Type>(this->typeString);
    if (oType.has_value())
    {
        this->type = oType.value();
    }
}

}  // namespace chatterino
