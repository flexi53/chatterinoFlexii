// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitHeader.hpp"

#include "Application.hpp"
#include "common/network/NetworkCommon.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/hotkeys/Hotkey.hpp"
#include "controllers/hotkeys/HotkeyCategory.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
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
#include "widgets/helper/CommonTexts.hpp"
#include "widgets/Label.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/splits/HeaderParts.hpp"
#include "widgets/splits/SplitHeaderExtras.hpp"
#include "widgets/TooltipWidget.hpp"

#include <QDrag>
#include <QHBoxLayout>
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
            this->setAddButtonVisible(this->addButtonWanted_);
        },
        this->managedConnections_, false);
    getSettings()->splitHeaderActivityShare.connect(
        [this] {
            this->fitActivity();
        },
        this->managedConnections_, false);
    // Look -> Style: Compact is lower, Flat has no frame
    getSettings()->uiStyle.connect(
        [this] {
            this->scaleChangedEvent(this->scale());
            this->activity_->setFixedHeight(
                int(uistyle::headerHeight() * this->scale()));
            this->activity_->updateGeometry();
            this->activity_->update();
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
    // As wide as there is room for, down to a third of that, so a narrow
    // split keeps its title
    this->activity_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // title
    this->titleLabel_ = makeWidget<Label>([](auto w) {
        w->setSizePolicy(QSizePolicy::MinimumExpanding,
                         QSizePolicy::Preferred);
        w->setCentered(true);
        w->setPadding(QMargins{});
        // ChattiFlexii: a title too long for the header ends in "...",
        // rather than running under what is next to it
        w->setShouldElide(true);
    });
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

    QObject::connect(this->chattersButton_, &Button::leftClicked, this,
                     [this]() {
                         this->split_->openChatterList();
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
            this->modeButton_->setText(text);
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
            this->modeButton_->setText(text);
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
    }
    else if (auto *kickChannel = dynamic_cast<KickChannel *>(channel.get()))
    {
        this->channelConnections_.managedConnect(kickChannel->streamDataChanged,
                                                 [this]() {
                                                     this->updateChannelText();
                                                 });
    }
}

void SplitHeader::scaleChangedEvent(float scale)
{
    // Look -> Style: Compact makes the header and its buttons smaller
    int w = int(uistyle::headerHeight() * scale);
    int addSplitWidth =
        int((uistyle::compact() ? ADD_SPLIT_BUTTON_WIDTH - 3
                                : ADD_SPLIT_BUTTON_WIDTH) *
            scale);

    this->setFixedHeight(w);
    this->dropdownButton_->setFixedWidth(w);
    this->moderationButton_->setFixedWidth(w);
    this->chattersButton_->setFixedWidth(w);

    this->addButton_->setFixedWidth(addSplitWidth);
}

void SplitHeader::setAddButtonVisible(bool value)
{
    this->addButtonWanted_ = value;
    this->addButton_->setVisible(
        value && headerparts::isShown(headerparts::Part::Add));
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
    const bool activity = getSettings()->splitHeaderActivity;
    this->activity_->setVisible(activity);
    this->activity_->setChannel(activity ? channel : nullptr);
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
            afterName = headerparts::titleAfterName(*streamStatus);
        }
        else
        {
            this->tooltipText_ = formatOfflineTooltip(*streamStatus);
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
            afterName = headerparts::titleAfterName(twitch);
        }
        else
        {
            this->tooltipText_ = formatOfflineTooltip(twitch);
        }
    }

    // Buttons -> Title bar: the name goes only where its picture stands
    const bool hadName = !title.isEmpty();
    title = headerparts::composeTitle(title, afterName, nameCanGo);

    if (hadName && !this->split_->getFilters().empty())
    {
        title += title.isEmpty() ? "filtered" : " - filtered";
    }

    this->titleLabel_->setText(title.isEmpty() && !(hadName && nameCanGo)
                                   ? "<empty>"
                                   : title);
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

    for (const auto part : headerparts::order())
    {
        switch (part)
        {
            case Part::Picture:
                layout->addWidget(this->channelPicture_);
                break;
            case Part::Cover:
                layout->addWidget(this->coverPicture_);
                break;
            case Part::Title:
                layout->addWidget(this->titleLabel_);
                layout->addWidget(this->titleSpace_);
                break;
            case Part::Activity:
                layout->addWidget(this->activity_);
                break;
            case Part::Mode:
                layout->addWidget(this->modeButton_);
                break;
            case Part::Moderation:
                layout->addWidget(this->moderationButton_);
                break;
            case Part::Chatters:
                layout->addWidget(this->chattersButton_);
                break;
            case Part::Menu:
                layout->addWidget(this->dropdownButton_);
                break;
            case Part::Add:
                layout->addWidget(this->addButton_);
                break;
        }
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
    const auto needed = static_cast<int>(std::ceil(
        getApp()
            ->getFonts()
            ->getFontMetrics(this->titleLabel_->getFontStyle(),
                             this->titleLabel_->scale())
            .horizontalAdvance(this->titleLabel_->getText())));
    this->activity_->setWantedWidth(headerparts::curveWidth(
        shared, needed, this->activity_->ownWidth(),
        getSettings()->splitHeaderActivityShare,
        int(headerparts::TITLE_KEEPS * this->scale())));
}

bool SplitHeader::eventFilter(QObject *watched, QEvent *event)
{
    // The title changes size whenever the header or what is in it does
    if (watched == this->titleLabel_ && event->type() == QEvent::Resize)
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
    }
    else
    {
        this->moderationButton_->hide();
        this->chattersButton_->hide();
    }
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
