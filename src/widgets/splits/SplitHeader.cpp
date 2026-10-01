// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitHeader.hpp"

#include "Application.hpp"
#include "common/network/NetworkCommon.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/banners/BannerChannels.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/hotkeys/Hotkey.hpp"
#include "controllers/hotkeys/HotkeyCategory.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "controllers/twitch/ChannelNumbers.hpp"
#include "controllers/notifications/NotificationController.hpp"
#include "providers/kick/KickChannel.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Settings.hpp"
#include "singletons/StreamerMode.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "util/FormatTime.hpp"
#include "util/Helpers.hpp"
#include "util/LayoutHelper.hpp"
#include "util/UiStyle.hpp"
#include "widgets/buttons/DrawnButton.hpp"
#include "widgets/buttons/LabelButton.hpp"
#include "widgets/buttons/SvgButton.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/helper/CommonTexts.hpp"
#include "widgets/Label.hpp"
#include "widgets/splits/PinnedMessageWidget.hpp"
#include "widgets/splits/HypeTrainBannerWidget.hpp"
#include "widgets/splits/VoteBannerWidget.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/splits/HeaderParts.hpp"
#include "widgets/splits/SplitHeaderExtras.hpp"
#include "widgets/TooltipWidget.hpp"

#include <QDrag>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QPainter>

#include <cmath>

namespace {

using namespace chatterino;

/// The width of the standard button.
constexpr const int BUTTON_WIDTH = 28;

/// The width of the "Add split" button.
///
/// This matches the scrollbar's full width.
constexpr const int ADD_SPLIT_BUTTON_WIDTH = 16;

// 5 minutes
constexpr const qint64 THUMBNAIL_MAX_AGE_MS = 5LL * 60 * 1000;

auto formatRoomModeUnclean(const TwitchChannel::RoomModes &modes) -> QString
{
    QString text;

    if (modes.r9k)
    {
        text += "r9k, ";
    }
    if (modes.slowMode > 0)
    {
        text += QString("slow(%1), ").arg(localizeNumbers(modes.slowMode));
    }
    if (modes.emoteOnly)
    {
        text += "emote, ";
    }
    if (modes.submode)
    {
        text += "sub, ";
    }
    if (modes.followerOnly != -1)
    {
        if (modes.followerOnly != 0)
        {
            text += QString("follow(%1), ")
                        .arg(formatDurationExact(
                            std::chrono::minutes{modes.followerOnly}));
        }
        else
        {
            text += QString("follow, ");
        }
    }

    return text;
}

QString formatRoomModeUnclean(const KickChannel::RoomModes &modes)
{
    TwitchChannel::RoomModes twitch{
        .submode = modes.subscribersMode,
        .r9k = false,
        .emoteOnly = modes.emotesMode,
        .followerOnly = -1,
        .slowMode = 0,
    };
    if (modes.followersModeDuration)
    {
        twitch.followerOnly =
            static_cast<int>(modes.followersModeDuration->count());
    }
    if (modes.slowModeDuration)
    {
        twitch.slowMode = static_cast<int>(modes.slowModeDuration->count());
    }
    return formatRoomModeUnclean(twitch);
}

/// ChattiFlexii: the modes one under the other instead of side by side.
/// With two lines of title bar there is room for it, and what it saves is
/// width - which is what the title needs.
QString stackRoomModes(const QString &text)
{
    auto parts = text.split(QStringLiteral(", "), Qt::SkipEmptyParts);
    if (parts.size() < 2)
    {
        return text;
    }

    const auto half = (parts.size() + 1) / 2;
    return parts.mid(0, half).join(QStringLiteral(", ")) + '\n' +
           parts.mid(half).join(QStringLiteral(", "));
}

void cleanRoomModeText(QString &text, bool hasModRights)
{
    if (text.length() > 2)
    {
        text = text.mid(0, text.size() - 2);
    }

    // ChattiFlexii: on one line rather than broken after the second mode,
    // so the header stays one tidy row

    if (text.isEmpty() && hasModRights)
    {
        text = "none";
    }
}

auto formatTooltip(const TwitchChannel::StreamStatus &s, QString thumbnail,
                   bool limitSize = false)
{
    auto title = [&s]() -> QString {
        if (s.title.isEmpty())
        {
            return QStringLiteral("");
        }

        return s.title.toHtmlEscaped() + "<br><br>";
    }();

    auto tooltip = [&]() -> QString {
        if (getSettings()->thumbnailSizeStream.getValue() == 0)
        {
            return QStringLiteral("");
        }

        if (thumbnail.isEmpty())
        {
            return QStringLiteral("Couldn't fetch thumbnail<br>");
        }

        QString sizeStr;
        if (limitSize)
        {
            auto height =
                std::min(getSettings()->thumbnailSizeStream.getValue(), 4) * 80;
            sizeStr =
                QStringLiteral(" height=\"") % QString::number(height) % '"';
        }

        return u"<img " % sizeStr % u" src=\"data:image/jpg;base64, " %
               thumbnail % u"\"><br>";
    }();

    auto game = [&s]() -> QString {
        if (s.game.isEmpty())
        {
            return QStringLiteral("");
        }

        return s.game.toHtmlEscaped() + "<br>";
    }();

    auto extraStreamData = [&s]() -> QString {
        if (getApp()->getStreamerMode()->isEnabled() &&
            getSettings()->streamerModeHideViewerCountAndDuration)
        {
            return QStringLiteral(
                "<span style=\"color: #808892;\">&lt;Streamer "
                "Mode&gt;</span>");
        }

        auto text = QString("%1 for %2 with %3 viewers")
                        .arg(s.rerun ? "Vod-casting" : "Live")
                        .arg(s.uptime)
                        .arg(localizeNumbers(s.viewerCount));

        if (s.sharedParticipantCount > 1)
        {
            text += QString("<br>%1 viewers across %2 channels streaming "
                            "together")
                        .arg(localizeNumbers(s.sharedViewerCount))
                        .arg(s.sharedParticipantCount);
        }

        return text;
    }();

    return QString("<p style=\"text-align: center;\">" +  //
                   title +                                //
                   tooltip +                              //
                   game +                                 //
                   extraStreamData +                      //
                   "</p>"                                 //
    );
}

auto formatOfflineTooltip(const TwitchChannel::StreamStatus &s)
{
    return QString("<p style=\"text-align: center;\">Offline<br>%1</p>")
        .arg(s.title.toHtmlEscaped());
}

TwitchChannel::StreamStatus toTwitchStreamStatus(
    const KickChannel::StreamData &data)
{
    return {
        .live = data.isLive,
        .viewerCount = static_cast<unsigned>(data.viewerCount),
        .title = data.title,
        .game = data.category,
        .uptime = data.uptime,
        .streamType = QStringLiteral("live"),
    };
}

auto distance(QPoint a, QPoint b)
{
    auto x = std::abs(a.x() - b.x());
    auto y = std::abs(a.y() - b.y());

    return std::sqrt(x * x + y * y);
}

}  // namespace

