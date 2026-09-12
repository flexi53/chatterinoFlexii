// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/NotebookTab.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/Common.hpp"
#include "common/QLogging.hpp"
#include "controllers/hotkeys/HotkeyCategory.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "util/Helpers.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/DraggedSplit.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"

#include <boost/bind/bind.hpp>
#include <boost/container_hash/hash.hpp>
#include <QAbstractAnimation>
#include <QApplication>
#include <QColorDialog>
#include <QDebug>
#include <QDialogButtonBox>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <algorithm>

namespace chatterino {
namespace {
// Translates the given rectangle by an amount in the direction to appear like the tab is selected.
// For example, if location is Top, the rectangle will be translated in the negative Y direction,
// or "up" on the screen, by amount.
void translateRectForLocation(QRect &rect, NotebookTabLocation location,
                              int amount)
{
    switch (location)
    {
        case NotebookTabLocation::Top:
            rect.translate(0, -amount);
            break;
        case NotebookTabLocation::Left:
            rect.translate(-amount, 0);
            break;
        case NotebookTabLocation::Right:
            rect.translate(amount, 0);
            break;
        case NotebookTabLocation::Bottom:
            rect.translate(0, amount);
            break;
    }
}

float getCompactDivider(TabStyle tabStyle)
{
    switch (tabStyle)
    {
        case TabStyle::Compact:
            return 1.5;
        case TabStyle::Normal:
        default:
            return 1.0;
    }
}

float getCompactReducer(TabStyle tabStyle)
{
    switch (tabStyle)
    {
        case TabStyle::Compact:
            return 4.0;
        case TabStyle::Normal:
        default:
            return 0.0;
    }
}
}  // namespace

NotebookTab::NotebookTab(Notebook *notebook)
    : Button(notebook)
    , positionChangedAnimation_(this, "pos")
    , notebook_(notebook)
    , menu_(this)
{
    this->setContentCacheEnabled(false);
    this->setAcceptDrops(true);

    this->positionChangedAnimation_.setEasingCurve(
        QEasingCurve(QEasingCurve::InCubic));

    getSettings()->showTabCloseButton.connect(
        [this] {
            this->tabSizeChanged();
        },
        this->managedConnections_);
    getSettings()->tabStyle.connect(
        [this] {
            this->tabSizeChanged();
        },
        this->managedConnections_);
    getSettings()->showTabLive.connect(
        [this](auto, auto) {
            this->update();
        },
        this->managedConnections_);
    for (auto *setting : {&getSettings()->tabBackgroundColor,
                          &getSettings()->tabSelectedBackgroundColor,
                          &getSettings()->tabGradientTopColor,
                          &getSettings()->tabGradientBottomColor})
    {
        setting->connect(
            [this](const auto &, const auto &) {
                this->update();
            },
            this->managedConnections_, false);
    }
    getSettings()->tabGradient.connect(
        [this](const auto &, const auto &) {
            this->update();
        },
        this->managedConnections_, false);

    this->setMouseTracking(true);

    this->menu_.addAction("Rename Tab", [this]() {
        this->showRenameDialog();
    });

    this->tabGroupMenu_ = new QMenu("Tab Group", &this->menu_);
    this->menu_.addMenu(this->tabGroupMenu_);

    this->tabColorMenu_ = new QMenu("Tab Color", &this->menu_);
    this->menu_.addMenu(this->tabColorMenu_);

    // The list of groups changes while the app runs, so the submenu is rebuilt
    // every time the context menu is opened.
    QObject::connect(&this->menu_, &QMenu::aboutToShow, this, [this] {
        this->rebuildTabGroupMenu();
        this->rebuildTabColorMenu();
    });

    // XXX: this doesn't update after changing hotkeys

    this->menu_.addAction("Close Tab",
                          getApp()->getHotkeys()->getDisplaySequence(
                              HotkeyCategory::Window, "removeTab"),
                          [this]() {
                              this->notebook_->removePage(this->page);
                          });

    this->closeMultipleTabsMenu_ = new QMenu("Close Multiple Tabs", this);

    this->menu_.addMenu(this->closeMultipleTabsMenu_);
    getSettings()->tabDirection.connect(
        [this](int val) {
            this->recreateCloseMultipleTabsMenu(
                static_cast<NotebookTabLocation>(val));
        },
        this->signalHolder_);

    this->menu_.addAction(
        "Popup Tab",
        getApp()->getHotkeys()->getDisplaySequence(HotkeyCategory::Window,
                                                   "popup", {{"window"}}),
        [this]() {
            if (auto *container = dynamic_cast<SplitContainer *>(this->page))
            {
                container->popup();
            }
        });

    this->menu_.addAction("Duplicate Tab", [this]() {
        this->notebook_->duplicatePage(this->page);
    });

    this->highlightNewMessagesAction_ =
        new QAction("Mark Tab as Unread on New Messages", &this->menu_);
    this->highlightNewMessagesAction_->setCheckable(true);
    this->highlightNewMessagesAction_->setChecked(this->highlightEnabled_);
    QObject::connect(this->highlightNewMessagesAction_, &QAction::triggered,
                     [this](bool checked) {
                         this->highlightEnabled_ = checked;
                     });
    this->menu_.addAction(this->highlightNewMessagesAction_);

    this->menu_.addSeparator();

    this->notebook_->addNotebookActionsToMenu(&this->menu_);
}

void NotebookTab::recreateCloseMultipleTabsMenu(
    const NotebookTabLocation tabLocation)
{
    this->closeMultipleTabsMenu_->clear();

    this->closeMultipleTabsMenu_->addAction("Close All Visible Tabs", [this]() {
        auto reply = QMessageBox::question(
            this, "Close All Visible Tabs",
            "Are you sure you want to close all visible tabs?",
            QMessageBox::Yes | QMessageBox::Cancel);

        if (reply != QMessageBox::Yes)
        {
            return;
        }

        for (int i = this->notebook_->getPageCount() - 1; i >= 0; --i)
        {
            auto *page = this->notebook_->getPageAt(i);

            auto *container = dynamic_cast<SplitContainer *>(page);
            if (!container)
            {
                continue;
            }

            auto *tab = container->getTab();
            if (!tab || !tab->isVisible())
            {
                continue;
            }

            this->notebook_->removePage(page);
        }
    });

    QString beforeSelectedName;
    QString afterSelectedName;
    switch (tabLocation)
    {
        case Top:
        case Bottom:
            beforeSelectedName = "Left";
            afterSelectedName = "Right";
            break;
        case Left:
        case Right:
            beforeSelectedName = "Top";
            afterSelectedName = "Bottom";
            break;
    }

    this->closeTabsBeforeSelectedAction_ =
        this->closeMultipleTabsMenu_->addAction(
            "Close Visible Tabs to " + beforeSelectedName,
            [this, beforeSelectedName]() {
                auto reply = QMessageBox::question(
                    this, "Close Visible Tabs to " + beforeSelectedName,
                    "Are you sure you want to close all visible tabs to the " +
                        beforeSelectedName.toLower() + "?",
                    QMessageBox::Yes | QMessageBox::Cancel);

                if (reply != QMessageBox::Yes)
                {
                    return;
                }

                std::vector<QWidget *> pagesToRemove;
                for (int i = 0; i < this->notebook_->getPageCount(); ++i)
                {
                    auto *page = this->notebook_->getPageAt(i);
                    if (page == this->page)
                    {
                        break;
                    }

                    auto *container = dynamic_cast<SplitContainer *>(page);
                    if (!container)
                    {
                        continue;
                    }

                    auto *tab = container->getTab();
                    if (!tab || !tab->isVisible())
                    {
                        continue;
                    }

                    pagesToRemove.push_back(page);
                }

                for (auto *page : pagesToRemove)
                {
                    this->notebook_->removePage(page);
                }
            });

    this->closeTabsAfterSelectedAction_ =
        this->closeMultipleTabsMenu_->addAction(
            "Close Visible Tabs to " + afterSelectedName,
            [this, afterSelectedName]() {
                auto reply = QMessageBox::question(
                    this, "Close Visible Tabs to " + afterSelectedName,
                    "Are you sure you want to close all visible tabs to the " +
                        afterSelectedName + "?",
                    QMessageBox::Yes | QMessageBox::Cancel);

                if (reply != QMessageBox::Yes)
                {
                    return;
                }

                for (int i = this->notebook_->getPageCount() - 1; i >= 0; --i)
                {
                    auto *p = this->notebook_->getPageAt(i);
                    if (p == this->page)
                    {
                        break;
                    }

                    auto *container = dynamic_cast<SplitContainer *>(p);
                    if (!container)
                    {
                        continue;
                    }

                    auto *tab = container->getTab();
                    if (!tab || !tab->isVisible())
                    {
                        continue;
                    }

                    this->notebook_->removePage(p);
                }
            });

    this->closeMultipleTabsMenu_->addAction(
        "Close Other Visible Tabs", [this]() {
            auto reply = QMessageBox::question(
                this, "Close Other Visible Tabs",
                "Are you sure you want to close all other visible tabs?",
                QMessageBox::Yes | QMessageBox::Cancel);

            if (reply != QMessageBox::Yes)
            {
                return;
            }

            for (int i = this->notebook_->getPageCount() - 1; i >= 0; --i)
            {
                auto *p = this->notebook_->getPageAt(i);
                if (p == this->page)
                {
                    continue;
                }

                auto *container = dynamic_cast<SplitContainer *>(p);
                if (!container)
                {
                    continue;
                }

                auto *tab = container->getTab();
                if (!tab || !tab->isVisible())
                {
                    continue;
                }

                this->notebook_->removePage(p);
            }
        });
}

void NotebookTab::showRenameDialog()
{
    auto *dialog = new QDialog(this);

    auto *vbox = new QVBoxLayout;

    auto *lineEdit = new QLineEdit;
    lineEdit->setText(this->getCustomTitle());
    lineEdit->setPlaceholderText(this->getDefaultTitle());
    lineEdit->selectAll();

    vbox->addWidget(new QLabel("Name:"));
    vbox->addWidget(lineEdit);
    vbox->addStretch(1);

    auto *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    vbox->addWidget(buttonBox);
    dialog->setLayout(vbox);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, [dialog] {
        dialog->accept();
        dialog->close();
    });

