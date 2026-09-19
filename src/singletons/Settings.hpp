// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/ChatterinoSetting.hpp"
#include "common/enums/MessageOverflow.hpp"
#include "common/LastMessageLineStyle.hpp"
#include "common/Modes.hpp"
#include "common/SignalVector.hpp"
#include "common/StreamerModeSetting.hpp"
#include "common/ThumbnailPreviewMode.hpp"
#include "common/TimeoutStackStyle.hpp"
#include "controllers/filters/FilterRecord.hpp"
#include "controllers/highlights/HighlightBadge.hpp"
#include "controllers/highlights/HighlightBlacklistUser.hpp"
#include "controllers/highlights/HighlightPhrase.hpp"
#include "controllers/ignores/IgnorePhrase.hpp"
#include "controllers/logging/ChannelLog.hpp"
#include "controllers/moderationactions/ModerationAction.hpp"
#include "controllers/nicknames/Nickname.hpp"
#include "providers/colors/NamedColor.hpp"
#include "controllers/sound/ISoundController.hpp"
#include "providers/emoji/EmojiStyle.hpp"
#include "singletons/Toasts.hpp"
#include "util/RapidJsonSerializeQString.hpp"  // IWYU pragma: keep
#include "widgets/NotebookEnums.hpp"

#include <pajlada/settings/setting.hpp>
#include <pajlada/settings/settinglistener.hpp>
#include <pajlada/settings/settingmanager.hpp>
#include <pajlada/signals/signalholder.hpp>

#include <optional>
#include <string_view>

using TimeoutButton = std::pair<QString, int>;

namespace chatterino {

class Args;

#ifdef Q_OS_WIN32
#    define DEFAULT_FONT_FAMILY "Segoe UI"
#    define DEFAULT_FONT_SIZE 10
#else
#    ifdef Q_OS_MACOS
#        define DEFAULT_FONT_FAMILY "Helvetica Neue"
#        define DEFAULT_FONT_SIZE 12
#    else
#        define DEFAULT_FONT_FAMILY "Arial"
#        define DEFAULT_FONT_SIZE 11
#    endif
#endif

void _actuallyRegisterSetting(
    std::weak_ptr<pajlada::Settings::SettingData> setting);

enum UsernameDisplayMode : int {
    Username = 1,                  // Username
    LocalizedName = 2,             // Localized name
    UsernameAndLocalizedName = 3,  // Username (Localized name)
};

enum UsernameRightClickBehavior : int {
    Reply = 0,
    Mention = 1,
    Ignore = 2,
};

enum class ChatSendProtocol : int {
    Default = 0,
    IRC = 1,
    Helix = 2,
};

enum class ShowModerationState : int {
    // Always show this moderation-related item
    Always = 0,
    // Never show this moderation-related item
    Never = 1,
};

enum class StreamLinkPreferredQuality : std::uint8_t {
    Choose,
    Source,
    High,
    Medium,
    Low,
    AudioOnly,
};

enum class TabStyle : std::uint8_t {
    Normal,
    Compact,
};

/// Which look the chat uses. Classic is Chatterino as it has always been;
/// Modern rounds things off more and gives surfaces a bit of depth.
enum class UiStyle : std::uint8_t {
    Classic,
    Modern,
};

enum class EmoteTooltipScale : std::uint8_t {
    Small,
    Medium,
    Large,
    Huge,
};

constexpr std::optional<std::string_view> qmagicenumDisplayName(
    EmoteTooltipScale value) noexcept
{
    switch (value)
    {
        case EmoteTooltipScale::Medium:
            return "Medium (default)";

        case EmoteTooltipScale::Small:
        case EmoteTooltipScale::Large:
        case EmoteTooltipScale::Huge:
            return {};
    }
}

/// Settings which are available for reading and writing on the gui thread.
// These settings are still accessed concurrently in the code but it is bad practice.
class Settings
{
    static Settings *instance_;
    Settings *prevInstance_ = nullptr;

    bool disableSaving;

public:
    Settings(const Args &args, const QString &settingsDirectory,
             bool isTest = false);
    ~Settings();

    static Settings &instance();

    /// Request the settings to be saved to file
    ///
    /// Depending on the launch options, a save might end up not happening
    ///
    /// Returns the result from the save, or Skipped if disableSave has been called
    pajlada::Settings::SettingManager::SaveResult requestSave() const;

    void saveSnapshot();
    void restoreSnapshot();

    void disableSave();

    /// Returns true if chat messages should be sent over Helix
    bool shouldSendHelixChat() const;

    FloatSetting uiScale = {"/appearance/uiScale2", 1};
    BoolSetting windowTopMost = {"/appearance/windowAlwaysOnTop", false};

    float getClampedUiScale() const;
    void setClampedUiScale(float value);

    /// Appearance
    BoolSetting showTimestamps = {"/appearance/messages/showTimestamps", true};
    BoolSetting animationsWhenFocused = {
        "/appearance/enableAnimationsWhenFocused", false};
    QStringSetting timestampFormat = {"/appearance/messages/timestampFormat",
                                      "h:mm"};
    BoolSetting showLastMessageIndicator = {
        "/appearance/messages/showLastMessageIndicator", false};
    EnumSetting<LastMessageLineStyle> lastMessagePattern = {
        "/appearance/messages/lastMessagePattern",
        LastMessageLineStyle::Solid,
    };
    QStringSetting lastMessageColor = {"/appearance/messages/lastMessageColor",
                                       "#7f2026"};
    BoolSetting showEmptyInput = {"/appearance/showEmptyInputBox", true};
    BoolSetting showMessageLength = {"/appearance/messages/showMessageLength",
                                     false};
    /// ChattiFlexii: a bar under the input running out while slow mode or a
    /// timeout keeps you from sending, as Twitch shows it
    BoolSetting slowModeBar = {"/appearance/slowModeBar", false};
    BoolSetting showSendWaitTimer = {"/appearance/messages/showSendWaitTimer",
                                     false};
    EnumSetting<MessageOverflow> messageOverflow = {
        "/appearance/messages/messageOverflow", MessageOverflow::Highlight};
    BoolSetting separateMessages = {"/appearance/messages/separateMessages",
                                    false};
    BoolSetting fadeMessageHistory = {"/appearance/messages/fadeMessageHistory",
                                      true};
    BoolSetting hideModerated = {"/appearance/messages/hideModerated", false};
    BoolSetting hideModerationActions = {
        "/appearance/messages/hideModerationActions", false};
    BoolSetting hideDeletionActions = {
        "/appearance/messages/hideDeletionActions", false};
    BoolSetting colorizeNicknames = {"/appearance/messages/colorizeNicknames",
                                     true};
    EnumSetting<UsernameDisplayMode> usernameDisplayMode = {
        "/appearance/messages/usernameDisplayMode",
        UsernameDisplayMode::UsernameAndLocalizedName};

