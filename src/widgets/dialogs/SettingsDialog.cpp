// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/SettingsDialog.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "common/QLogging.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "singletons/Settings.hpp"
#include "util/German.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/BaseWindow.hpp"
#include "widgets/helper/SettingsDialogTab.hpp"
#include "widgets/settingspages/AboutPage.hpp"
#include "widgets/settingspages/AccountsPage.hpp"
#include "widgets/settingspages/ButtonsPage.hpp"
#include "widgets/settingspages/CommandPage.hpp"
#include "widgets/settingspages/ExternalToolsPage.hpp"
#include "widgets/settingspages/FiltersPage.hpp"
#include "widgets/settingspages/GeneralPage.hpp"
#include "widgets/settingspages/HighlightingPage.hpp"
#include "widgets/settingspages/IgnoresPage.hpp"
#include "widgets/settingspages/KeyboardSettingsPage.hpp"
#include "widgets/settingspages/LookPage.hpp"
#include "widgets/settingspages/ModAssistantPage.hpp"
#include "widgets/settingspages/ModerationPage.hpp"
#include "widgets/settingspages/ModHighlightsPage.hpp"
#include "widgets/settingspages/NicknamesPage.hpp"
#include "widgets/settingspages/NotificationPage.hpp"
#include "widgets/settingspages/PluginsPage.hpp"
#include "widgets/settingspages/TransferPage.hpp"

#include <QDialogButtonBox>
#include <QHash>
#include <QLabel>
#include <QFile>
#include <QLineEdit>

namespace {

/// How wide the column of page names is - German names need more room than
/// the English ones this was once set for
constexpr int TAB_COLUMN_WIDTH = 185;

/// The icon a page gets in the modern look. Chatterino's own mix a filled,
/// grey style with a drawn one; these are all drawn the same way, like the
/// ones this fork brings. Empty where there is nothing to swap.
QString modernIcon(const QString &name)
{
    static const QHash<QString, QString> icons{
        {"General", "general"},
        {"Accounts", "accounts"},
        {"Nicknames", "nicknames"},
        {"Commands", "commands"},
        {"Highlights", "highlights"},
        {"Ignores", "ignores"},
        {"Filters", "filters"},
        {"Hotkeys", "hotkeys"},
        {"Moderation", "moderation"},
        {"Live Notifications", "live"},
        {"External tools", "tools"},
        {"Plugins", "plugins"},
        {"About", "about"},
    };

    const auto found = icons.find(name);
    if (found == icons.end())
    {
        return {};
    }
    return QStringLiteral(":/settings/flexii/%1.svg").arg(found.value());
}

}  // namespace

