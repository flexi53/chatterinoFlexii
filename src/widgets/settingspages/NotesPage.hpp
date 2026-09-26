// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

#include <QHash>
#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QVBoxLayout;

namespace chatterino {

/// Notizen: what you noted about people - written in their card, kept on this
/// computer, and here all in one place to read, change and throw away.
class NotesPage : public SettingsPage
{
public:
    NotesPage();

    bool filterElements(const QString &query) override;
    void onShow() override;

private:
    void buildUsersTab(QVBoxLayout *layout);
    /// Messages kept with "Merken" in the right-click menu
    void buildSavedTab(QVBoxLayout *layout);
    /// How many are kept, under the buttons of the saved tab
    void showKept();
    /// Fills the list from what is noted, narrowed by what is typed
    void showNotes();
    /// Looks up the names of the ids not known yet, in one request
    void fetchNames();
    /// Opens the same window the user card opens, for the user of @a userId
    void edit(const QString &userId);

    QTabWidget *tabs_{};
    QLineEdit *search_{};
    QListWidget *notes_{};
    QLabel *status_{};
    QLabel *keptStatus_{};
    QPushButton *editButton_{};
    QPushButton *deleteButton_{};
    /// Twitch id -> the name to show, as far as it is known
    QHash<QString, QString> names_;
    bool asking_ = false;
};

}  // namespace chatterino
