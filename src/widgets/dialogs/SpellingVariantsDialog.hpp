// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/SpellingVariants.hpp"
#include "widgets/BasePopup.hpp"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace chatterino {

/// Turns a word into a message highlight that also finds it written
/// differently - f0ll0w3r, fоllower with a Cyrillic о, f.o.l.l.o.w.e.r -
/// with a field to try it out before adding it
class SpellingVariantsDialog : public BasePopup
{
public:
    explicit SpellingVariantsDialog(QWidget *parent = nullptr);

private:
    spelling::Options options() const;
    void refresh();
    void addHighlight();

    QLineEdit *word_{};
    QCheckBox *leet_{};
    QCheckBox *lookalikes_{};
    QCheckBox *stretched_{};
    QCheckBox *separated_{};
    QCheckBox *wholeWord_{};
    QLabel *examples_{};
    QLineEdit *pattern_{};
    QLineEdit *tryOut_{};
    QLabel *result_{};
    QPushButton *add_{};
};

}  // namespace chatterino