    QObject::connect(buttonBox, &QDialogButtonBox::rejected, [dialog] {
        dialog->reject();
        dialog->close();
    });

    dialog->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    dialog->setMinimumSize(dialog->minimumSizeHint().width() + 50,
                           dialog->minimumSizeHint().height() + 10);

    dialog->setWindowFlags(
        (dialog->windowFlags() & ~(Qt::WindowContextHelpButtonHint)) |
        Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    dialog->setWindowTitle("Rename Tab");

    if (dialog->exec() == QDialog::Accepted)
    {
        QString newTitle = lineEdit->text();
        this->setCustomTitle(newTitle);
    }
}

void NotebookTab::themeChangedEvent()
{
    this->update();

    //    this->setMouseEffectColor(QColor("#999"));
    this->setMouseEffectColor(this->theme->tabs.regular.text);
}

void NotebookTab::growWidth(int width)
{
    if (this->growWidth_ != width)
    {
        this->growWidth_ = width;
        this->updateSize();
    }
    else
    {
        this->growWidth_ = width;
    }
}

int NotebookTab::normalTabWidth() const
{
    return this->normalTabWidthForHeight(this->height());
}

int NotebookTab::normalTabWidthForHeight(int height) const
{
    float scale = this->scale();
    int width = 0;

    // Bold text is wider, so the width has to be measured with the very font
    // the title will be drawn with - otherwise it gets clipped.
    QFontMetricsF metrics(this->titleFont());

    float compactDivider = getCompactDivider(getSettings()->tabStyle);
    if (this->hasXButton())
    {
        width = static_cast<int>(metrics.horizontalAdvance(this->getTitle()) +
                                 (32 / compactDivider * scale));
    }
    else
    {
        width = static_cast<int>(metrics.horizontalAdvance(this->getTitle()) +
                                 (16 / compactDivider * scale));
    }

    if (static_cast<float>(height) > 150 * scale)
    {
        width = height;
    }
    else
    {
        width = std::clamp(width, height, static_cast<int>(150 * scale));
    }

    return width;
}

void NotebookTab::updateSize()
{
    float scale = this->scale();
    auto height = static_cast<int>(NOTEBOOK_TAB_HEIGHT * scale);
    int width = this->normalTabWidthForHeight(height);

    if (width < this->growWidth_)
    {
        width = this->growWidth_;
    }

    if (this->width() != width || this->height() != height)
    {
        this->resize(width, height);
        this->notebook_->refresh();
    }
}

const QString &NotebookTab::getCustomTitle() const
{
    return this->customTitle_;
}

void NotebookTab::setCustomTitle(const QString &newTitle)
{
    if (this->customTitle_ != newTitle)
    {
        this->customTitle_ = newTitle;
        this->titleUpdated();
    }
}

void NotebookTab::resetCustomTitle()
{
    this->setCustomTitle(QString());
}

bool NotebookTab::hasCustomTitle() const
{
    return !this->customTitle_.isEmpty();
}

void NotebookTab::setDefaultTitle(const QString &title)
{
    if (this->defaultTitle_ != title)
    {
        this->defaultTitle_ = title;

        if (this->customTitle_.isEmpty())
        {
            this->titleUpdated();
        }
    }
}

const QString &NotebookTab::getDefaultTitle() const
{
    return this->defaultTitle_;
}

const QString &NotebookTab::getTitle() const
{
    return this->customTitle_.isEmpty() ? this->defaultTitle_
                                        : this->customTitle_;
}

void NotebookTab::titleUpdated()
{
    // Queue up save because: Tab title changed
    getApp()->getWindows()->queueSave();
    this->notebook_->refresh();
    this->updateSize();
    this->update();
}

bool NotebookTab::isSelected() const
{
    return this->selected_;
}

void NotebookTab::removeHighlightStateChangeSources(
    const HighlightSources &toRemove)
{
    for (const auto &[source, _] : toRemove)
    {
        this->removeHighlightSource(source);
    }
}

void NotebookTab::removeHighlightSource(
    const ChannelView::ChannelViewID &source)
{
    this->highlightSources_.erase(source);
}

void NotebookTab::newHighlightSourceAdded(const ChannelView &channelViewSource)
{
    auto channelViewId = channelViewSource.getID();
    this->removeHighlightSource(channelViewId);
    this->updateHighlightStateDueSourcesChange();

    auto *splitNotebook = dynamic_cast<SplitNotebook *>(this->notebook_);
    if (splitNotebook)
    {
        for (int i = 0; i < splitNotebook->getPageCount(); ++i)
        {
            auto *splitContainer =
                dynamic_cast<SplitContainer *>(splitNotebook->getPageAt(i));
            if (splitContainer)
            {
                auto *tab = splitContainer->getTab();
                if (tab && tab != this)
                {
                    tab->removeHighlightSource(channelViewId);
                    tab->updateHighlightStateDueSourcesChange();
                }
            }
        }
    }
}

void NotebookTab::updateHighlightStateDueSourcesChange()
{
    if (std::ranges::any_of(this->highlightSources_, [](const auto &keyval) {
            return keyval.second == HighlightState::Highlighted;
        }))
    {
        assert(this->highlightState_ == HighlightState::Highlighted);
        return;
    }

    if (std::ranges::any_of(this->highlightSources_, [](const auto &keyval) {
            return keyval.second == HighlightState::NewMessage;
        }))
    {
        if (this->highlightState_ != HighlightState::NewMessage)
        {
            this->highlightState_ = HighlightState::NewMessage;
            this->update();
        }
    }
    else
    {
        if (this->highlightState_ != HighlightState::None)
        {
            this->highlightState_ = HighlightState::None;
            this->update();
        }
    }

    assert(this->highlightState_ != HighlightState::Highlighted);
}

void NotebookTab::copyHighlightStateAndSourcesFrom(const NotebookTab *sourceTab)
{
    if (this->isSelected())
    {
        assert(this->highlightSources_.empty());
        assert(this->highlightState_ == HighlightState::None);
        return;
    }

    this->highlightSources_ = sourceTab->highlightSources_;

    if (!this->highlightEnabled_ &&
        sourceTab->highlightState_ == HighlightState::NewMessage)
    {
        return;
    }

    if (this->highlightState_ == sourceTab->highlightState_ ||
        this->highlightState_ == HighlightState::Highlighted)
    {
        return;
    }

    this->highlightState_ = sourceTab->highlightState_;
    this->update();
}

void NotebookTab::setSelected(bool value)
{
    this->selected_ = value;

    if (value)
    {
        auto *splitNotebook = dynamic_cast<SplitNotebook *>(this->notebook_);
        if (splitNotebook)
        {
            for (int i = 0; i < splitNotebook->getPageCount(); ++i)
            {
                auto *splitContainer =
                    dynamic_cast<SplitContainer *>(splitNotebook->getPageAt(i));
                if (splitContainer)
                {
                    auto *tab = splitContainer->getTab();
                    if (tab && tab != this)
                    {
                        tab->removeHighlightStateChangeSources(
                            this->highlightSources_);
                        tab->updateHighlightStateDueSourcesChange();
                    }
                }
            }
        }
    }

    this->highlightSources_.clear();
    this->highlightState_ = HighlightState::None;

    this->update();
}

void NotebookTab::setInLastRow(bool value)
{
    if (this->isInLastRow_ != value)
    {
        this->isInLastRow_ = value;
        this->update();
    }
}

void NotebookTab::setTabLocation(NotebookTabLocation location)
{
    if (this->tabLocation_ != location)
    {
        this->tabLocation_ = location;
        this->update();
    }
}

bool NotebookTab::setRerun(bool isRerun)
{
    if (this->isRerun_ != isRerun)
    {
        this->isRerun_ = isRerun;
        this->update();
        return true;
    }

    return false;
}

bool NotebookTab::setLive(bool isLive)
{
    if (this->isLive_ != isLive)
    {
        this->isLive_ = isLive;
        this->update();
        return true;
    }

    return false;
}

bool NotebookTab::isLive() const
{
    return this->isLive_;
}

HighlightState NotebookTab::highlightState() const
{
    return this->highlightState_;
}

void NotebookTab::setHighlightState(HighlightState newHighlightStyle)
{
    if (this->isSelected())
    {
        assert(this->highlightSources_.empty());
        assert(this->highlightState_ == HighlightState::None);
        return;
    }

    this->highlightSources_.clear();

    if (!this->highlightEnabled_ &&
        newHighlightStyle == HighlightState::NewMessage)
    {
        return;
    }

    if (this->highlightState_ == newHighlightStyle ||
        this->highlightState_ == HighlightState::Highlighted)
    {
        return;
    }

    this->highlightState_ = newHighlightStyle;
    this->update();
}

void NotebookTab::updateHighlightState(HighlightState newHighlightStyle,
                                       const ChannelView &channelViewSource)
{
    if (this->isSelected())
    {
        assert(this->highlightSources_.empty());
        assert(this->highlightState_ == HighlightState::None);
        return;
    }

    if (!this->shouldMessageHighlight(channelViewSource))
    {
        return;
    }

    if (!this->highlightEnabled_ &&
        newHighlightStyle == HighlightState::NewMessage)
    {
        return;
    }

    // message is highlighting unvisible tab

    auto channelViewId = channelViewSource.getID();

    switch (newHighlightStyle)
    {
        case HighlightState::Highlighted:
            // override lower states
            this->highlightSources_.insert_or_assign(channelViewId,
                                                     newHighlightStyle);
        case HighlightState::NewMessage: {
            // only insert if no state already there to avoid overriding
            if (!this->highlightSources_.contains(channelViewId))
            {
                this->highlightSources_.emplace(channelViewId,
                                                newHighlightStyle);
            }
            break;
        }
        case HighlightState::None:
            break;
    }

    if (this->highlightState_ == newHighlightStyle ||
        this->highlightState_ == HighlightState::Highlighted)
    {
        return;
    }

    this->highlightState_ = newHighlightStyle;
    this->update();
}

bool NotebookTab::shouldMessageHighlight(
    const ChannelView &channelViewSource) const
{
    auto *visibleSplitContainer =
        dynamic_cast<SplitContainer *>(this->notebook_->getSelectedPage());
    if (visibleSplitContainer != nullptr)
    {
        const auto &visibleSplits = visibleSplitContainer->getSplits();
        for (const auto &visibleSplit : visibleSplits)
        {
            if (channelViewSource.getID() ==
                visibleSplit->getChannelView().getID())
            {
                return false;
            }
        }
    }

    return true;
}

void NotebookTab::setHighlightsEnabled(const bool &newVal)
{
    this->highlightNewMessagesAction_->setChecked(newVal);
    this->highlightEnabled_ = newVal;
}

bool NotebookTab::hasHighlightsEnabled() const
{
    return this->highlightEnabled_;
}

QRect NotebookTab::getDesiredRect() const
{
    return QRect(this->positionAnimationDesiredPoint_, this->size());
}

void NotebookTab::tabSizeChanged()
{
    this->updateSize();
    this->update();
}

void NotebookTab::moveAnimated(QPoint targetPos, bool animated)
{
    this->positionAnimationDesiredPoint_ = targetPos;

    if (!animated || !this->notebook_->isVisible())
    {
        this->move(targetPos);
        return;
    }

    if (this->positionChangedAnimation_.state() ==
            QAbstractAnimation::Running &&
        this->positionChangedAnimation_.endValue() == targetPos)
    {
        return;
    }

    this->positionChangedAnimation_.stop();
    this->positionChangedAnimation_.setDuration(75);
    this->positionChangedAnimation_.setStartValue(this->pos());
    this->positionChangedAnimation_.setEndValue(targetPos);
    this->positionChangedAnimation_.start();
}

void NotebookTab::paintEvent(QPaintEvent *)
{
    auto *app = getApp();
    QPainter painter(this);
    float scale = this->scale();

    const auto font = this->titleFont();
    painter.setFont(font);
    QFontMetricsF metrics(font);

    int height = int(scale * NOTEBOOK_TAB_HEIGHT);

    // select the right tab colors
    Theme::TabColors colors;

    if (this->selected_)
    {
        colors = this->theme->tabs.selected;
    }
    else if (this->highlightState_ == HighlightState::Highlighted)
    {
        colors = this->theme->tabs.highlighted;
    }
    else if (this->highlightState_ == HighlightState::NewMessage)
    {
        colors = this->theme->tabs.newMessage;
    }
    else
    {
        colors = this->theme->tabs.regular;
    }

    bool windowFocused = this->window() == QApplication::activeWindow();

    QBrush tabBackground = /*this->mouseOver_ ? colors.backgrounds.hover
                                 :*/
        (windowFocused ? colors.backgrounds.regular
                       : colors.backgrounds.unfocused);

    // A tab that wants attention - a highlight, new messages - keeps the
    // theme's colour whatever was picked, so it still stands out.
    const auto *lookSettings = getSettings();
    const bool wantsAttention =
        !this->selected_ &&
        (this->highlightState_ == HighlightState::Highlighted ||
         this->highlightState_ == HighlightState::NewMessage);

    if (!wantsAttention)
    {
        const QColor picked(
            this->selected_
                ? lookSettings->tabSelectedBackgroundColor.getValue()
                : lookSettings->tabBackgroundColor.getValue());
        if (picked.isValid())
        {
            tabBackground = picked;
        }
    }

    auto selectionOffset = ceil((this->selected_ ? 0.f : 1.f) * scale);

    // fill the tab background
    auto bgRect = this->rect();
    switch (this->tabLocation_)
    {
        case NotebookTabLocation::Top:
            bgRect.setTop(selectionOffset);
            break;
        case NotebookTabLocation::Left:
            bgRect.setLeft(selectionOffset);
            break;
        case NotebookTabLocation::Right:
            bgRect.setRight(bgRect.width() - selectionOffset);
            break;
        case NotebookTabLocation::Bottom:
            bgRect.setBottom(bgRect.height() - selectionOffset);
            break;
    }

    // The rounded outline of this tab. The background, the marker colour and
    // the indicator line are all clipped to it, so nothing bleeds past the
    // rounded corners.
    const auto tabShape = this->tabShapePath(bgRect, scale);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setClipPath(tabShape);

    painter.fillRect(bgRect, tabBackground);

    // The gradient is a switch of its own, not part of a look. It leaves the
    // selected tab flat so the open one is still obvious, and group headers
    // keep their classic fill.
    if (lookSettings->tabGradient && !this->selected_ && !wantsAttention &&
        !this->hasFullyRoundedCorners())
    {
        const QColor top(lookSettings->tabGradientTopColor.getValue());
        const QColor bottom(lookSettings->tabGradientBottomColor.getValue());
        if (top.isValid() && bottom.isValid())
        {
            QLinearGradient gradient(bgRect.topLeft(), bgRect.bottomLeft());
            gradient.setColorAt(0.0, top);
            gradient.setColorAt(1.0, bottom);
            painter.fillRect(bgRect, gradient);
        }
    }

    // Modern gives the tab some depth: light from above, a hairline rim.
    // Classic keeps the flat fill Chatterino has always had.
    const bool modernLook = getSettings()->uiStyle == UiStyle::Modern &&
                            !this->hasFullyRoundedCorners();
    if (modernLook)
    {
        const bool lightTheme = this->theme->isLightTheme();

        QLinearGradient body(bgRect.topLeft(), bgRect.bottomLeft());
        if (lightTheme)
        {
            body.setColorAt(0.0, QColor(255, 255, 255, 170));
            body.setColorAt(0.45, QColor(255, 255, 255, 50));
            body.setColorAt(1.0, QColor(0, 0, 0, 14));
        }
        else
        {
            body.setColorAt(0.0, QColor(255, 255, 255, 46));
            body.setColorAt(0.45, QColor(255, 255, 255, 12));
            body.setColorAt(1.0, QColor(0, 0, 0, 30));
        }
        painter.fillRect(bgRect, body);
    }

    // wash the background with the tab's marker colour
    if (this->customColor_.isValid())
    {
        auto wash = this->customColor_;
        wash.setAlpha(this->selected_ ? 60 : 38);
        painter.fillRect(bgRect, wash);
    }

    // draw color indicator line
    auto lineThickness = ceil((this->selected_ ? 2.f : 1.f) * scale);
    auto lineColor = this->mouseOver_ ? colors.line.hover
                                      : (windowFocused ? colors.line.regular
                                                       : colors.line.unfocused);

    // A marked tab shows its own colour on the indicator line. Selection is
    // still readable because a selected tab draws the line twice as thick.
    if (this->customColor_.isValid())
    {
        lineColor = this->customColor_;
    }

    QRect lineRect;
    switch (this->tabLocation_)
    {
        case NotebookTabLocation::Top:
            lineRect =
                QRect(bgRect.left(), bgRect.y(), bgRect.width(), lineThickness);
            break;
        case NotebookTabLocation::Left:
            lineRect =
                QRect(bgRect.x(), bgRect.top(), lineThickness, bgRect.height());
            break;
        case NotebookTabLocation::Right:
            lineRect = QRect(bgRect.right() - lineThickness, bgRect.top(),
                             lineThickness, bgRect.height());
            break;
        case NotebookTabLocation::Bottom:
            lineRect = QRect(bgRect.left(), bgRect.bottom() - lineThickness,
                             bgRect.width(), lineThickness);
            break;
    }

    painter.fillRect(lineRect, lineColor);

    painter.restore();

    if (modernLook)
    {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        const auto inset = qreal(0.5) * scale;
        const auto rimRect =
            QRectF(bgRect).adjusted(inset, inset, -inset, -inset);

        QLinearGradient rim(rimRect.topLeft(), rimRect.bottomLeft());
        const bool lightTheme = this->theme->isLightTheme();
        rim.setColorAt(0.0, QColor(255, 255, 255, lightTheme ? 235 : 140));
        rim.setColorAt(0.5, QColor(255, 255, 255, lightTheme ? 110 : 55));
        rim.setColorAt(1.0, QColor(255, 255, 255, lightTheme ? 40 : 18));

        painter.setPen(QPen(QBrush(rim), qreal(1.0) * scale));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(this->tabShapePath(rimRect, scale));

        painter.restore();
    }

    // draw live indicator
    if ((this->isLive_ || this->isRerun_) && getSettings()->showTabLive)
    {
        // Live overrides rerun
        QBrush b;
        if (this->isLive_)
        {
            painter.setPen(this->theme->tabs.liveIndicator);
            b.setColor(this->theme->tabs.liveIndicator);
        }
        else
        {
            painter.setPen(this->theme->tabs.rerunIndicator);
            b.setColor(this->theme->tabs.rerunIndicator);
        }

        painter.setRenderHint(QPainter::Antialiasing);
        b.setStyle(Qt::SolidPattern);
        painter.setBrush(b);

        auto x = this->width() - (7 * scale);
        auto y = 4 * scale;
        auto diameter = 4 * scale;
        QRect liveIndicatorRect(x, y, diameter, diameter);
        translateRectForLocation(liveIndicatorRect, this->tabLocation_,
                                 this->selected_ ? 0 : -1);
        painter.drawEllipse(liveIndicatorRect);
    }

    // set the pen color
    painter.setPen(colors.text);

    float compactDivider = getCompactDivider(getSettings()->tabStyle);
    // set area for text
    int rectW =
        (!getSettings()->showTabCloseButton ? 0
                                            : int(16 * scale / compactDivider));
    QRect rect(0, 0, this->width() - rectW, height);

    // draw text
    int offset = int(scale * 4 / compactDivider);
    QRect textRect(offset, 0, this->width() - offset - offset, height);
    translateRectForLocation(textRect, this->tabLocation_,
                             this->selected_ ? -1 : -2);

    if (this->shouldDrawXButton())
    {
        textRect.setRight(textRect.right() - this->height() / 2);
    }

    int width = metrics.horizontalAdvance(this->getTitle());
    Qt::Alignment alignment = width > textRect.width()
                                  ? Qt::AlignLeft | Qt::AlignVCenter
                                  : Qt::AlignHCenter | Qt::AlignVCenter;

    QTextOption option(alignment);
    option.setWrapMode(QTextOption::NoWrap);
    painter.drawText(textRect, this->getTitle(), option);

    // draw close x
    if (this->shouldDrawXButton())
    {
        painter.setRenderHint(QPainter::Antialiasing, false);

        QRect xRect = this->getXRect();
        if (!xRect.isNull())
        {
            painter.setBrush(QColor("#fff"));

            if (this->mouseOverX_)
            {
                painter.fillRect(xRect, QColor(0, 0, 0, 64));

                if (this->mouseDownX_)
                {
                    painter.fillRect(xRect, QColor(0, 0, 0, 64));
                }
            }

            int a = static_cast<int>(scale * 4);

            painter.drawLine(xRect.topLeft() + QPoint(a, a),
                             xRect.bottomRight() + QPoint(-a, -a));
            painter.drawLine(xRect.topRight() + QPoint(-a, a),
                             xRect.bottomLeft() + QPoint(a, -a));
        }
    }

    // draw mouse over effect
    if (!this->selected_)
    {
        painter.save();
        painter.setClipPath(tabShape);
        this->fancyPaint(painter);
        painter.restore();
    }

    // draw line at border
    if (!this->selected_ && this->isInLastRow_)
    {
        QRect borderRect;
        switch (this->tabLocation_)
        {
            case NotebookTabLocation::Top:
                borderRect = QRect(0, this->height() - 1, this->width(), 1);
                break;
            case NotebookTabLocation::Left:
                borderRect = QRect(this->width() - 1, 0, 1, this->height());
                break;
            case NotebookTabLocation::Right:
                borderRect = QRect(0, 0, 1, this->height());
                break;
            case NotebookTabLocation::Bottom:
                borderRect = QRect(0, 0, this->width(), 1);
                break;
        }
        painter.fillRect(borderRect, app->getThemes()->window.background);
    }
}

bool NotebookTab::hasXButton() const
{
    return getSettings()->showTabCloseButton &&
           this->notebook_->getAllowUserTabManagement();
}

bool NotebookTab::shouldDrawXButton() const
{
    return this->hasXButton() && (this->mouseOver_ || this->selected_);
}

void NotebookTab::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        this->mouseDown_ = true;
        this->mouseDownX_ = this->getXRect().contains(event->pos());