namespace chatterino {

SplitHeader::SplitHeader(Split *split)
    : BaseWidget(split)
    , split_(split)
    , tooltipWidget_(new TooltipWidget(this))
{
    this->initializeLayout();

    this->setMouseTracking(true);
    this->updateChannelText();
    this->handleChannelChanged();
    this->updateIcons();

    // The lifetime of these signals are tied to the lifetime of the Split.
    // Since the SplitHeader is owned by the Split, they will always be destroyed
    // at the same time.
    std::ignore = this->split_->focused.connect([this]() {
        this->themeChangedEvent();
    });
    std::ignore = this->split_->focusLost.connect([this]() {
        this->themeChangedEvent();
    });
    std::ignore = this->split_->channelChanged.connect([this]() {
        this->handleChannelChanged();
    });

    this->bSignals_.emplace_back(
        getApp()->getAccounts()->twitch.currentUserChanged.connect([this] {
            this->updateIcons();
        }));

    auto _ = [this](const auto &, const auto &) {
        this->updateChannelText();
    };
    getSettings()->headerViewerCount.connect(_, this->managedConnections_);
    getSettings()->headerStreamTitle.connect(_, this->managedConnections_);
    getSettings()->headerGame.connect(_, this->managedConnections_);
    getSettings()->headerUptime.connect(_, this->managedConnections_);
    getSettings()->headerLiveMarker.connect(_, this->managedConnections_);
    getSettings()->headerChannelName.connect(_, this->managedConnections_);
    getSettings()->splitHeaderPictures.connect(_, this->managedConnections_);
    getSettings()->splitHeaderActivity.connect(_, this->managedConnections_);
    // Buttons -> Title bar
    getSettings()->splitHeaderOrder.connect(
        [this] {
            this->arrangeParts();
        },
        this->managedConnections_, false);
    getSettings()->splitHeaderHidden.connect(
        [this] {
            this->updateChannelText();
            this->updateRoomModes();
            this->updateIcons();
            this->updatePinButton();
            this->updateVoteButton();
            this->updateHypeButton();
            this->setAddButtonVisible(this->addButtonWanted_);
        },
        this->managedConnections_, false);
    getSettings()->splitHeaderActivityShare.connect(
        [this] {
            this->fitActivity();
        },
        this->managedConnections_, false);
    // Buttons -> Titelleiste: dragged wider or further apart in the preview
    const auto again = [this] {
        this->scaleChangedEvent(this->scale());
        this->fitActivity();
        this->update();
    };
    getSettings()->splitHeaderSpacing.connect(
        [again](auto, auto) {
            again();
        },
        this->managedConnections_, false);
    getSettings()->splitHeaderWidths.connect(
        [again](const auto &, auto) {
            again();
        },
        this->managedConnections_, false);
    getSettings()->headerColors.connect(
        [this](const auto &, auto) {
            this->updateChannelText();
        },
        this->managedConnections_, false);
    // Look -> Style: Compact is lower, Flat has no frame
    getSettings()->uiStyle.connect(
        [this] {
            this->scaleChangedEvent(this->scale());
            this->activity_->updateGeometry();
            this->activity_->update();
            this->update();
        },
        this->managedConnections_, false);
    // ChattiFlexii: Aussehen -> Chat: the bar in one line or in two
    getSettings()->headerTwoRows.connect(
        [this](const auto &, auto) {
            this->updateChannelText();
            this->arrangeParts();
            this->scaleChangedEvent(this->scale());
            this->update();
        },
        this->managedConnections_, false);
    // ...and the small buttons in one row or two
    getSettings()->splitHeaderButtonGrid.connect(
        [this](const auto &, auto) {
            this->arrangeParts();
            this->scaleChangedEvent(this->scale());
            this->update();
        },
        this->managedConnections_, false);

    auto *window = dynamic_cast<BaseWindow *>(this->window());
    if (window)
    {
        // Hack: In some cases Qt doesn't send the leaveEvent the "actual" last mouse receiver.
        // This can happen when quickly moving the mouse out of the window and right clicking.
        // To prevent the tooltip from getting stuck, we use the window's leaveEvent.
        this->managedConnections_.managedConnect(window->leaving, [this] {
            if (this->tooltipWidget_->isVisible())
            {
                this->tooltipWidget_->hide();
            }
        });
    }

    this->scaleChangedEvent(this->scale());
}

void SplitHeader::initializeLayout()
{
    assert(this->layout() == nullptr);

    this->moderationButton_ = new SvgButton(
        {
            .dark = ":/buttons/moderationDisabled-darkMode.svg",
            .light = ":/buttons/moderationDisabled-lightMode.svg",
        },
        this, {5, 5});

    this->chattersButton_ = new SvgButton(
        {
            .dark = ":/buttons/chatters-darkMode.svg",
            .light = ":/buttons/chatters-lightMode.svg",
        },
        this, {4, 4});

    this->trackerButton_ = new SvgButton(
        {
            .dark = ":/buttons/tracker-darkMode.svg",
            .light = ":/buttons/tracker-lightMode.svg",
        },
        this, {5, 5});
    this->trackerButton_->setToolTip("Kanal auf TwitchTracker öffnen");
    this->pinButton_ = new SvgButton(
        {
            .dark = ":/buttons/pinnedMessage-chat.svg",
            .light = ":/buttons/pinnedMessage-chat.svg",
        },
        this, {4, 4});
    this->pinButton_->setToolTip("Angepinnte Nachricht ein-/ausblenden");
    this->pinButton_->setColor(this->theme->isLightTheme()
                                   ? QColor(0x42, 0x42, 0x42)
                                   : QColor(0xc0, 0xc0, 0xc0));
    this->pinButton_->hide();

    // ChattiFlexii: the same for the poll or prediction running right now
    this->voteButton_ = new SvgButton(
        {
            .dark = ":/buttons/poll-chat.svg",
            .light = ":/buttons/poll-chat.svg",
        },
        this, {5, 5});
    this->voteButton_->setToolTip("Umfrage/Vorhersage ein-/ausblenden");
    this->voteButton_->setColor(this->theme->isLightTheme()
                                    ? QColor(0x42, 0x42, 0x42)
                                    : QColor(0xc0, 0xc0, 0xc0));
    this->voteButton_->hide();

    this->hypeButton_ = new SvgButton(
        {
            .dark = ":/buttons/hypetrain-chat.svg",
            .light = ":/buttons/hypetrain-chat.svg",
        },
        this, {5, 5});
    this->hypeButton_->setToolTip("Hype Train ein-/ausblenden");
    this->hypeButton_->setColor(this->theme->isLightTheme()
                                    ? QColor(0x42, 0x42, 0x42)
                                    : QColor(0xc0, 0xc0, 0xc0));
    this->hypeButton_->hide();

    this->addButton_ = new DrawnButton(DrawnButton::Symbol::Plus,
                                       {
                                           .padding = 3,
                                           .thickness = 1,
                                       },
                                       this);

    this->dropdownButton_ =
        new DrawnButton(DrawnButton::Symbol::Kebab, {}, this);

    /// XXX: this never gets disconnected
    QObject::connect(this->dropdownButton_, &Button::leftMousePress, this,
                     [this] {
                         this->dropdownButton_->setMenu(this->createMainMenu());
                     });

    // The room after each picture, before the title - halved again, so
    // the title sits close to what belongs to it
    this->channelPicture_ =
        new HeaderPicture(HeaderPicture::Shape::Round, 3, this);
    this->coverPicture_ =
        new HeaderPicture(HeaderPicture::Shape::Cover, 3, this);
    this->activity_ = new ActivityGraph(this);
    // A click in the curve sends the chat to that moment
    this->activity_->whenClicked([this](const QDateTime &when) {
        this->split_->getChannelView().scrollToTime(when);
    });
    // As wide as there is room for, down to a third of that, so a narrow
    // split keeps its title
    this->activity_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // title
    this->titleLabel_ = makeWidget<HeaderTitle>([](auto w) {
        w->setSizePolicy(QSizePolicy::MinimumExpanding,
                         QSizePolicy::Preferred);
        w->setCentered(true);
        w->setPadding(QMargins{});
        // ChattiFlexii: a title too long for the header ends in "...",
        // rather than running under what is next to it
        w->setShouldElide(true);
    });
    // ChattiFlexii: the second line, where the numbers stand - smaller and
    // paler than the title above it, see themeChangedEvent
    this->statsLabel_ = makeWidget<HeaderTitle>([](auto w) {
        w->setSizePolicy(QSizePolicy::MinimumExpanding,
                         QSizePolicy::Preferred);
        w->setCentered(true);
        w->setPadding(QMargins{});
        w->setShouldElide(true);
        // The same size as the title, only paler - see themeChangedEvent
        w->setFontStyle(FontStyle::UiMedium);
        w->hide();
    });

    // Both lines are what stands in the bar as its title
    auto *titleRows = new QVBoxLayout();
    titleRows->setContentsMargins(0, 0, 0, 0);
    titleRows->setSpacing(0);
    titleRows->addWidget(this->titleLabel_);
    titleRows->addWidget(this->statsLabel_);
    this->titleBox_ = wrapLayout(titleRows);
    this->titleBox_->setSizePolicy(QSizePolicy::MinimumExpanding,
                                   QSizePolicy::Preferred);

    // space - ChattiFlexii: a quarter of what it was, so the title keeps
    // close to what follows it
    this->titleSpace_ = makeWidget<BaseWidget>([](auto w) {
        w->setScaleIndependentSize(2, 4);
    });
    // mode
    this->modeButton_ = makeWidget<LabelButton>([&](auto w) {
        w->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        w->hide();
        w->setMenu(this->createChatModeMenu());
    });

    // ChattiFlexii: where the bar has two lines, the small buttons stand
    // in two rows of their own - half the width for the same buttons
    this->buttonGridLayout_ = new QGridLayout();
    this->buttonGridLayout_->setContentsMargins(0, 0, 0, 0);
    this->buttonGridLayout_->setSpacing(0);
    this->buttonGrid_ = new QWidget(this);
    this->buttonGrid_->setLayout(this->buttonGridLayout_);
    this->buttonGrid_->hide();

    // The space at the start stays first; everything after it stands in
    // the order Buttons -> Title bar asks for - see arrangeParts
    auto *layout = makeLayout<QHBoxLayout>({
        // space
        makeWidget<BaseWidget>([](auto w) {
            w->setScaleIndependentSize(8, 4);
        }),
    });
    this->partsLayout_ = layout;

    QObject::connect(
        this->moderationButton_, &Button::clicked, this,
        [this](Qt::MouseButton button) mutable {
            switch (button)
            {
                case Qt::LeftButton:
                    if (getSettings()->moderationActions.empty())
                    {
                        getApp()->getWindows()->showSettingsDialog(
                            this, SettingsDialogPreference::ModerationActions);
                        this->split_->setModerationMode(true);
                    }
                    else
                    {
                        auto moderationMode = this->split_->getModerationMode();

                        this->split_->setModerationMode(!moderationMode);
                        // w->setDim(moderationMode ? DimButton::Dim::Some
                        //                          : DimButton::Dim::None);
                    }
                    break;

                case Qt::RightButton:
                case Qt::MiddleButton:
                    getApp()->getWindows()->showSettingsDialog(
                        this, SettingsDialogPreference::ModerationActions);
                    break;

                default:
                    break;
            }
        });

    QObject::connect(this->trackerButton_, &Button::leftClicked, this, [this] {
        this->split_->openTrackerInBrowser();
    });

    QObject::connect(this->chattersButton_, &Button::leftClicked, this,
                     [this]() {
                         this->split_->openChatterList();
                     });

    QObject::connect(this->pinButton_, &Button::leftClicked, this, [this]() {
        this->split_->togglePinnedBanner();
    });

    QObject::connect(this->voteButton_, &Button::leftClicked, this, [this]() {
        this->split_->getVoteBanner()->toggleUserPinned();
    });

    QObject::connect(this->hypeButton_, &Button::leftClicked, this, [this]() {
        this->split_->getHypeBanner()->toggleUserPinned();
    });

    QObject::connect(this->addButton_, &Button::leftClicked, this, [this]() {
        this->split_->addSibling();
    });

    getSettings()->customURIScheme.connect(
        [this] {
            if (auto *const drop = this->dropdownButton_)
            {
                drop->setMenu(this->createMainMenu());
            }
        },
        this->managedConnections_);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    this->setLayout(layout);
    this->arrangeParts();

    // ChattiFlexii: the curve takes part of what the title leaves free
    this->titleLabel_->installEventFilter(this);
    this->statsLabel_->installEventFilter(this);

    this->setAddButtonVisible(false);
}

std::unique_ptr<QMenu> SplitHeader::createMainMenu()
{
    // top level menu
    const auto &h = getApp()->getHotkeys();
    auto menu = std::make_unique<QMenu>();
    menu->addAction(
        "Change channel",
        h->getDisplaySequence(HotkeyCategory::Split, "changeChannel"),
        this->split_, &Split::changeChannel);
    menu->addAction("Close",
                    h->getDisplaySequence(HotkeyCategory::Split, "delete"),
                    this->split_, &Split::deleteFromContainer);
    menu->addSeparator();
    menu->addAction(
        "Popup",
        h->getDisplaySequence(HotkeyCategory::Window, "popup", {{"split"}}),
        this->split_, &Split::popup);
    menu->addAction(
        "Popup overlay",
        h->getDisplaySequence(HotkeyCategory::Split, "popupOverlay"),
        this->split_, &Split::showOverlayWindow);
    menu->addAction("Search",
                    h->getDisplaySequence(HotkeyCategory::Split, "showSearch"),
                    this->split_, [this] {
                        this->split_->showSearch(true);
                    });
    menu->addAction("Set filters",
                    h->getDisplaySequence(HotkeyCategory::Split, "pickFilters"),
                    this->split_, &Split::setFiltersDialog);
    menu->addSeparator();

    auto *twitchChannel =
        dynamic_cast<TwitchChannel *>(this->split_->getChannel().get());
    auto *kickChannel =
        dynamic_cast<KickChannel *>(this->split_->getChannel().get());

    if (twitchChannel || kickChannel)
    {
        menu->addAction(
            OPEN_IN_BROWSER,
            h->getDisplaySequence(HotkeyCategory::Split, "openInBrowser"),
            this->split_, &Split::openInBrowser);
        if (twitchChannel)
        {
            menu->addAction(OPEN_PLAYER_IN_BROWSER,
                            h->getDisplaySequence(HotkeyCategory::Split,
                                                  "openPlayerInBrowser"),
                            this->split_, &Split::openBrowserPlayer);
        }
        menu->addAction(
            OPEN_IN_STREAMLINK,
            h->getDisplaySequence(HotkeyCategory::Split, "openInStreamlink"),
            this->split_, &Split::openInStreamlink);

        if (!getSettings()->customURIScheme.getValue().isEmpty())
        {
            menu->addAction("Open in custom player",
                            h->getDisplaySequence(HotkeyCategory::Split,
                                                  "openInCustomPlayer"),
                            this->split_, &Split::openWithCustomScheme);
        }

        if (this->split_->getChannel()->hasModRights())
        {
            menu->addAction(
                OPEN_MOD_VIEW_IN_BROWSER,
                h->getDisplaySequence(HotkeyCategory::Split, "openModView"),
                this->split_, &Split::openModViewInBrowser);
        }

        if (twitchChannel)
        {
            menu->addAction(
                    "Create a clip",
                    h->getDisplaySequence(HotkeyCategory::Split, "createClip"),
                    this->split_,
                    [twitchChannel] {
                        twitchChannel->createClip({}, {});
                    })
                ->setVisible(twitchChannel->isLive());
        }

        if (this->split_->getIndirectChannel().getType() ==
            Channel::Type::TwitchWatching)
        {
            menu->addAction("Reset /watching", this->split_, [] {
                if (!getApp()
                         ->getTwitch()
                         ->getWatchingChannel()
                         .get()
                         ->isEmpty())
                {
                    getApp()->getTwitch()->setWatchingChannel(
                        Channel::getEmpty());
                }
            });
        }

        menu->addSeparator();
    }

    if (this->split_->getChannel()->getType() == Channel::Type::TwitchWhispers)
    {
        menu->addAction(
            OPEN_WHISPERS_IN_BROWSER,
            h->getDisplaySequence(HotkeyCategory::Split, "openInBrowser"),
            this->split_, &Split::openWhispersInBrowser);
        menu->addSeparator();
    }

    // reload / reconnect
    if (this->split_->getChannel()->canReconnect())
    {
        menu->addAction(
            "Reconnect",
            h->getDisplaySequence(HotkeyCategory::Split, "reconnect"), this,
            &SplitHeader::reconnect);
    }

    if (twitchChannel || kickChannel)
    {
        auto bothSeq = h->getDisplaySequence(
            HotkeyCategory::Split, "reloadEmotes", {std::vector<QString>()});
        auto channelSeq = h->getDisplaySequence(HotkeyCategory::Split,
                                                "reloadEmotes", {{"channel"}});
        auto subSeq = h->getDisplaySequence(HotkeyCategory::Split,
                                            "reloadEmotes", {{"subscriber"}});
        menu->addAction("Reload channel emotes",
                        channelSeq.isEmpty() ? bothSeq : channelSeq, this,
                        &SplitHeader::reloadChannelEmotes);
        if (twitchChannel)
        {
            menu->addAction("Reload subscriber emotes",
                            subSeq.isEmpty() ? bothSeq : subSeq, this,
                            &SplitHeader::reloadSubscriberEmotes);
        }
    }

    menu->addSeparator();

    {
        // "How to..." sub menu
        auto *subMenu = new QMenu("How to...", this);
        subMenu->addAction("move split", this->split_, &Split::explainMoving);
        subMenu->addAction("add/split", this->split_, &Split::explainSplitting);
        menu->addMenu(subMenu);
    }

    menu->addSeparator();

    // sub menu
    auto *moreMenu = new QMenu("More", this);

    auto modModeSeq = h->getDisplaySequence(HotkeyCategory::Split,
                                            "setModerationMode", {{"toggle"}});
    if (modModeSeq.isEmpty())
    {
        modModeSeq =
            h->getDisplaySequence(HotkeyCategory::Split, "setModerationMode",
                                  {std::vector<QString>()});
        // this makes a full std::optional<> with an empty vector inside
    }
    moreMenu->addAction(
        "Toggle moderation mode", modModeSeq, this->split_, [this]() {
            this->split_->setModerationMode(!this->split_->getModerationMode());
        });

    if (twitchChannel != nullptr)
    {
        // ChattiFlexii: the hype train banner, for this channel - wherever
        // it is open. In a channel where one rides every other minute it is
        // in the way rather than news.
        auto *action = new QAction(this);
        action->setText("Show hype train");
        action->setCheckable(true);

        // The channel is looked up again each time: a split can be sent to
        // another one while its menu stands open
        const auto channelName = [this]() -> QString {
            auto *channel =
                dynamic_cast<TwitchChannel *>(this->split_->getChannel().get());
            return channel == nullptr ? QString() : channel->getName();
        };

        QObject::connect(
            moreMenu, &QMenu::aboutToShow, this, [action, channelName] {
                const auto name = channelName();
                action->setVisible(getSettings()->showHypeTrainBanner &&
                                   !name.isEmpty());
                action->setChecked(banners::hypeShownIn(name));
            });
        QObject::connect(action, &QAction::triggered, this,
                         [this, channelName](bool on) {
                             banners::setHypeShownIn(channelName(), on);
                             this->updateHypeButton();
                         });

        moreMenu->addAction(action);
    }

    {
        // ChattiFlexii: the curve, for this split alone - in a tab like the
        // mentions, where every channel runs into one, it only gets in the
        // way. The whole tab at once is in the tab's own menu.
        auto *action = new QAction(this);
        action->setText("Show activity graph");
        action->setCheckable(true);

        QObject::connect(moreMenu, &QMenu::aboutToShow, this, [action, this] {
            action->setVisible(getSettings()->splitHeaderActivity);
            action->setChecked(this->split_->getShowActivity());
        });
        QObject::connect(action, &QAction::triggered, this, [this](bool on) {
            this->split_->setShowActivity(on);
        });

        moreMenu->addAction(action);
    }

    if (this->split_->getChannel()->getType() == Channel::Type::TwitchMentions)
    {
        auto *action = new QAction(this);
        action->setText("Enable /mention tab highlights");
        action->setCheckable(true);

        QObject::connect(moreMenu, &QMenu::aboutToShow, this, [action]() {
            action->setChecked(getSettings()->highlightMentions);
        });
        QObject::connect(action, &QAction::triggered, this, []() {
            getSettings()->highlightMentions =
                !getSettings()->highlightMentions;
        });

        moreMenu->addAction(action);
    }

    if (twitchChannel)
    {
        if (twitchChannel->hasModRights())
        {
            moreMenu->addAction(
                "Show chatter list",
                h->getDisplaySequence(HotkeyCategory::Split, "openViewerList"),
                this->split_, &Split::openChatterList);
        }

        moreMenu->addAction("Subscribe",
                            h->getDisplaySequence(HotkeyCategory::Split,
                                                  "openSubscriptionPage"),
                            this->split_, &Split::openSubPage);

        {
            auto *action = new QAction(this);
            action->setText("Notify when live");
            action->setCheckable(true);

            auto notifySeq = h->getDisplaySequence(
                HotkeyCategory::Split, "setChannelNotification", {{"toggle"}});
            if (notifySeq.isEmpty())
            {
                notifySeq = h->getDisplaySequence(HotkeyCategory::Split,
                                                  "setChannelNotification",
                                                  {std::vector<QString>()});
                // this makes a full std::optional<> with an empty vector inside
            }
            action->setShortcut(notifySeq);

            QObject::connect(
                moreMenu, &QMenu::aboutToShow, this, [action, this]() {
                    action->setChecked(
                        getApp()->getNotifications()->isChannelNotified(
                            this->split_->getChannel()->getName(),
                            Platform::Twitch));
                });
            QObject::connect(action, &QAction::triggered, this, [this]() {
                getApp()->getNotifications()->updateChannelNotification(
                    this->split_->getChannel()->getName(), Platform::Twitch);
            });

            moreMenu->addAction(action);
        }

        {
            auto *action = new QAction(this);
            action->setText("Mute highlight sounds");
            action->setCheckable(true);

            auto notifySeq = h->getDisplaySequence(
                HotkeyCategory::Split, "setHighlightSounds", {{"toggle"}});
            if (notifySeq.isEmpty())
            {
                notifySeq = h->getDisplaySequence(HotkeyCategory::Split,
                                                  "setHighlightSounds",
                                                  {std::vector<QString>()});
            }
            action->setShortcut(notifySeq);

            QObject::connect(
                moreMenu, &QMenu::aboutToShow, this, [action, this]() {
                    action->setChecked(getSettings()->isMutedChannel(
                        this->split_->getChannel()->getName()));
                });
            QObject::connect(action, &QAction::triggered, this, [this]() {
                getSettings()->toggleMutedChannel(
                    this->split_->getChannel()->getName());
            });

            moreMenu->addAction(action);
        }
    }

    moreMenu->addSeparator();
    moreMenu->addAction(
        "Clear messages",
        h->getDisplaySequence(HotkeyCategory::Split, "clearMessages"),
        this->split_, &Split::clear);
    //    moreMenu->addSeparator();
    //    moreMenu->addAction("Show changelog", this,
    //    SLOT(moreMenuShowChangelog()));
    menu->addMenu(moreMenu);

    return menu;
}

std::unique_ptr<QMenu> SplitHeader::createChatModeMenu()
{
    auto menu = std::make_unique<QMenu>();

    this->modeActionSetSub = new QAction("Subscriber only", this);
    this->modeActionSetEmote = new QAction("Emote only", this);
    this->modeActionSetSlow = new QAction("Slow", this);
    this->modeActionSetR9k = new QAction("R9K", this);
    this->modeActionSetFollowers = new QAction("Followers only", this);

    this->modeActionSetFollowers->setCheckable(true);
    this->modeActionSetSub->setCheckable(true);
    this->modeActionSetEmote->setCheckable(true);
    this->modeActionSetSlow->setCheckable(true);
    this->modeActionSetR9k->setCheckable(true);

    menu->addAction(this->modeActionSetEmote);
    menu->addAction(this->modeActionSetSub);
    menu->addAction(this->modeActionSetSlow);
    menu->addAction(this->modeActionSetR9k);
    menu->addAction(this->modeActionSetFollowers);

    auto execCommand = [this](const QString &command) {
        auto text = getApp()->getCommands()->execCommand(
            command, this->split_->getChannel(), false);
        this->split_->getChannel()->sendMessage(text);
    };
    auto toggle = [execCommand](const QString &command,
                                QAction *action) mutable {
        execCommand(command + (action->isChecked() ? "" : "off"));
        action->setChecked(!action->isChecked());
    };

    QObject::connect(this->modeActionSetSub, &QAction::triggered, this,
                     [this, toggle]() mutable {
                         toggle("/subscribers", this->modeActionSetSub);
                     });

    QObject::connect(this->modeActionSetEmote, &QAction::triggered, this,
                     [this, toggle]() mutable {
                         toggle("/emoteonly", this->modeActionSetEmote);
                     });

    QObject::connect(this->modeActionSetSlow, &QAction::triggered, this,
                     [this, execCommand]() {
                         if (!this->modeActionSetSlow->isChecked())
                         {
                             execCommand("/slowoff");
                             this->modeActionSetSlow->setChecked(false);
                             return;
                         };
                         auto ok = bool();
                         auto seconds = QInputDialog::getInt(
                             this, "", "Seconds:", 10, 0, 500, 1, &ok,
                             Qt::FramelessWindowHint);
                         if (ok)
                         {
                             execCommand(QString("/slow %1").arg(seconds));
                         }
                         else
                         {
                             this->modeActionSetSlow->setChecked(false);
                         }
                     });

    QObject::connect(this->modeActionSetFollowers, &QAction::triggered, this,
                     [this, execCommand]() {
                         if (!this->modeActionSetFollowers->isChecked())
                         {
                             execCommand("/followersoff");
                             this->modeActionSetFollowers->setChecked(false);
                             return;
                         };
                         auto ok = bool();
                         auto time = QInputDialog::getText(
                             this, "", "Time:", QLineEdit::Normal, "15m", &ok,
                             Qt::FramelessWindowHint,
                             Qt::ImhLowercaseOnly | Qt::ImhPreferNumbers);
                         if (ok)
                         {
                             execCommand(QString("/followers %1").arg(time));
                         }
                         else
                         {
                             this->modeActionSetFollowers->setChecked(false);
                         }
                     });

    QObject::connect(this->modeActionSetR9k, &QAction::triggered, this,
                     [this, toggle]() mutable {
                         toggle("/r9kbeta", this->modeActionSetR9k);
                     });

    return menu;
}

void SplitHeader::updateRoomModes()
{
    assert(this->modeButton_ != nullptr);

    // Update the mode button
    if (auto *twitchChannel =
            dynamic_cast<TwitchChannel *>(this->split_->getChannel().get()))
    {
        this->modeButton_->setEnabled(twitchChannel->hasModRights());

        QString text;
        {
            auto roomModes = twitchChannel->accessRoomModes();
            text = formatRoomModeUnclean(*roomModes);

            // Set menu action
            this->modeActionSetR9k->setChecked(roomModes->r9k);
            this->modeActionSetSlow->setChecked(roomModes->slowMode > 0);
            this->modeActionSetEmote->setChecked(roomModes->emoteOnly);
            this->modeActionSetSub->setChecked(roomModes->submode);
            this->modeActionSetFollowers->setChecked(roomModes->followerOnly !=
                                                     -1);
        }
        cleanRoomModeText(text, twitchChannel->hasModRights());

        // set the label text

        if (!text.isEmpty())
        {
            this->modeButton_->setText(this->twoRows_ ? stackRoomModes(text)
                                                      : text);
            this->modeButton_->setVisible(
                headerparts::isShown(headerparts::Part::Mode));
        }
        else
        {
            this->modeButton_->hide();
        }

        // Update the mode button menu actions
    }
    else if (auto *kc =
                 dynamic_cast<KickChannel *>(this->split_->getChannel().get()))
    {
        this->modeButton_->setEnabled(false);

        QString text = formatRoomModeUnclean(kc->roomModes());
        cleanRoomModeText(text, false);

        if (!text.isEmpty())
        {
            this->modeButton_->setText(this->twoRows_ ? stackRoomModes(text)
                                                      : text);
            this->modeButton_->setVisible(
                headerparts::isShown(headerparts::Part::Mode));
        }
        else
        {
            this->modeButton_->hide();
        }
    }
    else
    {
        this->modeButton_->hide();
    }
}

void SplitHeader::resetThumbnail()
{
    this->lastThumbnail_.invalidate();
    this->thumbnail_.clear();
}

void SplitHeader::handleChannelChanged()
{
    this->resetThumbnail();

    this->updateChannelText();

    this->channelConnections_.clear();

    auto channel = this->split_->getChannel();
    if (auto *twitchChannel = dynamic_cast<TwitchChannel *>(channel.get()))
    {
        this->channelConnections_.managedConnect(
            twitchChannel->streamStatusChanged, [this]() {
                this->updateChannelText();
            });

        this->channelConnections_.managedConnect(
            twitchChannel->pinnedMessageChanged, [this]() {
                this->updatePinButton();
            });

        this->channelConnections_.managedConnect(
            this->split_->getPinnedBanner()->visibilityChanged, [this]() {
                this->updatePinButton();
            });

        // ChattiFlexii: and the same for what the channel votes on
        this->channelConnections_.managedConnect(twitchChannel->pollChanged,
                                                 [this]() {
                                                     this->updateVoteButton();
                                                 });
        this->channelConnections_.managedConnect(
            twitchChannel->predictionChanged, [this]() {
                this->updateVoteButton();
            });
        this->channelConnections_.managedConnect(
            this->split_->getVoteBanner()->visibilityChanged, [this]() {
                this->updateVoteButton();
            });

        this->channelConnections_.managedConnect(
            twitchChannel->hypeTrainChanged, [this]() {
                this->updateHypeButton();
            });
        this->channelConnections_.managedConnect(
            this->split_->getHypeBanner()->visibilityChanged, [this]() {
                this->updateHypeButton();
            });

    }
    else if (auto *kickChannel = dynamic_cast<KickChannel *>(channel.get()))
    {
        this->channelConnections_.managedConnect(kickChannel->streamDataChanged,
                                                 [this]() {
                                                     this->updateChannelText();
                                                 });
    }

    // Whatever the channel is, the banner buttons know where they stand
    this->updatePinButton();
    this->updateVoteButton();
    this->updateHypeButton();
}

void SplitHeader::scaleChangedEvent(float scale)
{
    // Look -> Style: Compact makes the header and its buttons smaller
    int w = int(uistyle::headerHeight() * scale);
    int addSplitWidth =
        int((uistyle::compact() ? ADD_SPLIT_BUTTON_WIDTH - 3
                                : ADD_SPLIT_BUTTON_WIDTH) *
            scale);

    this->applyHeights(scale);
    this->applyPartWidths(w, addSplitWidth, scale);
}

void SplitHeader::applyHeights(float scale)
{
    const int row = uistyle::headerHeight();
    // Only a channel with numbers to show gets a second line
    const int second = this->twoRows_ ? uistyle::headerSecondRow() : 0;

    this->setFixedHeight(int((row + second) * scale));
    this->titleLabel_->setFixedHeight(int(row * scale));
    this->statsLabel_->setFixedHeight(int(std::max(second, 1) * scale));
    this->statsLabel_->setVisible(second > 0);

    // The curve and both pictures stand over the two lines together
    this->activity_->setTallness(row + second);
    const int picture = std::max(16, row + second - 14);
    this->channelPicture_->setTallness(picture);
    this->coverPicture_->setTallness(picture);
}

void SplitHeader::applyPartWidths(int button, int addButton, float scale)
{
    using headerparts::Part;

    // Buttons -> Titelleiste: each part can be dragged wider or narrower in
    // the preview, and the room between them is set there too
    const auto width = [&](Part part, int usual) {
        return std::max(int(headerparts::LEAST_WIDTH * scale),
                        usual + int(headerparts::widthDelta(part) * scale));
    };

    this->dropdownButton_->setFixedWidth(width(Part::Menu, button));
    this->moderationButton_->setFixedWidth(width(Part::Moderation, button));
    this->chattersButton_->setFixedWidth(width(Part::Chatters, button));
    this->trackerButton_->setFixedWidth(width(Part::Tracker, button));
    this->pinButton_->setFixedWidth(width(Part::Pin, button));
    this->voteButton_->setFixedWidth(width(Part::Vote, button));
    this->hypeButton_->setFixedWidth(width(Part::Hype, button));
    this->addButton_->setFixedWidth(width(Part::Add, addButton));

    this->channelPicture_->setExtraWidth(headerparts::widthDelta(Part::Picture));
    this->coverPicture_->setExtraWidth(headerparts::widthDelta(Part::Cover));

    if (this->partsLayout_ != nullptr)
    {
        this->partsLayout_->setSpacing(int(headerparts::spacing() * scale));
    }
}

void SplitHeader::setAddButtonVisible(bool value)
{
    this->addButtonWanted_ = value;
    this->addButton_->setVisible(
        value && headerparts::isShown(headerparts::Part::Add));
    this->fillButtonGrid();
}

void SplitHeader::updatePictures()
{
    if (this->channelPicture_ == nullptr)
    {
        return;
    }
    const auto channel = this->split_->getChannel();
    auto *twitchChannel = dynamic_cast<TwitchChannel *>(channel.get());
    // Buttons -> Title bar can leave out either of them
    const bool picture = twitchChannel != nullptr &&
                         headerparts::isShown(headerparts::Part::Picture);
    const bool cover = twitchChannel != nullptr &&
                       headerparts::isShown(headerparts::Part::Cover);

    // The channel's own picture
    const auto login = picture ? channel->getName().toLower() : QString();
    if (login != this->pictureLogin_)
    {
        this->pictureLogin_ = login;
        this->channelPicture_->setPicture({});
        if (!login.isEmpty())
        {
            profilepictures::pixmap(login, int(32 * this->scale()), this,
                                    [this, login](const QPixmap &picture) {
                                        if (login == this->pictureLogin_)
                                        {
                                            this->channelPicture_->setPicture(
                                                picture);
                                            // The name may go now
                                            if (!getSettings()
                                                     ->headerChannelName)
                                            {
                                                this->updateChannelText();
                                            }
                                        }
                                    });
        }
    }
    this->channelPicture_->setToolTip(channel->getLocalizedName());

    // The cover of what it streams, while it is live
    QString gameId;
    QString game;
    if (cover)
    {
        const auto status = twitchChannel->accessStreamStatus();
        if (status->live)
        {
            gameId = status->gameId;
            game = status->game;
        }
    }
    if (gameId != this->coverGameId_)
    {
        this->coverGameId_ = gameId;
        this->coverPicture_->setPicture({});
        if (!gameId.isEmpty())
        {
            NetworkRequest(
                QStringLiteral(
                    "https://static-cdn.jtvnw.net/ttv-boxart/%1-52x72.jpg")
                    .arg(gameId),
                NetworkRequestType::Get)
                .cache()
                .caller(this)
                .onSuccess([this, gameId](const NetworkResult &result) {
                    QPixmap cover;
                    if (gameId == this->coverGameId_ &&
                        cover.loadFromData(result.getData()))
                    {
                        this->coverPicture_->setPicture(cover);
                    }
                })
                .execute();
        }
    }
    this->coverPicture_->setToolTip(game);

    // How lively the chat was
    // Buttons -> Titelleiste switches the curve off everywhere, and the
    // split's own menu leaves it out of this one alone
    const bool activity =
        getSettings()->splitHeaderActivity && this->split_->getShowActivity();
    this->activity_->setVisible(activity);
    this->activity_->setChannel(activity ? channel : nullptr);
}

headerparts::Extras SplitHeader::channelNumbers(TwitchChannel *channel) const
{
    const auto *settings = getSettings();
    headerparts::Extras extras;
    if (channel == nullptr)
    {
        return extras;
    }

    if (settings->headerMessageRate && this->activity_ != nullptr)
    {
        extras.messagesPerMinute = this->activity_->messagesPerMinute();
        if (settings->headerMessageRateTrend)
        {
            extras.rateTrend = this->activity_->rateTrend();
        }
    }
    if (settings->headerFollowers)
    {
        extras.followers = channelnumbers::followers(channel->roomId());
    }
    if (settings->headerChatters && channel->hasModRights())
    {
        extras.chatters = channelnumbers::chatters(
            channel->roomId(),
            getApp()->getAccounts()->twitch.getCurrent()->getUserId());
    }
    if (settings->headerViewerTrend)
    {
        extras.viewerTrend = channelnumbers::viewerTrend(channel->roomId());
    }
    return extras;
}

void SplitHeader::updatePinButton()
{
    auto channel = this->split_->getChannel();
    auto *twitchChannel = dynamic_cast<TwitchChannel *>(channel.get());
    const bool hasPinnedMessage = twitchChannel != nullptr &&
                                  twitchChannel->getPinnedMessage() != nullptr;

    // Buttons -> Titelleiste can leave the button out altogether
    this->pinButton_->setVisible(
        hasPinnedMessage && headerparts::isShown(headerparts::Part::Pin));
    if (hasPinnedMessage && this->split_->getPinnedBanner()->isVisible())
    {
        this->pinButton_->setColor(this->theme->accent);
    }
    else
    {
        this->pinButton_->setColor(this->theme->isLightTheme()
                                       ? QColor(0x42, 0x42, 0x42)
                                       : QColor(0xc0, 0xc0, 0xc0));
    }

    this->fillButtonGrid();
}

void SplitHeader::updateVoteButton()
{
    auto channel = this->split_->getChannel();
    auto *twitchChannel = dynamic_cast<TwitchChannel *>(channel.get());
    const bool hasVote = twitchChannel != nullptr &&
                         (twitchChannel->currentPoll() != nullptr ||
                          twitchChannel->currentPrediction() != nullptr);

    this->voteButton_->setVisible(
        hasVote && headerparts::isShown(headerparts::Part::Vote));
    if (hasVote && this->split_->getVoteBanner()->isVisible())
    {
        this->voteButton_->setColor(this->theme->accent);
    }
    else
    {
        this->voteButton_->setColor(this->theme->isLightTheme()
                                        ? QColor(0x42, 0x42, 0x42)
                                        : QColor(0xc0, 0xc0, 0xc0));
    }

    this->fillButtonGrid();
}

void SplitHeader::updateHypeButton()
{
    auto channel = this->split_->getChannel();
    auto *twitchChannel = dynamic_cast<TwitchChannel *>(channel.get());
    // ChattiFlexii: a channel the hype train is switched off in has no
    // button for it either
    const bool hasTrain = twitchChannel != nullptr &&
                          twitchChannel->currentHypeTrain() != nullptr &&
                          banners::hypeShownIn(twitchChannel->getName());

    this->hypeButton_->setVisible(
        hasTrain && headerparts::isShown(headerparts::Part::Hype));
    if (hasTrain && this->split_->getHypeBanner()->isVisible())
    {
        this->hypeButton_->setColor(this->theme->accent);
    }
    else
    {
        this->hypeButton_->setColor(this->theme->isLightTheme()
                                        ? QColor(0x42, 0x42, 0x42)
                                        : QColor(0xc0, 0xc0, 0xc0));
    }

    this->fillButtonGrid();
}

void SplitHeader::updateChannelText()
{
    this->updatePictures();

    auto indirectChannel = this->split_->getIndirectChannel();
    auto channel = this->split_->getChannel();
    this->isLive_ = false;
    this->tooltipText_ = QString();

    auto title = channel->getLocalizedName();
    // ChattiFlexii: what follows the name, kept apart so the name can go
    QString afterName;
    // ChattiFlexii: which stretches of it were given a colour
    std::vector<headerparts::Run> runs;
    // ChattiFlexii: the second line and its colours - the numbers, where
    // this channel has any and the bar was asked to stand in two lines
    QString second;
    std::vector<headerparts::Run> secondRuns;
    const bool wasTwoRows = this->twoRows_;
    this->twoRows_ = false;
    // Only once the picture is really there - until it has loaded, or if
    // it never does, the name says whose chat it is
    bool nameCanGo = dynamic_cast<TwitchChannel *>(channel.get()) != nullptr &&
                     headerparts::isShown(headerparts::Part::Picture) &&
                     !this->channelPicture_->isHidden();

    if (indirectChannel.getType() == Channel::Type::TwitchWatching)
    {
        title = "watching: " + (title.isEmpty() ? "none" : title);
        nameCanGo = false;
    }

    if (auto *twitchChannel = dynamic_cast<TwitchChannel *>(channel.get()))
    {
        const auto streamStatus = twitchChannel->accessStreamStatus();

        if (streamStatus->live)
        {
            this->isLive_ = true;
            // XXX: This URL format can be figured out from the Helix Get Streams API which we parse in TwitchChannel::parseLiveStatus
            QString url = "https://static-cdn.jtvnw.net/"
                          "previews-ttv/live_user_" +
                          channel->getName().toLower();
            switch (getSettings()->thumbnailSizeStream.getValue())
            {
                case 1:
                    url.append("-80x45.jpg");
                    break;
                case 2:
                    url.append("-160x90.jpg");
                    break;
                case 3:
                    url.append("-360x203.jpg");
                    break;
                default:
                    url = "";
            }
            if (!url.isEmpty() &&
                (!this->lastThumbnail_.isValid() ||
                 this->lastThumbnail_.elapsed() > THUMBNAIL_MAX_AGE_MS))
            {
                NetworkRequest(url, NetworkRequestType::Get)
                    .caller(this)
                    .onSuccess([this](auto result) {
                        assert(!isAppAboutToQuit());

                        // NOTE: We do not follow the redirects, so we need to make sure we only treat code 200 as a valid image
                        if (result.status() == 200)
                        {
                            this->thumbnail_ = QString::fromLatin1(
                                result.getData().toBase64());
                        }
                        else
                        {
                            this->thumbnail_.clear();
                        }
                        this->updateChannelText();
                    })
                    .execute();
                this->lastThumbnail_.restart();
            }
            this->tooltipText_ = formatTooltip(*streamStatus, this->thumbnail_);
            // ChattiFlexii: the audience over time, so the title can say
            // which way it is going
            channelnumbers::noteViewers(twitchChannel->roomId(),
                                        int(streamStatus->viewerCount));
            const auto numbers = this->channelNumbers(twitchChannel);
            this->twoRows_ = getSettings()->headerTwoRows;
            afterName = headerparts::titleAfterName(
                *streamStatus, numbers, &runs,
                this->twoRows_ ? std::optional(headerparts::Row::First)
                               : std::nullopt);
            if (this->twoRows_)
            {
                second = headerparts::titleSecondLine(*streamStatus, true,
                                                      numbers, &secondRuns);
            }
        }
        else
        {
            this->tooltipText_ = formatOfflineTooltip(*streamStatus);
            const auto numbers = this->channelNumbers(twitchChannel);
            this->twoRows_ = getSettings()->headerTwoRows;
            if (this->twoRows_)
            {
                second = headerparts::titleSecondLine({}, false, numbers,
                                                      &secondRuns);
            }
            else
            {
                afterName = headerparts::extrasAfterName(numbers, &runs);
            }
        }
    }
    else if (auto *kickChannel = dynamic_cast<KickChannel *>(channel.get()))
    {
        const auto &stream = kickChannel->streamData();
        auto twitch = toTwitchStreamStatus(stream);
        if (stream.isLive)
        {
            this->isLive_ = true;
            if (!stream.thumbnailUrl.isEmpty() &&
                (!this->lastThumbnail_.isValid() ||
                 this->lastThumbnail_.elapsed() > THUMBNAIL_MAX_AGE_MS))
            {
                NetworkRequest(stream.thumbnailUrl, NetworkRequestType::Get)
                    .caller(this)
                    .followRedirects(true)
                    .onSuccess([this](const auto &result) {
                        assert(!isAppAboutToQuit());

                        this->thumbnail_ =
                            QString::fromLatin1(result.getData().toBase64());
                        this->updateChannelText();
                    })
                    .execute();
                this->lastThumbnail_.restart();
            }
            this->tooltipText_ = formatTooltip(twitch, this->thumbnail_, true);
            this->twoRows_ = getSettings()->headerTwoRows;
            afterName = headerparts::titleAfterName(
                twitch, {}, &runs,
                this->twoRows_ ? std::optional(headerparts::Row::First)
                               : std::nullopt);
            if (this->twoRows_)
            {
                second =
                    headerparts::titleSecondLine(twitch, true, {}, &secondRuns);
            }
        }
        else
        {
            this->tooltipText_ = formatOfflineTooltip(twitch);
        }
    }

    // Buttons -> Title bar: the name goes only where its picture stands,
    // and every part can carry a colour of its own
    const bool hadName = !title.isEmpty();
    title = headerparts::composeTitle(title, afterName, nameCanGo, &runs);

    if (hadName && !this->split_->getFilters().empty())
    {
        title += title.isEmpty() ? "filtered" : " - filtered";
    }

    this->titleLabel_->setRuns(runs);
    this->titleLabel_->setText(title.isEmpty() && !(hadName && nameCanGo)
                                   ? "<empty>"
                                   : title);
    this->statsLabel_->setRuns(secondRuns);
    this->statsLabel_->setText(second);
    if (this->twoRows_ != wasTwoRows)
    {
        this->applyHeights(this->scale());
        // One line or two decides whether the chat modes stand side by side
        // and whether the buttons stand in two rows
        this->updateRoomModes();
        this->arrangeParts();
    }
    this->fitActivity();
}

void SplitHeader::arrangeParts()
{
    using headerparts::Part;

    auto *layout = this->partsLayout_;
    while (layout->count() > 1)
    {
        delete layout->takeAt(1);
    }
    while (this->buttonGridLayout_->count() > 0)
    {
        delete this->buttonGridLayout_->takeAt(0);
    }

    // ChattiFlexii: two rows of buttons only where there are two lines to
    // put them in
    const bool grid = this->twoRows_ &&
                      getSettings()->splitHeaderButtonGrid &&
                      getSettings()->headerTwoRows;
    this->buttonGrid_->setVisible(grid);
    bool gridPlaced = false;

    for (const auto part : headerparts::order())
    {
        if (grid && headerparts::isButton(part))
        {
            // The block of buttons stands where the first of them does; what
            // goes into which place is settled in fillButtonGrid
            if (!gridPlaced)
            {
                layout->addWidget(this->buttonGrid_);
                gridPlaced = true;
            }
            continue;
        }

        switch (part)
        {
            case Part::Picture:
                layout->addWidget(this->channelPicture_);
                break;
            case Part::Cover:
                layout->addWidget(this->coverPicture_);
                break;
            case Part::Title:
                layout->addWidget(this->titleBox_);
                layout->addWidget(this->titleSpace_);
                break;
            case Part::Activity:
                layout->addWidget(this->activity_);
                break;
            case Part::Mode:
                layout->addWidget(this->modeButton_);
                break;
            case Part::Pin:
                layout->addWidget(this->pinButton_);
                break;
            case Part::Vote:
                layout->addWidget(this->voteButton_);
                break;
            case Part::Hype:
                layout->addWidget(this->hypeButton_);
                break;
            case Part::Moderation:
                layout->addWidget(this->moderationButton_);
                break;
            case Part::Chatters:
                layout->addWidget(this->chattersButton_);
                break;
            case Part::Tracker:
                layout->addWidget(this->trackerButton_);
                break;
            case Part::Menu:
                layout->addWidget(this->dropdownButton_);
                break;
            case Part::Add:
                layout->addWidget(this->addButton_);
                break;
        }
    }

    this->fillButtonGrid();
}

void SplitHeader::fillButtonGrid()
{
    if (this->buttonGridLayout_ == nullptr || this->buttonGrid_->isHidden())
    {
        return;
    }

    while (this->buttonGridLayout_->count() > 0)
    {
        delete this->buttonGridLayout_->takeAt(0);
    }

    int index = 0;
    for (const auto part : headerparts::order())
    {
        if (!headerparts::isButton(part))
        {
            continue;
        }
        auto *button = this->buttonFor(part);
        // A button that is away - no pinned message, no mod rights - leaves
        // no hole behind: the next one moves up into its place
        if (button == nullptr || button->isHidden())
        {
            continue;
        }
        const auto [row, column] = headerparts::buttonPlace(index++);
        this->buttonGridLayout_->addWidget(button, row, column);
    }
}

QWidget *SplitHeader::buttonFor(headerparts::Part part) const
{
    using headerparts::Part;

    switch (part)
    {
        case Part::Pin:
            return this->pinButton_;
        case Part::Vote:
            return this->voteButton_;
        case Part::Hype:
            return this->hypeButton_;
        case Part::Moderation:
            return this->moderationButton_;
        case Part::Chatters:
            return this->chattersButton_;
        case Part::Tracker:
            return this->trackerButton_;
        case Part::Menu:
            return this->dropdownButton_;
        case Part::Add:
            return this->addButton_;
        default:
            return nullptr;
    }
}

void SplitHeader::fitActivity()
{
    if (this->activity_ == nullptr || this->titleLabel_ == nullptr)
    {
        return;
    }
    if (this->activity_->isHidden())
    {
        this->activity_->setWantedWidth(0);
        return;
    }

    // Title and curve share what the rest of the header leaves. Unless the
    // curve was given a share of its own, the title gets what it needs
    // first, a long one all of it, and the curve half of what is left over
    // - which halves the gap either side of the title.
    const auto shared = this->titleLabel_->width() + this->activity_->width();
    const auto roomFor = [this](HeaderTitle *label) {
        return static_cast<int>(std::ceil(
            getApp()
                ->getFonts()
                ->getFontMetrics(label->getFontStyle(), label->scale())
                .horizontalAdvance(label->getText())));
    };
    // ChattiFlexii: with two lines the longer of them says what the title
    // needs - otherwise the curve would cut the numbers short
    auto needed = roomFor(this->titleLabel_);
    if (this->twoRows_)
    {
        needed = std::max(needed, roomFor(this->statsLabel_));
    }
    this->activity_->setWantedWidth(headerparts::curveWidth(
        shared, needed, this->activity_->ownWidth(),
        getSettings()->splitHeaderActivityShare,
        int(headerparts::TITLE_KEEPS * this->scale())));
}

bool SplitHeader::eventFilter(QObject *watched, QEvent *event)
{
    // The title changes size whenever the header or what is in it does
    if ((watched == this->titleLabel_ || watched == this->statsLabel_) &&
        event->type() == QEvent::Resize)
    {
        this->fitActivity();
    }
    return BaseWidget::eventFilter(watched, event);
}

void SplitHeader::updateIcons()
{
    auto channel = this->split_->getChannel();

    if (channel->isTwitchOrKickChannel())
    {
        auto moderationMode = this->split_->getModerationMode() &&
                              !getSettings()->moderationActions.empty();

        if (moderationMode)
        {
            this->moderationButton_->setSource({
                .dark = ":/buttons/moderationEnabled-darkMode.svg",
                .light = ":/buttons/moderationEnabled-lightMode.svg",
            });
        }
        else
        {
            this->moderationButton_->setSource({
                .dark = ":/buttons/moderationDisabled-darkMode.svg",
                .light = ":/buttons/moderationDisabled-lightMode.svg",
            });
        }

        if (channel->hasModRights() || moderationMode)
        {
            this->moderationButton_->setVisible(
                headerparts::isShown(headerparts::Part::Moderation));
        }
        else
        {
            this->moderationButton_->hide();
        }

        if (channel->hasModRights() && channel->isTwitchChannel())
        {
            this->chattersButton_->setVisible(
                headerparts::isShown(headerparts::Part::Chatters));
        }
        else
        {
            this->chattersButton_->hide();
        }

        // TwitchTracker knows a Twitch channel alone - not Kick, and not
        // the tabs that only gather messages, like Erwähnungen or User
        this->trackerButton_->setVisible(
            dynamic_cast<TwitchChannel *>(channel.get()) != nullptr &&
            headerparts::isShown(headerparts::Part::Tracker));
    }
    else
    {
        this->moderationButton_->hide();
        this->chattersButton_->hide();
        this->trackerButton_->hide();
    }

    this->fillButtonGrid();
}

void SplitHeader::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);

    QColor background = this->theme->splits.header.background;
    QColor border = this->theme->splits.header.border;

    if (this->split_->hasFocus())
    {
        background = this->theme->splits.header.focusedBackground;
        border = this->theme->splits.header.focusedBorder;
    }

    painter.fillRect(this->rect(), background);
    // Look -> Style: Flat does without the frame
    if (!uistyle::flat())
    {
        painter.setPen(border);
        painter.drawRect(0, 0, this->width() - 1, this->height() - 2);
        painter.fillRect(0, this->height() - 1, this->width(), 1,
                         background);
    }
}