    EnumSetting<NotebookTabLocation> tabDirection = {"/appearance/tabDirection",
                                                     NotebookTabLocation::Top};
    EnumSetting<NotebookTabVisibility> tabVisibility = {
        "/appearance/tabVisibility",
        NotebookTabVisibility::AllTabs,
    };

    //    BoolSetting collapseLongMessages =
    //    {"/appearance/messages/collapseLongMessages", false};
    QStringSetting chatFontFamily{
        "/appearance/currentFontFamily",
        DEFAULT_FONT_FAMILY,
    };
    IntSetting chatFontSize{
        "/appearance/currentFontSize",
        DEFAULT_FONT_SIZE,
    };
    IntSetting chatFontWeight = {
        "/appearance/currentFontWeight",
        QFont::Normal,
    };
    BoolSetting hideReplyContext = {"/appearance/hideReplyContext", false};
    BoolSetting showReplyButton = {"/appearance/showReplyButton", false};
    BoolSetting stripReplyMention = {"/appearance/stripReplyMention", true};
    IntSetting collpseMessagesMinLines = {
        "/appearance/messages/collapseMessagesMinLines", 0};
    BoolSetting alternateMessages = {
        "/appearance/messages/alternateMessageBackground", false};
    /// How far every other message stands out, in percent - 0 is the
    /// theme's own shade (see alternatebg::STRENGTHS)
    IntSetting alternateMessageStrength = {
        "/appearance/messages/alternateStrength", 0};
    /// The colour every other message is tinted with - empty for lighter on
    /// a dark theme and darker on a light one
    QStringSetting alternateMessageTint = {"/appearance/messages/alternateTint",
                                           ""};
    /// Change the background only when someone else writes, so what one
    /// chatter writes in a row reads as one block
    BoolSetting alternateMessagesBySender = {
        "/appearance/messages/alternateBySender", false};

    // ChattiFlexii, Look: everything below starts out off or at Chatterino's
    // own look, and each part of the page can go back to that
    /// A stripe at the left edge of messages from the broadcaster,
    /// moderators, VIPs and subscribers - a role without a colour has none
    BoolSetting roleStripes = {"/appearance/roleStripes/enabled", false};
    QStringSetting roleStripeBroadcaster = {
        "/appearance/roleStripes/broadcaster", "#e91916"};
    QStringSetting roleStripeModerator = {"/appearance/roleStripes/moderator",
                                          "#00ad03"};
    QStringSetting roleStripeVip = {"/appearance/roleStripes/vip", "#e005b9"};
    QStringSetting roleStripeSubscriber = {"/appearance/roleStripes/subscriber",
                                           ""};
    /// Extra room around each message, in pixels at 100 %
    IntSetting messageSpacing = {"/appearance/messages/spacing", 0};
    /// Light up the message under the mouse; empty colour for a faint one
    /// that suits the theme
    BoolSetting hoverHighlight = {"/appearance/messages/hoverHighlight", false};
    QStringSetting hoverHighlightColor = {
        "/appearance/messages/hoverHighlightColor", ""};
    /// New messages fade in rather than appear at once
    BoolSetting fadeInMessages = {"/appearance/messages/fadeIn", false};
    /// The channel's profile picture in front of a tab's name
    BoolSetting tabProfilePictures = {"/appearance/tabs/profilePictures",
                                      false};
    /// A ring around a tab's picture while the channel is live, in place of
    /// the dot
    BoolSetting tabLiveRing = {"/appearance/tabs/liveRing", false};
    /// The channel's picture and the cover of what it streams in the split
    /// header
    BoolSetting splitHeaderPictures = {"/appearance/splitheader/pictures",
                                       false};
    /// How lively the chat was over the last minutes, as a small curve in the
    /// split header
    BoolSetting splitHeaderActivity = {"/appearance/splitheader/activity",
                                       false};
    /// Subs, raids, bans and the like marked with a symbol and a colour
    BoolSetting eventSymbols = {"/appearance/messages/eventSymbols", false};
    /// A message mentioning you lights up briefly as it comes in
    BoolSetting pulseMentions = {"/appearance/messages/pulseMentions", false};
    /// The chatter's round profile picture in front of their name
    BoolSetting chatAvatars = {"/appearance/messages/avatars", false};
    /// A border around the split being typed in, when a tab has several
    BoolSetting activeSplitBorder = {"/appearance/splitheader/activeBorder",
                                     false};
    QStringSetting activeSplitBorderColor = {
        "/appearance/splitheader/activeBorderColor", "#e91916"};
    /// Which saved view of the settings was last loaded - see Ansichten
    QStringSetting currentSnapshot = {"/snapshots/current", ""};
    /// Own colours over the theme's; each empty one keeps the theme's
    BoolSetting customColors = {"/appearance/customColors/enabled", false};
    QStringSetting customColorBackground = {
        "/appearance/customColors/background", ""};
    QStringSetting customColorText = {"/appearance/customColors/text", ""};
    QStringSetting customColorSystemText = {
        "/appearance/customColors/systemText", ""};
    QStringSetting customColorLink = {"/appearance/customColors/link", ""};
    QStringSetting customColorAccent = {"/appearance/customColors/accent", ""};
    QStringSetting customColorHeader = {"/appearance/customColors/header", ""};
    QStringSetting customColorInput = {"/appearance/customColors/input", ""};
    FloatSetting boldScale = {"/appearance/boldScale", 63};
    BoolSetting showTabCloseButton = {"/appearance/showTabCloseButton", true};
    /// Start every tab group on a row of its own, even when the group
    /// does not fill the row.
    BoolSetting tabGroupsOnOwnRow = {"/appearance/tabGroupsOnOwnRow", false};
    BoolSetting showTabLive = {"/appearance/showTabLiveButton", true};
    EnumStringSetting<TabStyle> tabStyle = {
        "/appearance/tabStyle",
        TabStyle::Normal,
    };
    EnumStringSetting<UiStyle> uiStyle = {
        "/appearance/uiStyle",
        UiStyle::Classic,
    };

    /// Tab colours picked on the Look page. They apply whichever look is
    /// chosen. An empty value leaves the colour to the theme.
    QStringSetting tabBackgroundColor = {"/appearance/tabs/backgroundColor",
                                         ""};
    QStringSetting tabSelectedBackgroundColor = {
        "/appearance/tabs/selectedBackgroundColor", ""};
    BoolSetting tabGradient = {"/appearance/tabs/gradient", false};
    QStringSetting tabGradientTopColor = {"/appearance/tabs/gradientTop",
                                          "#3c3c3c"};
    QStringSetting tabGradientBottomColor = {
        "/appearance/tabs/gradientBottom", "#1c1c1c"};

