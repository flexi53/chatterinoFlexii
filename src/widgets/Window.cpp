// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/Window.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "common/Common.hpp"
#include "common/Credentials.hpp"
#include "common/Modes.hpp"
#include "common/QLogging.hpp"
#include "common/Version.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Resources.hpp"
#include "singletons/Settings.hpp"
#include "singletons/StreamerMode.hpp"
#include "singletons/Theme.hpp"
#include "singletons/Updates.hpp"
#include "controllers/splits/PinnedSplits.hpp"
#include "singletons/WindowManager.hpp"
#include "util/RapidJsonSerializeQSize.hpp"
#include "widgets/AccountSwitchPopup.hpp"
#include "widgets/buttons/InitUpdateButton.hpp"
#include "widgets/buttons/LabelButton.hpp"
#include "widgets/buttons/PixmapButton.hpp"
#include "widgets/buttons/TitlebarButton.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/dialogs/switcher/QuickSwitcherPopup.hpp"
#include "widgets/dialogs/UpdateDialog.hpp"
#include "widgets/dialogs/WelcomeDialog.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/ClosedSplits.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"

#ifndef NDEBUG
#    include "providers/twitch/PubSubManager.hpp"
#    include "providers/twitch/PubSubMessages.hpp"
#    include "util/SampleData.hpp"

#    include <rapidjson/document.h>
#endif

#include <QApplication>
#include <QDesktopServices>
#include <QHeaderView>
#include <QMenuBar>
#include <QObject>
#include <QPalette>
#include <QStandardItemModel>
#include <QSplitter>
#include <QMouseEvent>
#include <QPainter>
#include <QSplitterHandle>
#include <QVBoxLayout>

#include <algorithm>

namespace chatterino {

namespace {

/// ChattiFlexii: the notebook of the part that stays at the bottom - one
/// page, no tabs of its own, nothing to add or close
/// ChattiFlexii: the line between the tabs and the part at the bottom.
/// Dragging it moves the border, as any splitter does; holding Alt leaves
/// empty room between the two instead - the same hand movement as between
/// two chats, see SplitContainer::ResizeHandle.
class GapHandle : public QSplitterHandle
{
public:
    GapHandle(Qt::Orientation orientation, QSplitter *parent)
        : QSplitterHandle(orientation, parent)
    {
    }

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        // ChattiFlexii: the same filling the room between two chats has,
        // so a window reads as one whether the gap sits inside a tab or
        // over the part kept at the bottom
        QPainter painter(this);
        SplitContainer::fillGap(painter, QRectF(this->rect()), getTheme(),
                                this);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        this->tookAt_ = event->globalPosition().toPoint().y();
        this->tookWidth_ = this->splitter()->handleWidth();
        QSplitterHandle::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!event->modifiers().testFlag(Qt::AltModifier))
        {
            QSplitterHandle::mouseMoveEvent(event);
            return;
        }

        // Up grows the room, down takes it away again
        const int moved = this->tookAt_ - event->globalPosition().toPoint().y();
        this->splitter()->setHandleWidth(
            std::clamp(this->tookWidth_ + moved, LEAST_GAP, MOST_GAP));
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        // Back to a plain line
        this->splitter()->setHandleWidth(LEAST_GAP);
        QSplitterHandle::mouseDoubleClickEvent(event);
    }

private:
    static constexpr int LEAST_GAP = 4;
    static constexpr int MOST_GAP = 160;

    int tookAt_ = 0;
    int tookWidth_ = 0;
};

/// A splitter whose line can be taken hold of that way
class GapSplitter : public QSplitter
{
public:
    GapSplitter(Qt::Orientation orientation, QWidget *parent)
        : QSplitter(orientation, parent)
    {
    }

protected:
    QSplitterHandle *createHandle() override
    {
        return new GapHandle(this->orientation(), this);
    }
};

class PinnedNotebook : public SplitNotebook
{
public:
    explicit PinnedNotebook(Window *parent)
        : SplitNotebook(parent)
    {
        this->setShowTabsQuietly(false);
        this->setAllowUserTabManagement(false);
        this->setShowAddButton(false);
        // No bar of its own either - the chats start right at the divider
        this->setCustomButtonsHidden(true);
    }
};

}  // namespace