        this->notebook_->select(this->page);
    }

    this->update();

    if (this->notebook_->getAllowUserTabManagement())
    {
        switch (event->button())
        {
            case Qt::RightButton: {
                this->menu_.popup(event->globalPosition().toPoint() +
                                  QPoint(0, 8));

                const int visibleTabCount =
                    this->notebook_->getVisibleTabCount();
                const int selectedTabIndex =
                    this->notebook_->visibleIndexOf(this->page);

                this->closeMultipleTabsMenu_->setEnabled(visibleTabCount > 1);

                this->closeTabsBeforeSelectedAction_->setEnabled(
                    selectedTabIndex > 0);
                this->closeTabsAfterSelectedAction_->setEnabled(
                    selectedTabIndex != -1 &&
                    selectedTabIndex < (visibleTabCount - 1));
            }
            break;
            default:;
        }
    }
}

void NotebookTab::mouseReleaseEvent(QMouseEvent *event)
{
    this->mouseDown_ = false;

    auto removeThisPage = [this] {
        auto reply = QMessageBox::question(
            this, "Remove this tab",
            "Are you sure that you want to remove this tab?",
            QMessageBox::Yes | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes)
        {
            this->notebook_->removePage(this->page);
        }
    };

    if (event->button() == Qt::MiddleButton &&
        this->notebook_->getAllowUserTabManagement())
    {
        if (this->rect().contains(event->pos()))
        {
            removeThisPage();
        }
    }
    else
    {
        if (this->hasXButton() && this->mouseDownX_ &&
            this->getXRect().contains(event->pos()))
        {
            this->mouseDownX_ = false;

            removeThisPage();
        }
        else
        {
            this->update();
        }
    }
}