namespace chatterino {

SettingsDialog::SettingsDialog(QWidget *parent)
    : BaseWindow(
          {
              BaseWindow::Flags::DisableCustomScaling,
              BaseWindow::Flags::Dialog,
              BaseWindow::DisableLayoutSave,
              BaseWindow::BoundsCheckOnShow,
          },
          parent)
{
    this->setObjectName("SettingsDialog");
    this->setWindowTitle("ChattiFlexii - Einstellungen");
    // Disable the ? button in the titlebar until we decide to use it
    this->setWindowFlags(this->windowFlags() &
                         ~Qt::WindowContextHelpButtonHint);

    this->resize(915, 600);
    this->themeChangedEvent();
    QFile styleFile(":/qss/settings.qss");
    if (!styleFile.open(QFile::ReadOnly))
    {
        assert(false && "Resources not loaded");
        qCWarning(chatterinoWidget) << "Resources not loaded";
    }
    QString stylesheet = QString::fromUtf8(styleFile.readAll());
    this->setStyleSheet(stylesheet);

    this->initUi();
    this->addTabs();
    this->overrideBackgroundColor_ = QColor("#111111");

    // The window is opened once and kept, so a change of look has to reach
    // the icons that are already on screen
    getSettings()->uiStyle.connect(
        [this](const auto &, auto) {
            this->refreshIcons();
        },
        this->signalHolder_, false);

    this->addShortcuts();
    this->signalHolder_.managedConnect(getApp()->getHotkeys()->onItemsUpdated,
                                       [this]() {
                                           this->clearShortcuts();
                                           this->addShortcuts();
                                       });
}

void SettingsDialog::addShortcuts()
{
    this->setSearchPlaceholderText();
    HotkeyController::HotkeyMap actions{
        {"search",
         [this](std::vector<QString>) -> QString {
             this->ui_.search->setFocus();
             this->ui_.search->selectAll();
             return "";
         }},
        {"delete", nullptr},
        {"accept", nullptr},
        {"reject", nullptr},
        {"scrollPage", nullptr},
        {"openTab", nullptr},
    };

    this->shortcuts_ = getApp()->getHotkeys()->shortcutsForCategory(
        HotkeyCategory::PopupWindow, actions, this);
}
void SettingsDialog::setSearchPlaceholderText()
{
    QString searchHotkey;
    auto searchSeq = getApp()->getHotkeys()->getDisplaySequence(
        HotkeyCategory::PopupWindow, "search");
    if (!searchSeq.isEmpty())
    {
        searchHotkey =
            "(" + searchSeq.toString(QKeySequence::SequenceFormat::NativeText) +
            ")";
    }
    this->ui_.search->setPlaceholderText("In den Einstellungen suchen ... " +
                                         searchHotkey);
}

void SettingsDialog::initUi()
{
    auto outerBox = LayoutCreator<QWidget>(this->getLayoutContainer())
                        .setLayoutType<QVBoxLayout>()
                        .withoutSpacing();

    // TOP
    auto title = outerBox.emplace<PageHeader>();
    auto edit = LayoutCreator<PageHeader>(title.getElement())
                    .setLayoutType<QHBoxLayout>()
                    .withoutMargin()
                    .emplace<QLineEdit>()
                    .assign(&this->ui_.search);
    this->setSearchPlaceholderText();

    edit->setClearButtonEnabled(true);
    edit->findChild<QAbstractButton *>()->setIcon(
        QPixmap(":/buttons/clearSearch.png"));
    this->ui_.search->installEventFilter(this);

    QObject::connect(edit.getElement(), &QLineEdit::textChanged, this,
                     &SettingsDialog::filterElements);

    // CENTER
    auto centerBox =
        outerBox.emplace<QHBoxLayout>().withoutMargin().withoutSpacing();

    // left side (tabs)
    centerBox.emplace<QWidget>()
        .assign(&this->ui_.tabContainerContainer)
        .setLayoutType<QVBoxLayout>()
        .withoutMargin()
        .assign(&this->ui_.tabContainer);
    this->ui_.tabContainerContainer->setFixedWidth(
        static_cast<int>(TAB_COLUMN_WIDTH * this->dpi_));

    // right side (pages)
    centerBox.emplace<QStackedLayout>()
        .assign(&this->ui_.pageStack)
        .withoutMargin();

    this->ui_.pageStack->setContentsMargins(0, 0, 0, 0);

    outerBox->addSpacing(12);

    // BOTTOM
    auto buttons = outerBox.emplace<QDialogButtonBox>(Qt::Horizontal);
    {
        this->ui_.okButton =
            buttons->addButton("Übernehmen", QDialogButtonBox::YesRole);
        this->ui_.cancelButton =
            buttons->addButton("Abbrechen", QDialogButtonBox::NoRole);
    }

    // ---- misc
    this->ui_.tabContainerContainer->setObjectName("tabWidget");
    this->ui_.pageStack->setObjectName("pages");

    QObject::connect(this->ui_.okButton, &QPushButton::clicked, this,
                     &SettingsDialog::onOkClicked);
    QObject::connect(this->ui_.cancelButton, &QPushButton::clicked, this,
                     &SettingsDialog::onCancelClicked);
}

void SettingsDialog::filterElements(const QString &text)
{
    // filter elements and hide pages
    for (auto &&tab : this->tabs_)
    {
        // filterElements returns true if anything on the page matches the search query
        tab->setVisible(tab->page()->filterElements(text) ||
                        tab->name().contains(text, Qt::CaseInsensitive));
    }

    // find next visible page
    if (this->lastSelectedByUser_ && this->lastSelectedByUser_->isVisible())
    {
        this->selectTab(this->lastSelectedByUser_, false);
    }
    else if (!this->selectedTab_->isVisible())
    {
        for (auto &&tab : this->tabs_)
        {
            if (tab->isVisible())
            {
                this->selectTab(tab, false);
                break;
            }
        }
    }

    this->refreshHeadings();

    // remove duplicate spaces
    bool shouldShowSpace = false;

    for (int i = 0; i < this->ui_.tabContainer->count(); i++)
    {
        auto *item = this->ui_.tabContainer->itemAt(i);
        if (auto *x = dynamic_cast<QSpacerItem *>(item); x)
        {
            x->changeSize(10, shouldShowSpace ? 16 : 0);
            shouldShowSpace = false;
        }
        else if (item->widget())
        {
            shouldShowSpace |= item->widget()->isVisible();
        }
    }
}

void SettingsDialog::setElementFilter(const QString &query)
{
    this->ui_.search->setText(query);
}

bool SettingsDialog::eventFilter(QObject *object, QEvent *event)
{
    if (object == this->ui_.search && event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
        if (keyEvent == QKeySequence::DeleteStartOfWord &&
            this->ui_.search->selectionLength() > 0)
        {
            this->ui_.search->backspace();
            return true;
        }
    }
    return false;
}

QLabel *SettingsDialog::addSectionLabel(const QString &text)
{
    auto *label = new QLabel(text.toUpper());
    label->setStyleSheet(
        QStringLiteral("color: #7a7a7a; font-size: 10px; "
                       "letter-spacing: 1px; padding: 0 0 2px 11px;"));
    this->ui_.tabContainer->addWidget(label);
    return label;
}

void SettingsDialog::addTabs()
{
    this->ui_.tabContainer->setSpacing(0);
    this->ui_.tabContainer->setContentsMargins(0, 14, 0, 20);

    // Constructors are wrapped in std::function to remove some strain from first time loading.

    // clang-format off
    // What ChattiFlexii adds comes first - that is what gets changed most
    this->ownHeading_ = this->addSectionLabel("ChattiFlexii");
    this->addTab([]{return new LookPage;},             "Aussehen",       ":/settings/look.svg");
    this->addTab([]{return new ButtonsPage;},          "Knöpfe",         ":/settings/buttons.svg");
    this->addTab([]{return new ModAssistantPage;},     "Mod-Assistent",  ":/settings/modassistant.svg");
    this->addTab([]{return new ModHighlightsPage;},    "Mod-Highlights", ":/settings/modhighlights.svg");
    this->addTab([]{return new TransferPage;},         "Sichern & Übertragen", ":/settings/transfer.svg");
    // Ours are drawn in the modern way to begin with; Chatterino's own get
    // a second set, picked when the modern look is on

    this->ui_.tabContainer->addSpacing(10);
    this->ownTabs_ = static_cast<int>(this->tabs_.size());
    this->chatterinoHeading_ = this->addSectionLabel("Chatterino");
    this->addTab([]{return new GeneralPage;},          "General",        ":/settings/about.svg", SettingsTabId::General);
    this->ui_.tabContainer->addSpacing(16);
    this->addTab([]{return new AccountsPage;},         "Accounts",       ":/settings/accounts.svg", SettingsTabId::Accounts);
    this->addTab([]{return new NicknamesPage;},        "Nicknames",      ":/settings/accounts.svg");
    this->ui_.tabContainer->addSpacing(16);
    this->addTab([]{return new CommandPage;},          "Commands",       ":/settings/commands.svg");
    this->addTab([]{return new HighlightingPage;},     "Highlights",     ":/settings/notifications.svg");
    this->addTab([]{return new IgnoresPage;},          "Ignores",        ":/settings/ignore.svg");
    this->addTab([]{return new FiltersPage;},          "Filters",        ":/settings/filters.svg");
    this->ui_.tabContainer->addSpacing(16);
    this->addTab([]{return new KeyboardSettingsPage;}, "Hotkeys",        ":/settings/keybinds.svg");
    this->addTab([]{return new ModerationPage;},       "Moderation",     ":/settings/moderation.svg", SettingsTabId::Moderation);
    this->addTab([]{return new NotificationPage;},     "Live Notifications",  ":/settings/notification2.svg");
    this->addTab([]{return new ExternalToolsPage;},    "External tools", ":/settings/externaltools.svg");
#ifdef CHATTERINO_HAVE_PLUGINS
    this->addTab([]{return new PluginsPage;},          "Plugins",        ":/settings/plugins.svg");
#endif
    this->ui_.tabContainer->addStretch(1);
    this->addTab([]{return new AboutPage;},            "About",          ":/settings/about.svg", SettingsTabId::About, Qt::AlignBottom);
    // clang-format on
}

void SettingsDialog::addTab(std::function<SettingsPage *()> page,
                            const QString &name, const QString &iconPath,
                            SettingsTabId id, Qt::Alignment alignment)
{
    // In the modern look Chatterino's own pages get an icon drawn like the
    // rest; the classic one keeps what it always had
    const auto modern = modernIcon(name);
    const auto shown =
        getSettings()->uiStyle == UiStyle::Modern && !modern.isEmpty()
            ? modern
            : iconPath;
    auto *tab = new SettingsDialogTab(this, std::move(page), german::say(name),
                                      shown, id);
    this->tabIcons_.push_back({tab, iconPath, modern});
    tab->setFixedHeight(static_cast<int>(30 * this->dpi_));

    this->ui_.tabContainer->addWidget(tab, 0, alignment);
    this->tabs_.push_back(tab);

    if (this->tabs_.size() == 1)
    {
        this->selectTab(tab);
    }
}

void SettingsDialog::refreshIcons()
{
    const bool modern = getSettings()->uiStyle == UiStyle::Modern;
    for (const auto &icons : this->tabIcons_)
    {
        icons.tab->setIcon(modern && !icons.modern.isEmpty() ? icons.modern
                                                             : icons.classic);
    }
}

void SettingsDialog::refreshHeadings()
{
    // A heading with nothing under it left would only be in the way
    const auto anyOf = [this](int from, int to) {
        for (int i = from; i < to && i < static_cast<int>(this->tabs_.size());
             i++)
        {
            if (this->tabs_[i]->isVisible())
            {
                return true;
            }
        }
        return false;
    };

    if (this->ownHeading_ != nullptr)
    {
        this->ownHeading_->setVisible(anyOf(0, this->ownTabs_));
    }
    if (this->chatterinoHeading_ != nullptr)
    {
        this->chatterinoHeading_->setVisible(
            anyOf(this->ownTabs_, static_cast<int>(this->tabs_.size())));
    }
}

void SettingsDialog::selectTab(SettingsDialogTab *tab, bool byUser)
{
    // add page if it's not been added yet
    [&] {
        for (int i = 0; i < this->ui_.pageStack->count(); i++)
        {
            if (this->ui_.pageStack->itemAt(i)->widget() == tab->page())
            {
                return;
            }
        }

        this->ui_.pageStack->addWidget(tab->page());
        // The pages that build their labels themselves say them in English
        german::translateWidgets(tab->page());
    }();

    this->ui_.pageStack->setCurrentWidget(tab->page());

    if (this->selectedTab_ != nullptr)
    {
        this->selectedTab_->setSelected(false);
        this->selectedTab_->setStyleSheet("color: #FFF");
    }

    tab->setSelected(true);
    tab->setStyleSheet(
        "background: #222; color: #4FC3F7;"  // Should this be same as accent color?
        "/*border: 1px solid #555; border-right: none;*/");
    this->selectedTab_ = tab;
    if (byUser)
    {
        this->lastSelectedByUser_ = tab;
    }
}

void SettingsDialog::selectTab(SettingsTabId id)
{
    auto *t = this->tab(id);
    assert(t);
    if (!t)
    {
        return;
    }

    this->selectTab(t);
}

SettingsDialogTab *SettingsDialog::tab(SettingsTabId id)
{
    for (auto &&tab : this->tabs_)
    {
        if (tab->id() == id)
        {
            return tab;
        }
    }

    assert(false);
    return nullptr;
}

void SettingsDialog::showDialog(QWidget *parent,
                                SettingsDialogPreference preferredTab)
{
    static SettingsDialog *instance = new SettingsDialog(parent);
    static bool hasShownBefore = false;
    if (hasShownBefore)
    {
        instance->refresh();
    }
    hasShownBefore = true;

    // Resets the cancel button.
    getSettings()->saveSnapshot();

    switch (preferredTab)
    {
        case SettingsDialogPreference::Accounts:
            instance->selectTab(SettingsTabId::Accounts);
            break;

        case SettingsDialogPreference::ModerationActions:
            if (auto *tab = instance->tab(SettingsTabId::Moderation))
            {
                instance->selectTab(tab);
                if (auto *page = dynamic_cast<ModerationPage *>(tab->page()))
                {
                    page->selectModerationActions();
                }
            }
            break;

        case SettingsDialogPreference::StreamerMode: {
            instance->selectTab(SettingsTabId::General);
        }
        break;

        case SettingsDialogPreference::About: {
            instance->selectTab(SettingsTabId::About);
        }
        break;

        default:;
    }

    instance->show();
    if (preferredTab == SettingsDialogPreference::StreamerMode)
    {
        // this is needed because each time the settings are opened, the query is reset
        instance->setElementFilter("Streamer Mode");
    }
    instance->activateWindow();
    instance->raise();
    instance->setFocus();
}

void SettingsDialog::refresh()
{
    // Updates tabs.
    for (auto *tab : this->tabs_)
    {
        tab->page()->onShow();
    }
}

void SettingsDialog::scaleChangedEvent(float newScale)
{
    assert(newScale == 1.F &&
           "Scaling is disabled for the settings dialog - its scale should "
           "always be 1");

    for (SettingsDialogTab *tab : this->tabs_)
    {
        tab->setFixedHeight(30);
    }

    if (this->ui_.tabContainerContainer)
    {
        this->ui_.tabContainerContainer->setFixedWidth(TAB_COLUMN_WIDTH);
    }
}

void SettingsDialog::themeChangedEvent()
{
    BaseWindow::themeChangedEvent();

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#111"));
    this->setPalette(palette);
}

void SettingsDialog::showEvent(QShowEvent *e)
{
    this->ui_.search->setText("");
    BaseWindow::showEvent(e);
}

///// Widget creation helpers
void SettingsDialog::onOkClicked()
{
    if (!getApp()->getArgs().dontSaveSettings)
    {
        getApp()->getCommands()->save();
    }

    getSettings()->requestSave();

    this->close();
}

void SettingsDialog::onCancelClicked()
{
    getSettings()->restoreSnapshot();

    this->close();
}

}  // namespace chatterino