Window::Window(WindowType type, QWidget *parent)
    : BaseWindow(
          {BaseWindow::EnableCustomFrame, BaseWindow::ClearBuffersOnDpiChange},
          parent)
    , type_(type)
    , notebook_(new SplitNotebook(this))
{
    this->addCustomTitlebarButtons();
    this->addShortcuts();
    this->addLayout();

#ifdef Q_OS_MACOS
    this->addMenuBar();
#endif

    this->bSignals_.emplace_back(
        getApp()->getAccounts()->twitch.currentUserChanged.connect([this] {
            this->onAccountSelected();
        }));
    this->onAccountSelected();

    if (type == WindowType::Main)
    {
        this->resize(int(600 * this->scale()), int(500 * this->scale()));
#ifdef Q_OS_LINUX
        if (this->theme->window.background.alpha() != 255)
        {
            this->setAttribute(Qt::WA_TranslucentBackground);
        }
#endif
    }
    else
    {
        auto lastPopup = getSettings()->lastPopupSize.getValue();
        if (lastPopup.isEmpty())
        {
            // The size in the setting was invalid, use the default value
            lastPopup = getSettings()->lastPopupSize.getDefaultValue();
        }
        this->resize(lastPopup.width(), lastPopup.height());
    }

    this->signalHolder_.managedConnect(getApp()->getHotkeys()->onItemsUpdated,
                                       [this]() {
                                           this->clearShortcuts();
                                           this->addShortcuts();
                                       });
    if (type == WindowType::Main || type == WindowType::Popup)
    {
        getSettings()->tabDirection.connect(
            [this](int val) {
                this->notebook_->setTabLocation(NotebookTabLocation(val));
            },
            this->signalHolder_);
    }
}

WindowType Window::getType()
{
    return this->type_;
}

SplitNotebook &Window::getNotebook()
{
    return *this->notebook_;
}

bool Window::event(QEvent *event)
{
    switch (event->type())
    {
        case QEvent::WindowActivate: {
            getApp()->getWindows()->selectedWindow_ = this;
            break;
        }

        case QEvent::WindowDeactivate: {
            auto *page = this->notebook_->getSelectedPage();

            if (page != nullptr)
            {
                std::vector<Split *> splits = page->getSplits();
                for (Split *split : splits)
                {
                    split->unpause();
                    split->updateLastReadMessage();
                }

                page->hideResizeHandles();
            }
        }
        break;

        default:;
    }

    return BaseWindow::event(event);
}

void Window::closeEvent(QCloseEvent *)
{
    if (isAppAboutToQuit())
    {
        qCWarning(chatterinoWidget)
            << "Window closeEvent ran when Application is already dead";
        return;
    }

    auto *app = getApp();

    if (this->type_ == WindowType::Main)
    {
        app->getWindows()->save();
        app->getWindows()->closeAll();
    }
    else
    {
        QRect rect = this->getBounds();
        QSize newSize(rect.width(), rect.height());
        getSettings()->lastPopupSize.setValue(newSize);
    }
    // Ensure selectedWindow_ is never an invalid pointer.
    // WindowManager will return the main window if no window is pointed to by
    // `selectedWindow_`.
    app->getWindows()->selectedWindow_ = nullptr;

    this->closed.invoke();

    if (this->type_ == WindowType::Main)
    {
        QApplication::exit();
    }
}

void Window::addLayout()
{
    auto *layout = new QVBoxLayout();

    // ChattiFlexii: under the tabs a part that stays put. What is held
    // there is shown whichever tab is open above it - one window instead
    // of two. It has a page of its own and no tabs.
    this->pinnedNotebook_ = new PinnedNotebook(this);
    this->pinnedNotebook_->getOrAddSelectedPage();
    this->pinnedNotebook_->hide();
    this->pinnedNotebook_->getOrAddSelectedPage()->installEventFilter(this);

    this->splitter_ = new GapSplitter(Qt::Vertical, this);
    this->splitter_->setChildrenCollapsible(false);
    this->splitter_->setHandleWidth(4);
    this->splitter_->addWidget(this->notebook_);
    this->splitter_->addWidget(this->pinnedNotebook_);
    this->splitter_->setStretchFactor(0, 1);
    this->splitter_->setStretchFactor(1, 0);
    QObject::connect(this->splitter_, &QSplitter::splitterMoved, this,
                     [](int, int) {
                         getApp()->getWindows()->queueSave();
                     });

    layout->addWidget(this->splitter_);
    this->getLayoutContainer()->setLayout(layout);

    // ChattiFlexii: the lower part follows the one list that holds for
    // every window - changed in one place, there in all of them
    getSettings()->pinnedSplits.connect(
        [this](const auto &, auto) {
            this->applyPinnedSplits();
        },
        this->signalHolder_, false);

    // set margin
    layout->setContentsMargins(0, 0, 0, 0);

    this->notebook_->setAllowUserTabManagement(true);
    this->notebook_->setShowAddButton(true);

    // Once the window stands, what belongs at its bottom is put there
    QTimer::singleShot(0, this, [this] {
        this->applyPinnedSplits();
    });
}