    /// The empty space of the tab bar around the tabs - what shows behind
    /// them, not the tabs themselves. Empty leaves the window's background.
    QStringSetting tabBarBackgroundColor = {
        "/appearance/tabBar/backgroundColor", ""};
    BoolSetting tabBarGradient = {"/appearance/tabBar/gradient", false};
    QStringSetting tabBarGradientTopColor = {"/appearance/tabBar/gradientTop",
                                             "#2a2a2a"};
    QStringSetting tabBarGradientBottomColor = {
        "/appearance/tabBar/gradientBottom", "#121212"};

    /// Channels the moderation assistant runs in, as "channel:learn" or
    /// "channel:suggest" separated by commas
    QStringSetting modAssistModes = {"/moderation/assistant/modes", ""};
    /// Suggestions start once a channel has collected this many cases
    IntSetting modAssistMinCases = {"/moderation/assistant/minCases", 50};
    /// ...and only when at least this many of them resemble the message
    IntSetting modAssistMinSimilar = {"/moderation/assistant/minSimilar", 3};
    /// How alike two messages have to be to count as similar, in percent
    IntSetting modAssistSimilarity = {"/moderation/assistant/similarity", 60};
    /// Channels that raise an alert when a chatter repeats the same message,
    /// separated by commas
    QStringSetting repeatAlertChannels = {"/moderation/repeatAlert/channels",
                                          ""};
    /// The timeouts the repeated message alert offers, one step further each
    /// time the chatter carries on after serving one
    QStringSetting repeatAlertSteps = {"/moderation/repeatAlert/steps",
                                       "30s, 1m, 5m, 10m, 30m"};
    /// How alike two messages have to be to count as the same one, in percent.
    /// 100 only takes identical ones.
    IntSetting repeatAlertSimilarity = {"/moderation/repeatAlert/similarity",
                                        65};
    /// Seconds before the alert window closes by itself, 0 for never
    IntSetting repeatAlertAutoClose = {"/moderation/repeatAlert/autoClose",
                                       15};
    /// Channels that raise an alert for messages made only of emotes,
    /// separated by commas
    QStringSetting emoteAlertChannels = {"/moderation/emoteAlert/channels",
                                         ""};
    /// How many emotes within the counting window raise the emote alert
    IntSetting emoteAlertMinEmotes = {"/moderation/emoteAlert/minEmotes", 8};
    /// Seconds over which the emote alert adds up a chatter's emotes
    IntSetting emoteAlertWindowSeconds = {
        "/moderation/emoteAlert/windowSeconds", 60};
    /// Keeps alert windows above every other program, so one that comes up
    /// while something else is in front does not open unseen behind it
    BoolSetting modAlertAlwaysOnTop = {"/moderation/alerts/alwaysOnTop", true};
    /// Plays the ping when a new alert window comes up
    BoolSetting modAlertSound = {"/moderation/alerts/sound", false};
    /// The sound each kind of alert plays: empty for the ping everything else
    /// plays, "builtin:<name>" for one that comes with the app, or a file
    QStringSetting modAlertSoundRepeat = {"/moderation/alerts/soundRepeat",
                                          "builtin:zweiton"};
    QStringSetting modAlertSoundEmote = {"/moderation/alerts/soundEmote",
                                         "builtin:dringend"};
    QStringSetting modAlertSoundSuggestion = {
        "/moderation/alerts/soundSuggestion", "builtin:glocke"};
    /// The size alert windows open with, as the moderator last dragged one
    /// to. 0 leaves that side to the window.
    IntSetting modAlertWidth = {"/moderation/alerts/width", 0};
    IntSetting modAlertHeight = {"/moderation/alerts/height", 0};
    /// Where alert windows open, as the moderator last moved one - until
    /// then the system places them
    BoolSetting modAlertPositionSaved = {"/moderation/alerts/positionSaved",
                                         false};
    IntSetting modAlertX = {"/moderation/alerts/x", 0};
    IntSetting modAlertY = {"/moderation/alerts/y", 0};
    /// The colour each alert's reason stands out in. Shown lit up whatever is
    /// picked, so the reason always catches the eye.
    QStringSetting modAlertColorRepeat = {"/moderation/alerts/colorRepeat",
                                          "#ffa31a"};
    QStringSetting modAlertColorEmote = {"/moderation/alerts/colorEmote",
                                         "#ff33f5"};
    QStringSetting modAlertColorSuggestion = {
        "/moderation/alerts/colorSuggestion", "#1ae8ff"};

    /// Messages from moderators of the channels picked here are marked with
    /// those channels' profile pictures. Needs the WhoseTheMod plugin.
    BoolSetting modHighlightsEnabled = {"/moderation/modHighlights/enabled",
                                        true};
    /// Whether the user card shows where someone moderates
    BoolSetting modHighlightsUserCard = {"/moderation/modHighlights/userCard",
                                         true};
    ChatterinoSetting<std::vector<QString>> modHighlightChannels = {
        "/moderation/modHighlights/channels", {}};
    BoolSetting modHighlightsColorEnabled = {
        "/moderation/modHighlights/colorEnabled", true};
    QStringSetting modHighlightsColor = {"/moderation/modHighlights/color",
                                         "#509146ff"};
    /// Moderators never marked - bots, mostly - separated by commas
    QStringSetting modHighlightsIgnoredUsers = {
        "/moderation/modHighlights/ignoredUsers",
        "fossabot, nightbot, moobot, aecrobot, streamelements, sery_bot, "
        "botrixoficial, mixitupbot, streamerbot, streamlabs, restreambot, "
        "kofistreambot, tangiabot, wizebot, vivbot, rainmaker, blerp, "
        "pokemoncommunitygame"};
    /// Whether any moderator whose name ends in "bot" is left out as well
    /// An automatic backup of the whole profile, made every so many days into
    /// a folder of the user's choosing and replaced each time - see Export &
    /// Import
    BoolSetting autoBackupEnabled = {"/backup/enabled", false};
    IntSetting autoBackupDays = {"/backup/days", 7};
    /// Empty for the default folder
    QStringSetting autoBackupFolder = {"/backup/folder", ""};
    /// When the last one was made, ISO 8601 in UTC
    QStringSetting autoBackupLast = {"/backup/last", ""};

    /// Look on GitHub for a newer download - only the downloads do, see
    /// updatecheck
    BoolSetting updateCheckEnabled = {"/update/check", true};
    /// A newer download put off with "Später", and until when (ISO 8601 in
    /// UTC) it is not offered again
    QStringSetting updateSnoozedCommit = {"/update/snoozedCommit", ""};
    QStringSetting updateSnoozedUntil = {"/update/snoozedUntil", ""};

