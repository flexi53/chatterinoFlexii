// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

#include <QStringList>

class QLabel;
class QLineEdit;
class QListWidget;
class QTabWidget;
class QVBoxLayout;

namespace chatterino {

/// Mod highlights: the channels whose moderators get those channels' pictures,
/// how their messages are marked, and the bots left out. Needs the
/// WhoseTheMod plugin, and says so while it is off.
class ModHighlightsPage : public SettingsPage
{
public:
    ModHighlightsPage();

    bool filterElements(const QString &query) override;
    void onShow() override;

private:
    void buildGeneralTab(QVBoxLayout *layout);
    void buildBotsTab(QVBoxLayout *layout);
    /// A tab that says who became a mod of the chosen channels, or went
    void buildChangesTab(QVBoxLayout *layout);
    /// Whether the plugin is there, and how many channels and mods are marked
    void showState();
    /// The chosen channels - and those let go while the page is open, so an
    /// untick by mistake can be undone - narrowed down by what is typed
    void showChannels();
    /// Checks the typed channel name and adds it when it has mods
    void addTypedChannel();

    QLabel *pluginNote_{};
    QTabWidget *tabs_{};
    QLineEdit *search_{};
    QLabel *listNote_{};
    QListWidget *channels_{};
    QLabel *status_{};
    /// Unticked while the page is open, and kept in the list to tick again
    QStringList letGo_;
    bool filling_ = false;
};

}  // namespace chatterino
