// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/RapidjsonHelpers.hpp"
#include "util/RapidJsonSerializeQString.hpp"

#include <pajlada/serialize.hpp>
#include <QColor>
#include <QString>

namespace chatterino {

/// A colour the user has put a name to, so the picker can say what they keep
/// it around for - "Mods", "Regulars" - instead of showing a wall of swatches
/// that all look alike.
class NamedColor
{
public:
    NamedColor() = default;
    NamedColor(QString name, QColor color)
        : name_(std::move(name))
        , color_(color)
    {
    }

    [[nodiscard]] const QString &name() const
    {
        return this->name_;
    }

    [[nodiscard]] const QColor &color() const
    {
        return this->color_;
    }

private:
    QString name_;
    QColor color_;
};

}  // namespace chatterino

namespace pajlada {

template <>
struct Serialize<chatterino::NamedColor> {
    static rapidjson::Value get(const chatterino::NamedColor &value,
                                rapidjson::Document::AllocatorType &a)
    {
        rapidjson::Value ret(rapidjson::kObjectType);

        chatterino::rj::set(ret, "name", value.name(), a);
        chatterino::rj::set(ret, "color", value.color().name(QColor::HexArgb),
                            a);

        return ret;
    }
};

template <>
struct Deserialize<chatterino::NamedColor> {
    static chatterino::NamedColor get(const rapidjson::Value &value,
                                      bool *error = nullptr)
    {
        if (!value.IsObject())
        {
            PAJLADA_REPORT_ERROR(error)
            return {};
        }

        QString name;
        QString color;

        chatterino::rj::getSafe(value, "name", name);
        chatterino::rj::getSafe(value, "color", color);

        return {name, QColor(color)};
    }
};

}  // namespace pajlada