bool Window::eventFilter(QObject *watched, QEvent *event)
{
    // ChattiFlexii: a chat taken out of the lower part - once the last one
    // is gone the part goes with it. The child list is still in flux while
    // the event runs, so the look comes a moment later.
    if (this->pinnedNotebook_ != nullptr &&
        watched == this->pinnedNotebook_->getSelectedPage() &&
        (event->type() == QEvent::ChildAdded ||
         event->type() == QEvent::ChildRemoved))
    {
        QTimer::singleShot(0, this, [this] {
            this->refreshPinnedArea();
        });
    }

    return BaseWindow::eventFilter(watched, event);
}

SplitContainer *Window::getPinnedContainer()
{
    // Always the one page it was given - never a second one
    if (auto *page = dynamic_cast<SplitContainer *>(
            this->pinnedNotebook_->getPageAt(0)))
    {
        return page;
    }
    return this->pinnedNotebook_->getOrAddSelectedPage();
}

void Window::applyPinnedSplits()
{
    auto *container = this->getPinnedContainer();
    const auto wanted = pinnedsplits::held();

    // What is no longer wanted goes
    for (auto *split : container->getSplits())
    {
        if (!wanted.contains(pinnedsplits::nameOf(split->getChannel())))
        {
            container->deleteSplit(split);
        }
    }

    // ...and what is missing comes, in the order of the list
    for (const auto &entry : wanted)
    {
        bool there = false;
        for (auto *split : container->getSplits())
        {
            there = there || pinnedsplits::nameOf(split->getChannel()) == entry;
        }
        if (there)
        {
            continue;
        }

        SplitDescriptor descriptor;
        if (entry.startsWith("twitch:"))
        {
            descriptor.type_ = "twitch";
            descriptor.channelName_ = entry.mid(7);
        }
        else
        {
            descriptor.type_ = entry;
        }

        auto *split = new Split(container);
        split->setChannel(WindowManager::decodeChannel(descriptor));
        container->insertSplit(split, {});
    }

    this->refreshPinnedArea();
}

bool Window::hasPinnedSplits() const
{
    if (this->pinnedNotebook_ == nullptr)
    {
        return false;
    }
    auto *page = dynamic_cast<SplitContainer *>(
        this->pinnedNotebook_->getSelectedPage());
    return page != nullptr && !page->getSplits().empty();
}

int Window::pinnedHeight() const
{
    if (!this->hasPinnedSplits() || this->splitter_ == nullptr)
    {
        return 0;
    }
    return this->splitter_->sizes().value(1);
}

int Window::pinnedGap() const
{
    return this->splitter_ == nullptr ? 0 : this->splitter_->handleWidth();
}

void Window::setPinnedGap(int gap)
{
    if (this->splitter_ == nullptr || gap <= 0)
    {
        return;
    }
    this->splitter_->setHandleWidth(std::clamp(gap, 4, 160));
}

void Window::refreshPinnedArea(int height)
{
    if (this->pinnedNotebook_ == nullptr || this->splitter_ == nullptr)
    {
        return;
    }

    const bool anything = this->hasPinnedSplits();
    this->pinnedNotebook_->setVisible(anything);
    if (!anything)
    {
        return;
    }

    if (height > 0)
    {
        const int whole = this->splitter_->height();
        const int lower = std::clamp(height, 60, std::max(60, whole - 120));
        this->splitter_->setSizes({whole - lower, lower});
    }
    else if (this->splitter_->sizes().value(1) <= 0)
    {
        // Coming up for the first time: a third of the window, no more
        const int whole = this->splitter_->height();
        const int lower = std::max(120, whole / 3);
        this->splitter_->setSizes({whole - lower, lower});
    }
}