void NotebookTab::mouseDoubleClickEvent(QMouseEvent *event)
{
    const auto canRenameTab = this->notebook_->getAllowUserTabManagement() &&
                              getSettings()->disableTabRenamingOnClick == false;

    if (event->button() == Qt::LeftButton && canRenameTab)
    {
        this->showRenameDialog();
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void NotebookTab::enterEvent(QEnterEvent *event)
#else
void NotebookTab::enterEvent(QEvent *event)
#endif
{
    this->mouseOver_ = true;

    this->update();

    Button::enterEvent(event);
}

void NotebookTab::leaveEvent(QEvent *event)
{
    this->mouseOverX_ = false;
    this->mouseOver_ = false;

    this->update();

    Button::leaveEvent(event);
}

void NotebookTab::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasFormat("chatterino/split"))
    {
        return;
    }

    if (!isDraggingSplit())
    {
        // Ensure dragging a split from a different Chatterino instance doesn't switch tabs around
        return;
    }

    event->acceptProposedAction();

    if (this->notebook_->getAllowUserTabManagement())
    {
        this->notebook_->select(this->page);
    }
}

void NotebookTab::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasFormat("chatterino/split"))
    {
        return;
    }

    if (!isDraggingSplit())
    {
        // Ensure dragging a split from a different Chatterino instance doesn't switch tabs around
        return;
    }

    auto *draggedSplit = dynamic_cast<Split *>(event->source());
    if (!draggedSplit)
    {
        qCDebug(chatterinoWidget)
            << "Dropped something that wasn't a split onto a notebook button";
        return;
    }

    if (auto *container = dynamic_cast<SplitContainer *>(this->page))
    {
        event->acceptProposedAction();
        container->insertSplit(draggedSplit);
    }
}

