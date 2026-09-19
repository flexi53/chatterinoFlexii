// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QVBoxLayout;

namespace chatterino {

/// Moves the whole setup to another computer: exports it to a folder on the
/// desktop, and imports such a folder made on another computer. Also makes
/// backups by itself, and keeps two computers alike.
class TransferPage : public SettingsPage
{
public:
    TransferPage();

    bool filterElements(const QString &query) override;

private:
    void buildExportTab(QVBoxLayout *layout);
    void buildImportTab(QVBoxLayout *layout);
    void buildBackupTab(QVBoxLayout *layout);
    void buildSyncTab(QVBoxLayout *layout);
    void buildViewsTab(QVBoxLayout *layout);
    void showViews();
    void showBackupState();
    void showSyncState();
    void exportNow();
    void importNow();

    QCheckBox *includeLogin_{};
    QLabel *exportStatus_{};
    QPushButton *reveal_{};
    QString lastExport_;
    QLabel *backupFolder_{};
    QLabel *backupStatus_{};
    QLabel *syncState_{};
    QListWidget *views_{};
    QLabel *syncStatus_{};
};

}  // namespace chatterino
