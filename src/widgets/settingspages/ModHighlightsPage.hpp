// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/settingspages/SettingsPage.hpp"

#include <vector>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTimer;
class QVBoxLayout;

namespace chatterino {

struct ModHighlightChannel;

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
    /// Whether the plugin is there, and how many channels and mods are marked
    void showState();
    /// The channels whosthemod.xyz knows, matching the search - the next page
    /// of them with @a more
    void searchChannels(bool more);
    /// Just the chosen channels, while the site has no channel list
    void showChosenChannels();
    void fillChannels(const std::vector<ModHighlightChannel> &channels,
                      bool append, bool chosenFirst);
    /// Checks the typed channel name directly and adds it when it has mods
    void addTypedChannel();
    void showChannelPicture(const QString &login, const QString &url);

    QLabel *pluginNote_{};
    QTabWidget *tabs_{};
    QLineEdit *search_{};
    QLabel *listNote_{};
    QListWidget *channels_{};
    QPushButton *more_{};
    QLabel *status_{};
    QTimer *searchDelay_{};
    QString cursor_;
    bool siteListAvailable_ = true;
    /// Whether the channel list has been asked for yet
    bool asked_ = false;
    bool filling_ = false;
};

}  // namespace chatterino