void NotebookTab::mouseMoveEvent(QMouseEvent *event)
{
    if (getSettings()->showTabCloseButton &&
        this->notebook_->getAllowUserTabManagement())
    {
        bool overX = this->getXRect().contains(event->pos());

        if (overX != this->mouseOverX_)
        {
            // Over X state has been changed (we either left or entered it;
            this->mouseOverX_ = overX;

            this->update();
        }
    }

    QPoint relPoint = this->mapToParent(event->pos());

    if (this->mouseDown_ && !this->getDesiredRect().contains(relPoint) &&
        this->notebook_->getAllowUserTabManagement())
    {
        int index;
        QWidget *clickedPage =
            this->notebook_->tabAt(relPoint, index, this->width());

        if (clickedPage != nullptr && clickedPage != this->page)
        {
            this->notebook_->rearrangePage(this->page, index);
        }
    }

    Button::mouseMoveEvent(event);
}

void NotebookTab::wheelEvent(QWheelEvent *event)
{
    const auto defaultMouseDelta = 120;
    const auto verticalDelta = event->angleDelta().y();
    const auto selectTab = [this](int delta) {
        delta > 0 ? this->notebook_->selectPreviousTab()
                  : this->notebook_->selectNextTab();
    };
    // If it's true
    // Then the user uses the trackpad or perhaps the most accurate mouse
    // Which has small delta.
    if (std::abs(verticalDelta) < defaultMouseDelta)
    {
        this->mouseWheelDelta_ += verticalDelta;
        if (std::abs(this->mouseWheelDelta_) >= defaultMouseDelta)
        {
            selectTab(this->mouseWheelDelta_);
            this->mouseWheelDelta_ = 0;
        }
    }
    else
    {
        selectTab(verticalDelta);
    }
}