void SplitHeader::mousePressEvent(QMouseEvent *event)
{
    switch (event->button())
    {
        case Qt::LeftButton: {
            this->split_->setFocus(Qt::MouseFocusReason);

            this->dragging_ = true;

            this->dragStart_ = event->pos();
        }
        break;

        case Qt::RightButton: {
            auto *menu = this->createMainMenu().release();
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->popup(this->mapToGlobal(event->pos() + QPoint(0, 4)));
        }
        break;

        case Qt::MiddleButton: {
            this->split_->openInBrowser();
        }
        break;

        default: {
        }
        break;
    }

    this->doubleClicked_ = false;
}

void SplitHeader::mouseReleaseEvent(QMouseEvent * /*event*/)
{
    this->dragging_ = false;
}

void SplitHeader::mouseMoveEvent(QMouseEvent *event)
{
    if (this->dragging_)
    {
        if (distance(this->dragStart_, event->pos()) > 15 * this->scale())
        {
            this->split_->drag();
            this->dragging_ = false;
        }
    }
}

void SplitHeader::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->split_->changeChannel();
    }
    this->doubleClicked_ = true;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SplitHeader::enterEvent(QEnterEvent *event)
#else
void SplitHeader::enterEvent(QEvent *event)
#endif
{
    if (!this->tooltipText_.isEmpty())
    {
        this->tooltipWidget_->setOne({nullptr, this->tooltipText_});
        this->tooltipWidget_->setWordWrap(true);
        this->tooltipWidget_->adjustSize();

        // On Windows, a lot of the resizing/activating happens when calling
        // show() and calling it doesn't synchronously create a visible window,
        // so moving the window won't cause the visible window to jump.
        //
        // On other platforms, this isn't the case, hence we call show() after
        // moving.
#ifdef Q_OS_WIN
        this->tooltipWidget_->show();
#endif

        auto pos =
            this->mapToGlobal(this->rect().bottomLeft()) +
            QPoint((this->width() - this->tooltipWidget_->width()) / 2, 1);

        this->tooltipWidget_->moveTo(pos,
                                     widgets::BoundsChecking::CursorPosition);

#ifndef Q_OS_WIN
        this->tooltipWidget_->show();
#endif
    }

    BaseWidget::enterEvent(event);
}