void Window::addCustomTitlebarButtons()
{
    if (!this->hasCustomWindowFrame())
    {
        return;
    }
    if (this->type_ != WindowType::Main)
    {
        return;
    }

    // settings
    this->addTitleBarButton<TitleBarButton>(
        [this] {
            getApp()->getWindows()->showSettingsDialog(this);
        },
        TitleBarButtonStyle::Settings);

    // updates
    auto *update = this->addTitleBarButton<PixmapButton>([] {});

    initUpdateButton(*update, [] {}, this->signalHolder_);

    // account
    this->userLabel_ = this->addTitleBarLabel([this] {
        getApp()->getWindows()->showAccountSelectPopup(
            this->userLabel_->mapToGlobal(
                this->userLabel_->rect().bottomLeft()));
    });
    this->userLabel_->setMinimumWidth(20 * this->scale());

    // streamer mode
    this->streamerModeTitlebarIcon_ =
        this->addTitleBarButton<PixmapButton>([this] {
            getApp()->getWindows()->showSettingsDialog(
                this, SettingsDialogPreference::StreamerMode);
        });
    QObject::connect(getApp()->getStreamerMode(), &IStreamerMode::changed, this,
                     &Window::updateStreamerModeIcon);

    // Update initial state
    this->updateStreamerModeIcon();
}

void Window::updateStreamerModeIcon()
{
    // A duplicate of this code is in SplitNotebook class (in Notebook.{c,h}pp)
    // That one is the one near splits (on linux and mac or non-main windows on Windows)
    // This copy handles the TitleBar icon in Window (main window on Windows)
    if (this->streamerModeTitlebarIcon_ == nullptr)
    {
        return;
    }
#ifdef Q_OS_WIN
    assert(this->getType() == WindowType::Main);
    if (getTheme()->isLightTheme())
    {
        this->streamerModeTitlebarIcon_->setPixmap(
            getResources().buttons.streamerModeEnabledLight);
    }
    else
    {
        this->streamerModeTitlebarIcon_->setPixmap(
            getResources().buttons.streamerModeEnabledDark);
    }
    this->streamerModeTitlebarIcon_->setVisible(
        getApp()->getStreamerMode()->isEnabled());
#else
    // clang-format off
    assert(false && "Streamer mode TitleBar icon should not exist on non-Windows OSes");
    // clang-format on
#endif
}

void Window::themeChangedEvent()
{
    this->updateStreamerModeIcon();
    BaseWindow::themeChangedEvent();
}

void Window::addDebugStuff(HotkeyController::HotkeyMap &actions)
{
#ifndef NDEBUG
    actions.emplace("addMiscMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleMiscMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addCheerMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleCheerMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addLinkMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleLinkMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addRewardMessage", [=](std::vector<QString>) -> QString {
        rapidjson::Document doc;
        static bool alt = true;
        if (alt)
        {
            auto oMessage =
                parsePubSubBaseMessage(getSampleChannelRewardMessage());
            auto oInnerMessage =
                oMessage->toInner<PubSubMessageMessage>()
                    ->toInner<PubSubCommunityPointsChannelV1Message>();

            getApp()->getTwitch()->addFakeMessage(
                getSampleChannelRewardIRCMessage());
            getApp()->getTwitchPubSub()->pointReward.redeemed.invoke(
                oInnerMessage->data.value("redemption").toObject());
            alt = !alt;
        }
        else
        {
            auto oMessage =
                parsePubSubBaseMessage(getSampleChannelRewardMessage2());
            auto oInnerMessage =
                oMessage->toInner<PubSubMessageMessage>()
                    ->toInner<PubSubCommunityPointsChannelV1Message>();
            getApp()->getTwitchPubSub()->pointReward.redeemed.invoke(
                oInnerMessage->data.value("redemption").toObject());
            alt = !alt;
        }
        return "";
    });

    actions.emplace("addEmoteMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleEmoteTestMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addSubMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = getSampleSubMessages();
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->getTwitch()->addFakeMessage(msg);
        return "";
    });
