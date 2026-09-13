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
class QTimer;
class QVBoxLayout;

namespace chatterino {

struct ModHighlightChannel;

/// Everything the moderation assistant and its alerts can be told: the alert
/// windows, suggestions, repeated messages, emote spam and mod highlights.
class ModAssistantPage : public SettingsPage
{
public:
    ModAssistantPage();

    bool filterElements(const QString &query) override;
    void onShow() override;

private:
    void buildModHighlights(QVBoxLayout *layout);
    /// Whether the plugin is there, and how many channels and mods are marked
    void showModHighlightsState();
    /// The channels whosthemod.xyz knows, matching the search - the next page
    /// of them with @a more
    void searchModChannels(bool more);
    /// Just the chosen channels, while the site has no channel list
    void showChosenModChannels();
    void fillModChannels(const std::vector<ModHighlightChannel> &channels,
                         bool append, bool chosenFirst);
    /// Checks the typed channel name directly and adds it when it has mods
    void addTypedModChannel();
    void showModChannelPicture(const QString &login, const QString &url);

    QLabel *modPluginNote_{};
    QWidget *modHighlightsBody_{};
    QLineEdit *modSearch_{};
    QLabel *modListNote_{};
    QListWidget *modChannels_{};
    QPushButton *modMore_{};
    QLabel *modStatus_{};
    QTimer *modSearchDelay_{};
    QString modCursor_;
    bool siteListAvailable_ = true;
    bool modChannelsAsked_ = false;
    bool fillingModChannels_ = false;
};

}  // namespace chatterino