void SplitHeader::leaveEvent(QEvent *event)
{
    this->tooltipWidget_->hide();

    BaseWidget::leaveEvent(event);
}

void SplitHeader::themeChangedEvent()
{
    auto palette = QPalette();

    if (this->split_->hasFocus())
    {
        palette.setColor(QPalette::WindowText,
                         this->theme->splits.header.focusedText);
    }
    else
    {
        palette.setColor(QPalette::WindowText, this->theme->splits.header.text);
    }
    this->titleLabel_->setPalette(palette);

    // ChattiFlexii: the numbers underneath stand back a little
    auto second = palette;
    auto pale = palette.color(QPalette::WindowText);
    pale.setAlphaF(0.72);
    second.setColor(QPalette::WindowText, pale);
    this->statsLabel_->setPalette(second);

    // Re-apply pin button color to respect updated theme
    this->updatePinButton();

    auto bg = this->theme->splits.header.background;
    this->addButton_->setOptions({
        .background = bg,
        .backgroundHover = bg,
    });

    this->update();
}

void SplitHeader::reloadChannelEmotes()
{
    using namespace std::chrono_literals;

    auto now = std::chrono::steady_clock::now();
    if (this->lastReloadedChannelEmotes_ + 30s > now)
    {
        return;
    }
    this->lastReloadedChannelEmotes_ = now;

    auto channel = this->split_->getChannel();

    if (auto *twitchChannel = dynamic_cast<TwitchChannel *>(channel.get()))
    {
        twitchChannel->refreshFFZChannelEmotes(true);
        twitchChannel->refreshBTTVChannelEmotes(true);
        twitchChannel->refreshSevenTVChannelEmotes(true);
    }
    else if (auto *kc = dynamic_cast<KickChannel *>(channel.get()))
    {
        kc->reloadSeventvEmotes(true);
    }
}

void SplitHeader::reloadSubscriberEmotes()
{
    using namespace std::chrono_literals;

    auto now = std::chrono::steady_clock::now();
    if (this->lastReloadedSubEmotes_ + 30s > now)
    {
        return;
    }
    this->lastReloadedSubEmotes_ = now;

    auto channel = this->split_->getChannel();
    if (auto *twitchChannel = dynamic_cast<TwitchChannel *>(channel.get()))
    {
        twitchChannel->refreshTwitchChannelEmotes(true);
    }
}

void SplitHeader::reconnect()
{
    this->split_->getChannel()->reconnect();
}

}  // namespace chatterino