void NotebookTab::update()
{
    Button::update();
}

QRect NotebookTab::getXRect() const
{
    QRect rect = this->rect();
    float s = this->scale();
    int size = static_cast<int>(16 * s);

    int centerAdjustment = this->tabLocation_ == NotebookTabLocation::Top
                               ? (size / 3)   // slightly off true center
                               : (size / 2);  // true center

    float compactReducer = getCompactReducer(getSettings()->tabStyle);
    QRect xRect(rect.right() - static_cast<int>((20 - compactReducer) * s),
                rect.center().y() - centerAdjustment, size, size);

    if (this->selected_)
    {
        translateRectForLocation(xRect, this->tabLocation_, 1);
    }

    return xRect;
}

const QString &NotebookTab::getGroupName() const
{
    return this->groupName_;
}

void NotebookTab::setGroupName(const QString &name)
{
    if (this->groupName_ == name)
    {
        return;
    }

    this->groupName_ = name;
    this->update();
}

bool NotebookTab::isInGroup() const
{
    return !this->groupName_.isEmpty();
}

bool NotebookTab::isGroupCollapsed() const
{
    if (this->groupName_.isEmpty())
    {
        return false;
    }

    return this->notebook_->isTabGroupCollapsed(this->groupName_);
}

bool NotebookTab::isGroupAlwaysVisible() const
{
    if (this->groupName_.isEmpty())
    {
        return false;
    }

    return this->notebook_->isTabGroupAlwaysVisible(this->groupName_);
}

