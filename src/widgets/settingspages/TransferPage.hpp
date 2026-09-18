// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace chatterino {

/// Moves the whole setup to another computer: exports it to a folder on the
/// desktop, and imports such a folder made on another computer.
class TransferPage : public SettingsPage
{
public:
    TransferPage();

    bool filterElements(const QString &query) override;

private:
    void buildExportTab(QVBoxLayout *layout);
    void buildImportTab(QVBoxLayout *layout);
    void exportNow();
    void importNow();

    QCheckBox *includeLogin_{};
    QLabel *exportStatus_{};
    QPushButton *reveal_{};
    QString lastExport_;
};

}  // namespace chatterino