    /// Keep the setup alike on the user's computers through the backup
    /// folder - see profilesync
    BoolSetting profileSyncEnabled = {"/sync/enabled", false};
    /// When the shared setup this computer last wrote or took was written -
    /// one written at any other time is new from another computer
    QStringSetting profileSyncBase = {"/sync/base", ""};
    /// What this computer's setup looked like when it last wrote it
    QStringSetting profileSyncWritten = {"/sync/written", ""};

    BoolSetting modHighlightsIgnoreBotNames = {
        "/moderation/modHighlights/ignoreBotNames", true};
    /// What the emote alert offers at each step: "delete" or a timeout length
    QStringSetting emoteAlertSteps = {"/moderation/emoteAlert/steps",
                                      "delete, delete, 30s"};
    BoolSetting hidePreferencesButton = {"/appearance/hidePreferencesButton",
                                         false};
    BoolSetting hideUserButton = {"/appearance/hideUserButton", false};
    BoolSetting enableSmoothScrolling = {"/appearance/smoothScrolling", true};
    BoolSetting enableSmoothScrollingNewMessages = {
        "/appearance/smoothScrollingNewMessages", false};
    BoolSetting displaySevenTVPaints = {"/misc/displaySevenTVPaints", true};
    BoolSetting displaySevenTVPaintShadows = {
        "/misc/displaySevenTVPaintShadows", true};
    BoolSetting largeSevenTVPaintShadows = {"/misc/largeSevenTVPaintShadows",
                                            true};
    BoolSetting boldUsernames = {"/appearance/messages/boldUsernames", true};
    BoolSetting colorUsernames = {"/appearance/messages/colorUsernames", true};
    BoolSetting findAllUsernames = {"/appearance/messages/findAllUsernames",
                                    false};
    // BoolSetting customizable splitheader
    BoolSetting headerViewerCount = {"/appearance/splitheader/showViewerCount",
                                     false};
    BoolSetting headerStreamTitle = {"/appearance/splitheader/showTitle",
                                     false};
    BoolSetting headerGame = {"/appearance/splitheader/showGame", false};
    BoolSetting headerUptime = {"/appearance/splitheader/showUptime", false};
    FloatSetting customThemeMultiplier = {"/appearance/customThemeMultiplier",
                                          -0.5f};
    // BoolSetting useCustomWindowFrame = {"/appearance/useCustomWindowFrame",
    // false};

    FloatSetting overlayScaleFactor = {"/appearance/overlay/scaleFactor", 1};
    IntSetting overlayBackgroundOpacity = {
        "/appearance/overlay/backgroundOpacity", 50};
    BoolSetting enableOverlayShadow = {"/appearance/overlay/shadow", true};
    IntSetting overlayShadowOpacity = {"/appearance/overlay/shadowOpacity",
                                       255};
    QStringSetting overlayShadowColor = {"/appearance/overlay/shadowColor",
                                         "#000"};
    // These should be floats, but there's no good input UI for them
    IntSetting overlayShadowOffsetX = {"/appearance/overlay/shadowOffsetX", 2};
    IntSetting overlayShadowOffsetY = {"/appearance/overlay/shadowOffsetY", 2};
    IntSetting overlayShadowRadius = {"/appearance/overlay/shadowRadius", 8};

    float getClampedOverlayScale() const;
    void setClampedOverlayScale(float value);

    // Badges
    BoolSetting showBadgesGlobalAuthority = {
        "/appearance/badges/GlobalAuthority", true};
    BoolSetting showBadgesPredictions = {"/appearance/badges/predictions",
                                         true};
    BoolSetting showBadgesChannelAuthority = {
        "/appearance/badges/ChannelAuthority", true};
    BoolSetting showBadgesSubscription = {"/appearance/badges/subscription",
                                          true};
    BoolSetting showBadgesVanity = {"/appearance/badges/vanity", true};
    BoolSetting showBadgesChatterino = {"/appearance/badges/chatterino", true};
    BoolSetting showBadgesFfz = {"/appearance/badges/ffz", true};
    BoolSetting useCustomFfzModeratorBadges = {
        "/appearance/badges/useCustomFfzModeratorBadges", true};
    BoolSetting useCustomFfzVipBadges = {
        "/appearance/badges/useCustomFfzVipBadges", true};
    BoolSetting showBadgesBttv = {"/appearance/badges/bttv", true};
    BoolSetting showBadgesSevenTV = {"/appearance/badges/seventv", true};
    BoolSetting showBadgesHomies = {"/appearance/badges/homies", true};
    /// The label put on a first-time chatter's message, the way Twitch does.
    /// Edited through the "First Messages" row on the highlights page like
    /// every other caption; empty means no label.
    QStringSetting firstMessageCaption = {
        "/appearance/messages/firstMessageCaption", "FIRST"};
    BoolSetting animateSevenTVBadges = {"/appearance/badges/animateSeventv",
                                        true};
    QSizeSetting lastPopupSize = {
        "/appearance/lastPopup/size",
        {300, 500},
    };

    // Scrollbar
    BoolSetting hideScrollbarThumb = {
        "/appearance/scrollbar/hideThumb",
        false,
    };
    BoolSetting hideScrollbarHighlights = {
        "/appearance/scrollbar/hideHighlights",
        false,
    };

    BoolSetting pulseTextInputOnSelfMessage = {
        "/appearance/pulseTextInputOnSelfMessage",
        false,
    };

    /// Behaviour
    BoolSetting allowDuplicateMessages = {"/behaviour/allowDuplicateMessages",
                                          true};
    BoolSetting mentionUsersWithAt = {"/behaviour/mentionUsersWithAt", false};
    BoolSetting showJoins = {"/behaviour/showJoins", false};
    BoolSetting showParts = {"/behaviour/showParts", false};
    FloatSetting mouseScrollMultiplier = {"/behaviour/mouseScrollMultiplier",
                                          1.0};
    BoolSetting autoCloseUserPopup = {"/behaviour/autoCloseUserPopup", true};
    BoolSetting autoCloseThreadPopup = {"/behaviour/autoCloseThreadPopup",
                                        false};

    /// Specifies whether the search functionality should be enabled
    BoolSetting searchEnabled = {
        "/behaviour/search/enabled",
        false,
    };
    /// The URL of the search engine
    QStringSetting searchEngineUrl = {
        "/behaviour/search/engineUrl",
        "",
    };
    /// The name of the search engine
    QStringSetting searchEngineName = {
        "/behaviour/search/engineName",
        "",
    };
    BoolSetting searchIncognito = {
        "/behaviour/search/incognito",
        false,
    };