#endif
}

void Window::addShortcuts()
{
    HotkeyController::HotkeyMap actions{
        {"openSettings",  // Open settings
         [this](std::vector<QString>) -> QString {
             SettingsDialog::showDialog(this);
             return "";
         }},
        {"openAccountSelector",  // Open account selector
         [](const std::vector<QString> &) -> QString {
             getApp()->getWindows()->showAccountSelectPopup({0, 0});
             return "";
         }},
        {"newSplit",  // Create a new split
         [this](std::vector<QString>) -> QString {
             this->notebook_->getOrAddSelectedPage()->appendNewSplit(true);
             return "";
         }},
        {"openTab",  // CTRL + 1-8 to open corresponding tab.
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "openTab shortcut called without arguments. "
                        "Takes only "
                        "one argument: tab specifier";
                 return "openTab shortcut called without arguments. "
                        "Takes only "
                        "one argument: tab specifier";
             }
             auto target = arguments.at(0);
             if (target == "last")
             {
                 this->notebook_->selectLastTab();
             }
             else if (target == "next")
             {
                 this->notebook_->selectNextTab();
             }
             else if (target == "previous")
             {
                 this->notebook_->selectPreviousTab();
             }
             else
             {
                 bool ok;
                 int result = target.toInt(&ok);
                 if (ok)
                 {
                     this->notebook_->selectVisibleIndex(result);
                 }
                 else
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for openTab shortcut";
                     return QString("Invalid argument for openTab "
                                    "shortcut: \"%1\". Use \"last\", "
                                    "\"next\", \"previous\" or an integer.")
                         .arg(target);
                 }
             }
             return "";
         }},
        {"popup",
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 return "popup action called without arguments. Takes only "
                        "one: \"split\" or \"window\".";
             }
             if (arguments.at(0) == "split")
             {
                 if (auto *page = dynamic_cast<SplitContainer *>(
                         this->notebook_->getSelectedPage()))
                 {
                     if (auto *split = page->getSelectedSplit())
                     {
                         split->popup();
                     }
                 }
             }
             else if (arguments.at(0) == "window")
             {
                 if (auto *page = dynamic_cast<SplitContainer *>(
                         this->notebook_->getSelectedPage()))
                 {
                     page->popup();
                 }
             }
             else
             {
                 return R"(Invalid popup target. Use "split" or "window".)";
             }
             return "";
         }},
        {"zoom",
         [](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "zoom shortcut called without arguments. Takes "
                        "only "
                        "one argument: \"in\", \"out\", or \"reset\"";
                 return "zoom shortcut called without arguments. Takes "
                        "only "
                        "one argument: \"in\", \"out\", or \"reset\"";
             }
             auto change = 0.0f;
             auto direction = arguments.at(0);
             if (direction == "reset")
             {
                 getSettings()->uiScale.setValue(1);
                 return "";
             }

             if (direction == "in")
             {
                 change = 0.1f;
             }
             else if (direction == "out")
             {
                 change = -0.1f;
             }
             else
             {
                 qCWarning(chatterinoHotkeys)
                     << "Invalid zoom direction, use \"in\", \"out\", or "
                        "\"reset\"";
                 return "Invalid zoom direction, use \"in\", \"out\", or "
                        "\"reset\"";
             }
             getSettings()->setClampedUiScale(
                 getSettings()->getClampedUiScale() + change);
             return "";
         }},
        {"newTab",
         [this](std::vector<QString>) -> QString {
             this->notebook_->addPage(true);
             return "";
         }},
        {"removeTab",
         [this](std::vector<QString>) -> QString {
             this->notebook_->removeCurrentPage();
             return "";
         }},
        {"reopenSplit",
         [this](std::vector<QString>) -> QString {
             if (ClosedSplits::empty())
             {
                 return "";
             }
             ClosedSplits::SplitInfo si = ClosedSplits::pop();
             SplitContainer *splitContainer{nullptr};
             if (si.tab)
             {
                 splitContainer = dynamic_cast<SplitContainer *>(si.tab->page);
             }
             if (!splitContainer)
             {
                 splitContainer = this->notebook_->getOrAddSelectedPage();
             }
             Split *split = new Split(splitContainer);
             split->setChannel(
                 getApp()->getTwitch()->getOrAddChannel(si.channelName));
             split->setFilters(si.filters);
             splitContainer->insertSplit(split);
             splitContainer->setSelected(split);
             this->notebook_->select(splitContainer);
             return "";
         }},
        {"toggleLocalR9K",
         [](std::vector<QString>) -> QString {
             getSettings()->hideSimilar.setValue(!getSettings()->hideSimilar);
             getApp()->getWindows()->forceLayoutChannelViews();
             return "";
         }},
        {"openQuickSwitcher",
         [this](std::vector<QString>) -> QString {
             auto *quickSwitcher = new QuickSwitcherPopup(this);
             quickSwitcher->show();
             return "";
         }},
        {"quit",
         [](std::vector<QString>) -> QString {
             QApplication::exit();
             return "";
         }},
        {"moveTab",
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "moveTab shortcut called without arguments. "
                        "Takes only one argument: new index (number, "
                        "\"next\" "
                        "or \"previous\")";
                 return "moveTab shortcut called without arguments. "
                        "Takes only one argument: new index (number, "
                        "\"next\" "
                        "or \"previous\")";
             }
             int newIndex = -1;
             bool indexIsGenerated =
                 false;  // indicates if `newIndex` was generated using target="next" or target="previous"

             auto target = arguments.at(0);
             qCDebug(chatterinoHotkeys) << target;
             if (target == "next")
             {
                 newIndex = this->notebook_->getSelectedIndex() + 1;
                 indexIsGenerated = true;
             }
             else if (target == "previous")
             {
                 newIndex = this->notebook_->getSelectedIndex() - 1;
                 indexIsGenerated = true;
             }
             else
             {
                 bool ok;
                 int result = target.toInt(&ok);
                 if (!ok)
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for moveTab shortcut";
                     return QString("Invalid argument for moveTab shortcut: "
                                    "%1. Use \"next\" or \"previous\" or an "
                                    "integer.")
                         .arg(target);
                 }
                 newIndex = result;
             }
             if (newIndex >= this->notebook_->getPageCount() || 0 > newIndex)
             {
                 if (indexIsGenerated)
                 {
                     return "";  // don't error out on generated indexes, ie move tab right
                 }
                 qCWarning(chatterinoHotkeys)
                     << "Invalid index for moveTab shortcut:" << newIndex;
                 return QString("Invalid index for moveTab shortcut: %1.")
                     .arg(newIndex);
             }
             this->notebook_->rearrangePage(this->notebook_->getSelectedPage(),
                                            newIndex);
             return "";
         }},
        {"setStreamerMode",
         [](std::vector<QString> arguments) -> QString {
             auto mode = 2;
             if (arguments.size() != 0)
             {
                 auto arg = arguments.at(0);
                 if (arg == "off")
                 {
                     mode = 0;
                 }
                 else if (arg == "on")
                 {
                     mode = 1;
                 }
                 else if (arg == "toggle")
                 {
                     mode = 2;
                 }
                 else if (arg == "auto")
                 {
                     mode = 3;
                 }
                 else
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for setStreamerMode hotkey: "
                         << arg;
                     return QString("Invalid argument for setStreamerMode "
                                    "hotkey: %1. Use \"on\", \"off\", "
                                    "\"toggle\" or \"auto\".")
                         .arg(arg);
                 }
             }

             if (mode == 0)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::Disabled);
             }
             else if (mode == 1)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::Enabled);
             }
             else if (mode == 2)
             {
                 if (getApp()->getStreamerMode()->isEnabled())
                 {
                     getSettings()->enableStreamerMode.setValue(
                         StreamerModeSetting::Disabled);
                 }
                 else
                 {
                     getSettings()->enableStreamerMode.setValue(
                         StreamerModeSetting::Enabled);
                 }
             }
             else if (mode == 3)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::DetectStreamingSoftware);
             }
             return "";
         }},
        {"setTabVisibility",
         [this](std::vector<QString> arguments) -> QString {
             QString arg = arguments.empty() ? "toggle" : arguments.front();

             if (arg == "off")
             {
                 this->notebook_->hideAllTabsAction->trigger();
             }
             else if (arg == "on")
             {
                 this->notebook_->showAllTabsAction->trigger();
             }
             else if (arg == "toggle")
             {
                 this->notebook_->toggleTabVisibility();
             }
             else if (arg == "liveOnly")
             {
                 this->notebook_->onlyShowLiveTabsAction->trigger();
             }
             else if (arg == "toggleLiveOnly")
             {
                 // NOOP: Removed 2024-08-04 https://github.com/Chatterino/chatterino2/pull/5530
                 return "toggleLiveOnly is no longer a valid argument for "
                        "setTabVisibility";
             }
             else
             {
                 qCWarning(chatterinoHotkeys)
                     << "Invalid argument for setTabVisibility hotkey: " << arg;
                 return QString("Invalid argument for setTabVisibility hotkey: "
                                "%1. Use \"on\", \"off\", \"toggle\", or "
                                "\"liveOnly\".")
                     .arg(arg);
             }

             return "";
         }},
    };

    this->addDebugStuff(actions);

    this->shortcuts_ = getApp()->getHotkeys()->shortcutsForCategory(
        HotkeyCategory::Window, actions, this);
}

