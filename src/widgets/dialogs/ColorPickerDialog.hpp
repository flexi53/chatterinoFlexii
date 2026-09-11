// SPDX-FileCopyrightText: 2020 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BasePopup.hpp"

#include <QVBoxLayout>

namespace chatterino {

class ColorPickerDialog : public BasePopup
{
    Q_OBJECT

public:
    ColorPickerDialog(QColor color, QWidget *parent);

    QColor color() const;

Q_SIGNALS:
    void colorChanged(QColor color);
    void colorConfirmed(QColor color);

public Q_SLOTS:
    void setColor(const QColor &color);

private:
    /// Fills the list of named colours from the settings. Called again after
    /// one is added, renamed or dropped.
    void rebuildNamedColors();

    QColor color_;
    QVBoxLayout *namedColors_{};
};

}  // namespace chatterino