    EnumSetting<UsernameRightClickBehavior> usernameRightClickBehavior = {
        "/behaviour/usernameRightClickBehavior",
        UsernameRightClickBehavior::Mention,
    };
    EnumSetting<UsernameRightClickBehavior> usernameRightClickModifierBehavior =
        {
            "/behaviour/usernameRightClickBehaviorWithModifier",
            UsernameRightClickBehavior::Reply,
    };
    EnumSetting<Qt::KeyboardModifier> usernameRightClickModifier = {
        "/behaviour/usernameRightClickModifier",
        Qt::KeyboardModifier::ShiftModifier};

    BoolSetting autoSubToParticipatedThreads = {
        "/behaviour/autoSubToParticipatedThreads",
        true,
    };

    /// The maximum length the contents of a deleted message can be
    /// before we truncate it in the chat
    IntSetting deletedMessageLengthLimit = {
        "/behaviour/deletedMessageLengthLimit",
        50,
    };

    // Auto-completion
    BoolSetting onlyFetchChattersForSmallerStreamers = {
        "/behaviour/autocompletion/onlyFetchChattersForSmallerStreamers", true};
    IntSetting smallStreamerLimit = {
        "/behaviour/autocompletion/smallStreamerLimit", 1000};
    BoolSetting prefixOnlyEmoteCompletion = {
        "/behaviour/autocompletion/prefixOnlyCompletion", true};
    BoolSetting userCompletionOnlyWithAt = {
        "/behaviour/autocompletion/userCompletionOnlyWithAt", false};
    BoolSetting emoteCompletionWithColon = {
        "/behaviour/autocompletion/emoteCompletionWithColon", true};
    BoolSetting showUsernameCompletionMenu = {
        "/behaviour/autocompletion/showUsernameCompletionMenu", true};
    BoolSetting alwaysIncludeBroadcasterInUserCompletions = {
        "/behaviour/autocompletion/alwaysIncludeBroadcasterInUserCompletions",
        true,
    };
    BoolSetting useSmartEmoteCompletion = {
        "/experiments/useSmartEmoteCompletion",
        false,
    };

    BoolSetting enableSpellChecking = {
        "/behaviour/spellChecking/enabled",
        false,
    };
    QStringSetting spellCheckingDefaultDictionary = {
        "/behaviour/spellChecking/defaultDictionary",
        "",
    };
    IntSetting nSpellCheckingSuggestions = {
        "/behaviour/spellChecking/suggestions/count",
        -1,
    };

    FloatSetting pauseOnHoverDuration = {"/behaviour/pauseOnHoverDuration", 0};
    EnumSetting<Qt::KeyboardModifier> pauseChatModifier = {
        "/behaviour/pauseChatModifier", Qt::KeyboardModifier::NoModifier};
    BoolSetting autorun = {"/behaviour/autorun", false};
    BoolSetting mentionUsersWithComma = {"/behaviour/mentionUsersWithComma",
                                         true};

    BoolSetting disableTabRenamingOnClick = {
        "/behaviour/disableTabRenamingOnClick",
        false,
    };

    /// Emotes
    BoolSetting scaleEmotesByLineHeight = {"/emotes/scaleEmotesByLineHeight",
                                           false};
    BoolSetting enableEmoteImages = {"/emotes/enableEmoteImages", true};
    BoolSetting animateEmotes = {"/emotes/enableGifAnimations", true};
    BoolSetting enableZeroWidthEmotes = {"/emotes/enableZeroWidthEmotes", true};
    FloatSetting emoteScale = {"/emotes/scale", 1.f};
    EnumStringSetting<EmoteTooltipScale> emoteTooltipScale = {
        "/emotes/tooltipScale",
        EmoteTooltipScale::Medium,
    };
    BoolSetting showUnlistedSevenTVEmotes = {
        "/emotes/showUnlistedSevenTVEmotes", false};
    /**
     * This setting is kept for backwards compatibility.
     */
    BoolSetting showUnlistedEmotesDontUse = {"/emotes/showUnlistedEmotes",
                                             false};

    EnumStringSetting<EmojiStyle> emojiSet = {
        "/emotes/emojiSet",
        EmojiStyle::Twitter,
    };

    BoolSetting stackBits = {"/emotes/stackBits", false};
    BoolSetting removeSpacesBetweenEmotes = {
        "/emotes/removeSpacesBetweenEmotes", false};

    BoolSetting enableBTTVGlobalEmotes = {"/emotes/bttv/global", true};
    BoolSetting enableBTTVChannelEmotes = {"/emotes/bttv/channel", true};
    BoolSetting enableBTTVLiveUpdates = {"/emotes/bttv/liveupdates", true};
    BoolSetting sendBTTVActivity = {"/emotes/bttv/sendActivity", true};
    BoolSetting enableFFZGlobalEmotes = {"/emotes/ffz/global", true};
    BoolSetting enableFFZChannelEmotes = {"/emotes/ffz/channel", true};
    BoolSetting enableSevenTVGlobalEmotes = {"/emotes/seventv/global", true};
    BoolSetting enableSevenTVChannelEmotes = {"/emotes/seventv/channel", true};
    BoolSetting enableSevenTVPersonalEmotes = {"/emotes/seventv/personal",
                                               true};
    BoolSetting enableSevenTVEventAPI = {"/emotes/seventv/eventapi", true};
    BoolSetting sendSevenTVActivity = {"/emotes/seventv/sendActivity", true};

    BoolSetting allowAvifImages = {"/emotes/allowAvif", true};

    /// Links
    BoolSetting linksDoubleClickOnly = {"/links/doubleClickToOpen", false};
    BoolSetting linkInfoTooltip = {"/links/linkInfoTooltip", false};
    IntSetting thumbnailSize = {"/appearance/thumbnailSize", 0};
    IntSetting thumbnailSizeStream = {"/appearance/thumbnailSizeStream", 2};
    BoolSetting unshortLinks = {"/links/unshortLinks", false};
    BoolSetting lowercaseDomains = {"/links/linkLowercase", true};

    /// Streamer Mode
    // TODO: Should these settings be converted to booleans that live outside of
    // streamer mode?
    // Something like:
    //  - "Hide when streamer mode is enabled"
    //  - "Always hide"
    //  - "Don't hide"
    EnumSetting<StreamerModeSetting> enableStreamerMode = {
        "/streamerMode/enabled",
        StreamerModeSetting::DetectStreamingSoftware,
    };
    BoolSetting streamerModeHideUsercardAvatars = {
        "/streamerMode/hideUsercardAvatars", true};
    BoolSetting streamerModeHideLinkThumbnails = {
        "/streamerMode/hideLinkThumbnails", true};
    BoolSetting streamerModeHideViewerCountAndDuration = {
        "/streamerMode/hideViewerCountAndDuration", false};
    BoolSetting streamerModeHideModActions = {"/streamerMode/hideModActions",
                                              true};
    BoolSetting streamerModeHideRestrictedUsers = {
        "/streamerMode/hideRestrictedUsers",
        true,
    };
    BoolSetting streamerModeMuteMentions = {"/streamerMode/muteMentions", true};
    BoolSetting streamerModeSuppressLiveNotifications = {
        "/streamerMode/supressLiveNotifications", false};
    BoolSetting streamerModeSuppressInlineWhispers = {
        "/streamerMode/suppressInlineWhispers", true};
    BoolSetting streamerModeHideBlockedTermText = {
        "/streamerMode/hideBlockedTermText",
        true,
    };