void Window::addMenuBar()
{
    QMenuBar *mainMenu = new QMenuBar();
    mainMenu->setNativeMenuBar(true);

    // First menu.
    QMenu *menu = mainMenu->addMenu(QString());

    // About button that shows the About tab in the Settings Dialog.
    QAction *about = menu->addAction(QString());
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, [this] {
        SettingsDialog::showDialog(this, SettingsDialogPreference::About);
    });

    QAction *prefs = menu->addAction(QString());
    prefs->setMenuRole(QAction::PreferencesRole);
    connect(prefs, &QAction::triggered, this, [this] {
        SettingsDialog::showDialog(this);
    });

    // Window menu.
    QMenu *windowMenu = mainMenu->addMenu(QString("Window"));

    // Window->Minimize item
    QAction *minimizeWindow = windowMenu->addAction(QString("Minimize"));
    minimizeWindow->setShortcuts({QKeySequence("Meta+M")});
    connect(minimizeWindow, &QAction::triggered, this, [this] {
        this->setWindowState(Qt::WindowMinimized);
    });

    QAction *nextTab = windowMenu->addAction(QString("Select next tab"));
    nextTab->setShortcuts({QKeySequence("Meta+Tab")});
    connect(nextTab, &QAction::triggered, this, [this] {
        this->notebook_->selectNextTab();
    });

    QAction *prevTab = windowMenu->addAction(QString("Select previous tab"));
    prevTab->setShortcuts({QKeySequence("Meta+Shift+Tab")});
    connect(prevTab, &QAction::triggered, this, [this] {
        this->notebook_->selectPreviousTab();
    });

    // Help menu.
    QMenu *helpMenu = mainMenu->addMenu(QString("Help"));

    // Help->Chatterino Wiki item
    QAction *helpWiki = helpMenu->addAction(QString("Chatterino Wiki"));
    connect(helpWiki, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(LINK_CHATTERINO_WIKI.toString()));
    });

    // Help->Chatterino Github
    QAction *helpGithub = helpMenu->addAction(QString("Chatterino GitHub"));
    connect(helpGithub, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(LINK_CHATTERINO_SOURCE.toString()));
    });

    // Help->Chatterino Discord
    QAction *helpDiscord = helpMenu->addAction(QString("Chatterino Discord"));
    connect(helpDiscord, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(LINK_CHATTERINO_DISCORD.toString()));
    });
}

void Window::onAccountSelected()
{
    auto user = getApp()->getAccounts()->twitch.getCurrent();

    // update title (also append username on Linux and MacOS)
    QString windowTitle = Version::instance().fullVersion();

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if (user->isAnon())
    {
        windowTitle += " - not logged in";
    }
    else
    {
        windowTitle += " - " + user->getUserName();
    }
#endif

    if (getApp()->getArgs().safeMode)
    {
        windowTitle += " (safe mode)";
    }

    this->setWindowTitle(windowTitle);

    // update user
    if (this->userLabel_)
    {
        if (user->isAnon())
        {
            this->userLabel_->setText("anonymous");
        }
        else
        {
            this->userLabel_->setText(user->getUserName());
        }
    }
}

}  // namespace chatterino