Notebook *NotebookTab::notebook() const
{
    return this->notebook_;
}

void NotebookTab::rebuildTabGroupMenu()
{
    this->tabGroupMenu_->clear();

    this->tabGroupMenu_->addAction("New Group...", [this] {
        bool accepted = false;
        auto name = QInputDialog::getText(this, "New Tab Group",
                                          "Group name:", QLineEdit::Normal,
                                          QString(), &accepted)
                        .trimmed();

        if (accepted && !name.isEmpty())
        {
            this->notebook_->addTabToGroup(this, name);
        }
    });

    const auto groupNames = this->notebook_->tabGroupNames();
    if (!groupNames.isEmpty())
    {
        this->tabGroupMenu_->addSeparator();
    }

    for (const auto &name : groupNames)
    {
        auto *action = this->tabGroupMenu_->addAction(name, [this, name] {
            this->notebook_->addTabToGroup(this, name);
        });
        action->setCheckable(true);
        action->setChecked(name == this->groupName_);
    }

    if (this->isInGroup())
    {
        this->tabGroupMenu_->addSeparator();
        this->tabGroupMenu_->addAction("Remove from Group", [this] {
            this->notebook_->removeTabFromGroup(this);
        });
    }
}

const QColor &NotebookTab::getCustomColor() const
{
    return this->customColor_;
}