    /// Blocked Users
    BoolSetting enableTwitchBlockedUsers = {"/ignore/enableTwitchBlockedUsers",
                                            true};
    IntSetting showBlockedUsersMessages = {"/ignore/showBlockedUsers", 0};

    /// Moderation
    QStringSetting timeoutAction = {"/moderation/timeoutAction", "Disable"};
    IntSetting timeoutStackStyle = {
        "/moderation/timeoutStackStyle",
        static_cast<int>(TimeoutStackStyle::Default)};
    EnumStringSetting<ShowModerationState> showBlockedTermAutomodMessages = {
        "/moderation/showBlockedTermAutomodMessages",
        ShowModerationState::Always,
    };

    /// Highlighting
    //    BoolSetting enableHighlights = {"/highlighting/enabled", true};

    BoolSetting enableSelfHighlight = {
        "/highlighting/selfHighlight/nameIsHighlightKeyword", true};
    BoolSetting showSelfHighlightInMentions = {
        "/highlighting/selfHighlight/showSelfHighlightInMentions", true};
    BoolSetting enableSelfHighlightSound = {
        "/highlighting/selfHighlight/enableSound", true};
    BoolSetting enableSelfHighlightTaskbar = {
        "/highlighting/selfHighlight/enableTaskbarFlashing", true};
    QStringSetting selfHighlightSoundUrl = {
        "/highlighting/selfHighlightSoundUrl", ""};
    QStringSetting selfHighlightColor = {"/highlighting/selfHighlightColor",
                                         ""};
    /// Free text shown next to a matching message
    QStringSetting selfHighlightCaption = {"/highlighting/selfHighlightCaption",
                                           ""};

    BoolSetting enableSelfMessageHighlight = {
        "/highlighting/selfMessageHighlight/enabled", false};
    BoolSetting showSelfMessageHighlightInMentions = {
        "/highlighting/selfMessageHighlight/showInMentions", false};
    QStringSetting selfMessageHighlightColor = {
        "/highlighting/selfMessageHighlight/color", ""};
    /// Free text shown next to a matching message
    QStringSetting selfMessageHighlightCaption = {
        "/highlighting/selfMessageHighlightCaption", ""};

    BoolSetting enableWhisperHighlight = {
        "/highlighting/whisperHighlight/whispersHighlighted", true};
    BoolSetting enableWhisperHighlightSound = {
        "/highlighting/whisperHighlight/enableSound", false};
    BoolSetting enableWhisperHighlightTaskbar = {
        "/highlighting/whisperHighlight/enableTaskbarFlashing", false};
    QStringSetting whisperHighlightSoundUrl = {
        "/highlighting/whisperHighlightSoundUrl", ""};
    QStringSetting whisperHighlightColor = {
        "/highlighting/whisperHighlightColor", ""};
    /// Free text shown next to a matching message
    QStringSetting whisperHighlightCaption = {
        "/highlighting/whisperHighlightCaption", ""};

    BoolSetting enableRedeemedHighlight = {
        "/highlighting/redeemedHighlight/highlighted", true};
    //    BoolSetting enableRedeemedHighlightSound = {
    //        "/highlighting/redeemedHighlight/enableSound", false};
    //    BoolSetting enableRedeemedHighlightTaskbar = {
    //        "/highlighting/redeemedHighlight/enableTaskbarFlashing", false};
    //    QStringSetting redeemedHighlightSoundUrl = {
    //        "/highlighting/redeemedHighlightSoundUrl", ""};
    QStringSetting redeemedHighlightColor = {
        "/highlighting/redeemedHighlightColor", ""};
    /// Free text shown next to a matching message
    QStringSetting redeemedHighlightCaption = {
        "/highlighting/redeemedHighlightCaption", ""};

    BoolSetting enableFirstMessageHighlight = {
        "/highlighting/firstMessageHighlight/highlighted", true};
    //    BoolSetting enableFirstMessageHighlightSound = {
    //        "/highlighting/firstMessageHighlight/enableSound", false};
    //    BoolSetting enableFirstMessageHighlightTaskbar = {
    //        "/highlighting/firstMessageHighlight/enableTaskbarFlashing", false};
    //    QStringSetting firstMessageHighlightSoundUrl = {
    //        "/highlighting/firstMessageHighlightSoundUrl", ""};
    QStringSetting firstMessageHighlightColor = {
        "/highlighting/firstMessageHighlightColor", ""};

    BoolSetting enableElevatedMessageHighlight = {
        "/highlighting/elevatedMessageHighlight/highlighted", true};
    //    BoolSetting enableElevatedMessageHighlightSound = {
    //        "/highlighting/elevatedMessageHighlight/enableSound", false};
    //    BoolSetting enableElevatedMessageHighlightTaskbar = {
    //        "/highlighting/elevatedMessageHighlight/enableTaskbarFlashing", false};
    //    QStringSetting elevatedMessageHighlightSoundUrl = {
    //        "/highlighting/elevatedMessageHighlight/soundUrl", ""};
    QStringSetting elevatedMessageHighlightColor = {
        "/highlighting/elevatedMessageHighlight/color", ""};
    /// Free text shown next to a matching message
    QStringSetting elevatedMessageHighlightCaption = {
        "/highlighting/elevatedMessageHighlightCaption", ""};

    BoolSetting enableSubHighlight = {
        "/highlighting/subHighlight/subsHighlighted", true};
    BoolSetting enableSubHighlightSound = {
        "/highlighting/subHighlight/enableSound", false};
    BoolSetting enableSubHighlightTaskbar = {
        "/highlighting/subHighlight/enableTaskbarFlashing", false};
    QStringSetting subHighlightSoundUrl = {"/highlighting/subHighlightSoundUrl",
                                           ""};
    QStringSetting subHighlightColor = {"/highlighting/subHighlightColor", ""};
    /// Free text shown next to a matching message
    QStringSetting subHighlightCaption = {"/highlighting/subHighlightCaption",
                                          ""};

    BoolSetting enableWatchStreakHighlight = {
        "/highlighting/watchStreak/enabled", true};
    QStringSetting watchStreakHighlightColor = {
        "/highlighting/watchStreak/color", ""};
    /// Free text shown next to a matching message
    QStringSetting watchStreakHighlightCaption = {
        "/highlighting/watchStreakHighlightCaption", ""};

    BoolSetting enableAutomodHighlight = {
        "/highlighting/automod/enabled",
        true,
    };
    BoolSetting showAutomodInMentions = {
        "/highlighting/automod/showInMentions",
        false,
    };
    BoolSetting enableAutomodHighlightSound = {
        "/highlighting/automod/enableSound",
        false,
    };
    BoolSetting enableAutomodHighlightTaskbar = {
        "/highlighting/automod/enableTaskbarFlashing",
        false,
    };
    QStringSetting automodHighlightSoundUrl = {
        "/highlighting/automod/soundUrl",
        "",
    };
    QStringSetting automodHighlightColor = {"/highlighting/automod/color", ""};
    /// Free text shown next to a matching message
    QStringSetting automodHighlightCaption = {
        "/highlighting/automodHighlightCaption", ""};

    BoolSetting enableThreadHighlight = {
        "/highlighting/thread/nameIsHighlightKeyword", true};
    BoolSetting showThreadHighlightInMentions = {
        "/highlighting/thread/showSelfHighlightInMentions", true};
    BoolSetting enableThreadHighlightSound = {
        "/highlighting/thread/enableSound", true};
    BoolSetting enableThreadHighlightTaskbar = {
        "/highlighting/thread/enableTaskbarFlashing", true};
    QStringSetting threadHighlightSoundUrl = {
        "/highlighting/threadHighlightSoundUrl", ""};
    QStringSetting threadHighlightColor = {"/highlighting/threadHighlightColor",
                                           ""};
    /// Free text shown next to a matching message
    QStringSetting threadHighlightCaption = {
        "/highlighting/threadHighlightCaption", ""};

    QStringSetting highlightColor = {"/highlighting/color", ""};

    BoolSetting longAlerts = {"/highlighting/alerts", false};

    BoolSetting highlightMentions = {"/highlighting/mentions", true};

    /// Filtering
    BoolSetting excludeUserMessagesFromFilter = {
        "/filtering/excludeUserMessages", false};

    /// Logging
    BoolSetting enableLogging = {"/logging/enabled", false};
    BoolSetting onlyLogListedChannels = {"/logging/onlyLogListedChannels",
                                         false};
    BoolSetting separatelyStoreStreamLogs = {
        "/logging/separatelyStoreStreamLogs",
        false,
    };
    QStringSetting logTimestampFormat = {
        "/logging/logTimestampFormat",
        "hh:mm:ss",
    };
    BoolSetting tryUseTwitchTimestamps = {
        "/logging/tryUseTwitchTimestamps",
        false,
    };
    QStringSetting logPath = {"/logging/path", ""};

    QStringSetting pathHighlightSound = {"/highlighting/highlightSoundPath",
                                         ""};

    BoolSetting highlightAlwaysPlaySound = {"/highlighting/alwaysPlaySound",
                                            false};

    BoolSetting inlineWhispers = {"/whispers/enableInlineWhispers", true};
    BoolSetting highlightInlineWhispers = {"/whispers/highlightInlineWhispers",
                                           false};

    /// Notifications
    BoolSetting notificationFlashTaskbar = {"/notifications/enableFlashTaskbar",
                                            false};
    BoolSetting notificationPlaySound = {"/notifications/enablePlaySound",
                                         false};
    BoolSetting notificationCustomSound = {"/notifications/customPlaySound",
                                           false};
    QStringSetting notificationPathSound = {"/notifications/highlightSoundPath",
                                            "qrc:/sounds/ping3.wav"};
    BoolSetting notificationOnAnyChannel = {"/notifications/onAnyChannel",
                                            false};
    BoolSetting suppressInitialLiveNotification = {
        "/notifications/suppressInitialLive", false};

    BoolSetting notificationToast = {"/notifications/enableToast", false};
    BoolSetting createShortcutForToasts = {
        "/notifications/createShortcutForToasts",
        (Modes::instance().isPortable || Modes::instance().isExternallyPackaged)
            ? false
            : true,
    };
    IntSetting openFromToast = {"/notifications/openFromToast",
                                static_cast<int>(ToastReaction::OpenInBrowser)};

    /// External tools
    // Streamlink
    BoolSetting streamlinkUseCustomPath = {"/external/streamlink/useCustomPath",
                                           false};
    QStringSetting streamlinkPath = {"/external/streamlink/customPath", ""};
    EnumStringSetting<StreamLinkPreferredQuality> preferredQuality = {
        "/external/streamlink/quality",
        StreamLinkPreferredQuality::Choose,
    };
    QStringSetting streamlinkOpts = {"/external/streamlink/options", ""};

    // Custom URI Scheme
    QStringSetting customURIScheme = {"/external/urischeme"};

    // Image Uploader
    BoolSetting imageUploaderEnabled = {"/external/imageUploader/enabled",
                                        false};
    QStringSetting imageUploaderUrl = {"/external/imageUploader/url", ""};
    QStringSetting imageUploaderFormField = {
        "/external/imageUploader/formField", ""};
    QStringSetting imageUploaderHeaders = {"/external/imageUploader/headers",
                                           ""};
    QStringSetting imageUploaderLink = {"/external/imageUploader/link", ""};
    QStringSetting imageUploaderDeletionLink = {
        "/external/imageUploader/deletionLink", ""};

    /// Misc
    BoolSetting betaUpdates = {"/misc/beta", false};
#ifdef Q_OS_LINUX
    BoolSetting useKeyring = {"/misc/useKeyring", true};
#endif

    IntSetting startUpNotification = {"/misc/startUpNotification", 0};
    QStringSetting currentVersion = {"/misc/currentVersion", ""};
    IntSetting overlayKnowledgeLevel = {"/misc/overlayKnowledgeLevel", 0};

    BoolSetting loadTwitchMessageHistoryOnConnect = {
        "/misc/twitch/loadMessageHistoryOnConnect", true};
    IntSetting twitchMessageHistoryLimit = {
        "/misc/twitch/messageHistoryLimit",
        800,
    };
    IntSetting scrollbackSplitLimit = {
        "/misc/scrollback/splitLimit",
        1000,
    };
    IntSetting scrollbackUsercardLimit = {
        "/misc/scrollback/usercardLimit",
        1000,
    };
    BoolSetting displaySevenTVAnimatedProfile = {
        "/misc/displaySevenTVAnimatedProfile", true};

    EnumStringSetting<ChatSendProtocol> chatSendProtocol = {
        "/misc/chatSendProtocol", ChatSendProtocol::Default};

    BoolSetting openLinksIncognito = {"/misc/openLinksIncognito", 0};