void NotebookTab::setCustomColor(const QColor &color)
{
    if (this->customColor_ == color)
    {
        return;
    }

    this->customColor_ = color;

    // Queue up save because: Tab colour changed
    getApp()->getWindows()->queueSave();

    this->update();
}

bool NotebookTab::hasCustomColor() const
{
    return this->customColor_.isValid();
}

void NotebookTab::buildColorMenu(
    QMenu *menu, QWidget *parent, const QColor &current,
    const std::function<void(const QColor &)> &apply)
{
    // A small palette that stays legible on both the light and the dark theme
    static const std::vector<std::pair<const char *, const char *>> presets{
        {"Red", "#e5484d"},    {"Orange", "#f76b15"}, {"Yellow", "#ffb224"},
        {"Green", "#30a46c"},  {"Teal", "#12a594"},   {"Blue", "#0091ff"},
        {"Purple", "#8e4ec6"}, {"Pink", "#e93d82"},
    };

    auto swatch = [](const QColor &color) {
        QPixmap pixmap(16, 16);
        pixmap.fill(color);
        return QIcon(pixmap);
    };

    for (const auto &[name, hex] : presets)
    {
        QColor color(hex);
        auto *action = menu->addAction(swatch(color), name, [apply, color] {
            apply(color);
        });
        action->setCheckable(true);
        action->setChecked(current == color);
    }

    menu->addSeparator();

    menu->addAction("Custom Color...", [apply, current, parent] {
        auto color = QColorDialog::getColor(
            current.isValid() ? current : QColor("#0091ff"), parent, "Color");

        if (color.isValid())
        {
            apply(color);
        }
    });

    if (current.isValid())
    {
        menu->addAction("Remove Color", [apply] {
            apply(QColor());
        });
    }
}

void NotebookTab::rebuildTabColorMenu()
{
    this->tabColorMenu_->clear();

    // A grouped tab has no colour of its own - the whole group is marked, so
    // the menu applies to every tab in it.
    if (this->isInGroup())
    {
        const auto groupName = this->groupName_;
        this->tabColorMenu_->setTitle("Group Color");
        buildColorMenu(this->tabColorMenu_, this,
                       this->notebook_->tabGroupColor(groupName),
                       [this, groupName](const QColor &color) {
                           this->notebook_->setTabGroupColor(groupName, color);
                       });
        return;
    }

    this->tabColorMenu_->setTitle("Tab Color");
    buildColorMenu(this->tabColorMenu_, this, this->customColor_,
                   [this](const QColor &color) {
                       this->setCustomColor(color);
                   });
}

bool NotebookTab::hasFullyRoundedCorners() const
{
    return false;
}

QPainterPath NotebookTab::tabShapePath(const QRectF &rect, float scale) const
{
    QPainterPath path;

    // Group headers keep their classic shape and colour in either look - they
    // are a label for the tabs beneath them, not another tab, and giving them
    // the same treatment made the two harder to tell apart.
    const bool modern = getSettings()->uiStyle == UiStyle::Modern &&
                        !this->hasFullyRoundedCorners();

    if (this->hasFullyRoundedCorners())
    {
        const auto radius = qreal(6.0 * scale);
        path.addRoundedRect(rect, radius, radius);
        return path;
    }

    const auto radius = qreal((modern ? 8.0 : 4.0) * scale);
    path.addRoundedRect(rect, radius, radius);

    if (modern)
    {
        // Modern lets a tab float free of the page, so every corner stays
        // round - the same shape the group headers already have.
        return path;
    }

    // Square off the edge that faces the page, so the tab still sits flush
    // against the content it belongs to. Only the two corners on the
    // notebook's outer edge stay rounded.
    QRectF squareEdge = rect;
    switch (this->tabLocation_)
    {
        case NotebookTabLocation::Top:
            squareEdge.setTop(rect.top() + radius);
            break;
        case NotebookTabLocation::Bottom:
            squareEdge.setBottom(rect.bottom() - radius);
            break;
        case NotebookTabLocation::Left:
            squareEdge.setLeft(rect.left() + radius);
            break;
        case NotebookTabLocation::Right:
            squareEdge.setRight(rect.right() - radius);
            break;
    }

    QPainterPath squarePath;
    squarePath.addRect(squareEdge);

    return path.united(squarePath);
}

bool NotebookTab::usesBoldTitle() const
{
    return false;
}

QFont NotebookTab::titleFont() const
{
    auto font = getApp()->getFonts()->getFont(FontStyle::UiTabs, this->scale());

    if (this->usesBoldTitle())
    {
        font.setBold(true);
    }

    return font;
}

}  // namespace chatterino