    EnumSetting<ThumbnailPreviewMode> emotesTooltipPreview = {
        "/misc/emotesTooltipPreview",
        ThumbnailPreviewMode::AlwaysShow,
    };
    QStringSetting cachePath = {"/cache/path", ""};
    BoolSetting attachExtensionToAnyProcess = {
        "/misc/attachExtensionToAnyProcess", false};
    BoolSetting askOnImageUpload = {"/misc/askOnImageUpload", true};
    /// The newest entry the user has been shown in the "What's new" window.
    QStringSetting lastSeenChanges = {"/misc/lastSeenChanges", ""};
    BoolSetting informOnTabVisibilityToggle = {"/misc/askOnTabVisibilityToggle",
                                               true};
    BoolSetting lockNotebookLayout = {"/misc/lockNotebookLayout", false};
    BoolSetting showPronouns = {"/misc/showPronouns", false};
    BoolSetting showTitleInLiveMessage = {
        "/extraChannels/live/showTitle",
        false,
    };

    /// UI

    BoolSetting showSendButton = {"/ui/showSendButton", false};

    struct {
        // this isn't shown in the UI
        BoolSetting enabled = {"/plugins/repl/enabled", false};
        // An empty string implies the default monospace font
        QStringSetting fontFamily = {"/plugins/repl/fontFamily", {}};
        QStringSetting fontStyle = {"/plugins/repl/fontStyle", "Regular"};
        IntSetting fontSize = {"/plugins/repl/fontSize", 10};
    } pluginRepl;

    // Similarity
    BoolSetting similarityEnabled = {"/similarity/similarityEnabled", false};
    BoolSetting colorSimilarDisabled = {"/similarity/colorSimilarDisabled",
                                        true};
    BoolSetting hideSimilar = {"/similarity/hideSimilar", false};
    BoolSetting hideSimilarBySameUser = {"/similarity/hideSimilarBySameUser",
                                         true};
    BoolSetting hideSimilarMyself = {"/similarity/hideSimilarMyself", false};
    BoolSetting shownSimilarTriggerHighlights = {
        "/similarity/shownSimilarTriggerHighlights", false};
    FloatSetting similarityPercentage = {"/similarity/similarityPercentage",
                                         0.9f};
    IntSetting hideSimilarMaxDelay = {"/similarity/hideSimilarMaxDelay", 5};
    IntSetting hideSimilarMaxMessagesToCheck = {
        "/similarity/hideSimilarMaxMessagesToCheck", 3};

    /// Timeout buttons

    ChatterinoSetting<std::vector<TimeoutButton>> timeoutButtons = {
        "/timeouts/timeoutButtons",
        {{"s", 1},
         {"s", 30},
         {"m", 1},
         {"m", 5},
         {"m", 30},
         {"h", 1},
         {"d", 1},
         {"w", 1}}};

    BoolSetting pluginsEnabled = {"/plugins/supportEnabled", false};
    ChatterinoSetting<std::vector<QString>> enabledPlugins = {
        "/plugins/enabledPlugins", {}};

    // Sound
    EnumStringSetting<SoundBackend> soundBackend = {
        "/sound/backend",
        SoundBackend::Miniaudio,
    };

    BoolSetting soundMiniaudioKeepEngineAlive = {
        "/sound/miniaudio/keepEngineAlive",
        false,
    };

    // Advanced
    BoolSetting enableExperimentalEventSub = {
        "/eventsub/enableExperimental",
        true,
    };

    QStringSetting additionalExtensionIDs{"/misc/additionalExtensionIDs", ""};

    BoolSetting xChatterino7NoHttp2{"/x-chatterino7/no-http2", false};

private:
    ChatterinoSetting<std::vector<HighlightPhrase>> highlightedMessagesSetting =
        {"/highlighting/highlights"};
    ChatterinoSetting<std::vector<HighlightPhrase>> highlightedUsersSetting = {
        "/highlighting/users"};
    ChatterinoSetting<std::vector<HighlightBadge>> highlightedBadgesSetting = {
        "/highlighting/badges"};
    ChatterinoSetting<std::vector<HighlightBlacklistUser>>
        blacklistedUsersSetting = {"/highlighting/blacklist"};
    ChatterinoSetting<std::vector<IgnorePhrase>> ignoredMessagesSetting = {
        "/ignore/phrases"};
    ChatterinoSetting<std::vector<QString>> mutedChannelsSetting = {
        "/pings/muted"};
    ChatterinoSetting<std::vector<FilterRecordPtr>> filterRecordsSetting = {
        "/filtering/filters"};
    ChatterinoSetting<std::vector<Nickname>> nicknamesSetting = {"/nicknames"};

    /// Colours the user has named, so the picker can show what each one is
    /// kept around for.
    ChatterinoSetting<std::vector<NamedColor>> namedColorsSetting = {
        "/namedColors"};
    ChatterinoSetting<std::vector<ModerationAction>> moderationActionsSetting =
        {"/moderation/actions"};
    ChatterinoSetting<std::vector<ChannelLog>> loggedChannelsSetting = {
        "/logging/channels"};
    SignalVector<QString> mutedChannels;

public:
    SignalVector<HighlightPhrase> highlightedMessages;
    SignalVector<HighlightPhrase> highlightedUsers;
    SignalVector<HighlightBadge> highlightedBadges;
    SignalVector<HighlightBlacklistUser> blacklistedUsers;
    SignalVector<IgnorePhrase> ignoredMessages;
    SignalVector<FilterRecordPtr> filterRecords;
    SignalVector<Nickname> nicknames;
    SignalVector<NamedColor> namedColors;
    SignalVector<ModerationAction> moderationActions;
    SignalVector<ChannelLog> loggedChannels;

    bool isHighlightedUser(const QString &username);
    bool isBlacklistedUser(const QString &username);
    bool isMutedChannel(const QString &channelName);
    bool toggleMutedChannel(const QString &channelName);
    std::optional<QString> matchNickname(const QString &username);
    void mute(const QString &channelName);
    void unmute(const QString &channelName);

private:
    void updateModerationActions();

    std::unique_ptr<rapidjson::Document> snapshot_;

    pajlada::Signals::SignalHolder signalHolder;
};

Settings *getSettings();

}  // namespace chatterino

template <>
constexpr magic_enum::customize::customize_t
    magic_enum::customize::enum_name<chatterino::StreamLinkPreferredQuality>(
        chatterino::StreamLinkPreferredQuality value) noexcept
{
    using chatterino::StreamLinkPreferredQuality;
    switch (value)
    {
        case chatterino::StreamLinkPreferredQuality::Choose:
        case chatterino::StreamLinkPreferredQuality::Source:
        case chatterino::StreamLinkPreferredQuality::High:
        case chatterino::StreamLinkPreferredQuality::Medium:
        case chatterino::StreamLinkPreferredQuality::Low:
            return default_tag;

        case chatterino::StreamLinkPreferredQuality::AudioOnly:
            return "Audio only";

        default:
            return default_tag;
    }
}
