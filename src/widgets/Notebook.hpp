// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"
#include "widgets/NotebookEnums.hpp"

#include <pajlada/signals/signal.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QColor>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <span>
#include <vector>

namespace chatterino {

class Button;
class PixmapButton;
class Window;
class DrawnButton;
class NotebookTab;
class NotebookTabGroupHeader;
class SplitContainer;
class Split;

using TabVisibilityFilter = std::function<bool(const NotebookTab *)>;

class Notebook : public BaseWidget
{
    Q_OBJECT

public:
    explicit Notebook(QWidget *parent);
    ~Notebook() override = default;

    NotebookTab *addPage(QWidget *page, QString title = QString(),
                         bool select = false);

    /**
     * @brief Adds a page to the Notebook at a given position.
     *
     * @param position if set to -1, adds the page to the end
     **/
    NotebookTab *addPageAt(QWidget *page, int position,
                           QString title = QString(), bool select = false);
    void removePage(QWidget *page);
    void duplicatePage(QWidget *page);
    void removeCurrentPage();

    /**
     * @brief Returns index of page in Notebook, or -1 if not found.
     **/
    int indexOf(QWidget *page) const;

    /**
     * @brief Returns the visible index of page in Notebook, or -1 if not found.
     * Given page should be visible according to the set TabVisibilityFilter.
     **/
    int visibleIndexOf(QWidget *page) const;

    /**
     * @brief Returns the number of visible tabs in Notebook. 
     **/
    int getVisibleTabCount() const;

    /**
     * @brief Selects the Notebook tab containing the given page.
     **/
    virtual void select(QWidget *page, bool focusPage = true);

    /**
     * @brief Selects the Notebook tab at the given index. Ignores whether tabs
     * are visible or not. 
     **/
    void selectIndex(int index, bool focusPage = true);

    /**
     * @brief Selects the index'th visible tab in the Notebook.
     * 
     * For example, selecting the 0th visible tab selects the first tab in this 
     * Notebook that is visible according to the TabVisibilityFilter. If no filter
     * is set, equivalent to Notebook::selectIndex.
     **/
    void selectVisibleIndex(int index, bool focusPage = true);

    /**
     * @brief Selects the next visible tab. Wraps to the start if required. 
     **/
    void selectNextTab(bool focusPage = true);

    /**
     * @brief Selects the previous visible tab. Wraps to the end if required. 
     **/
    void selectPreviousTab(bool focusPage = true);

    /**
     * @brief Selects the last visible tab. 
     **/
    void selectLastTab(bool focusPage = true);

    int getPageCount() const;
    QWidget *getPageAt(int index) const;
    int getSelectedIndex() const;
    QWidget *getSelectedPage() const;

    QWidget *tabAt(QPoint point, int &index, int maxWidth = 2000000000);
    void rearrangePage(QWidget *page, int index);

    bool getAllowUserTabManagement() const;
    void setAllowUserTabManagement(bool value);

    bool getShowAddButton() const;
    void setShowAddButton(bool value);

    void setTabLocation(NotebookTabLocation location);

    bool isNotebookLayoutLocked() const;
    virtual void setLockNotebookLayout(bool value);

    virtual void addNotebookActionsToMenu(QMenu *menu);

    /// @name Tab groups
    /// Tabs can be collected under a named, collapsible header. Grouped tabs
    /// are kept next to each other; the header is drawn in front of the first
    /// tab of its group.
    /// @{

    /// Puts @a tab into the group @a groupName, creating the group if needed.
    /// Passing an empty name removes the tab from its current group.
    void addTabToGroup(NotebookTab *tab, const QString &groupName);
    void removeTabFromGroup(NotebookTab *tab);

    /// Moves a whole group - header and all of its tabs - so that it starts at
    /// @a index. Used when a group header is dragged.
    void moveTabGroup(const QString &groupName, int index);
    /// The position of the group's first tab in the notebook, or -1.
    int tabGroupIndex(const QString &groupName) const;

    /// The names of all groups that currently have at least one tab, in the
    /// order their first tab appears.
    QStringList tabGroupNames() const;

    /// Rearranges the groups into @a order. Each group's block moves into the
    /// slot another block used to occupy, so ungrouped tabs keep their place.
    void setTabGroupOrder(const QStringList &order);

    /// Opens the window listing every group, where they can be reordered,
    /// renamed, recoloured and dissolved.
    void showTabGroupsDialog();

    void renameTabGroup(const QString &oldName, const QString &newName);
    /// Removes the group and returns its tabs to the ungrouped state.
    void dissolveTabGroup(const QString &name);

    /// The colour shared by every tab of the group, or an invalid QColor if
    /// the group is unmarked.
    QColor tabGroupColor(const QString &name) const;
    /// Marks the whole group. Every current member takes this colour, and so
    /// does every tab added to the group later.
    void setTabGroupColor(const QString &name, const QColor &color);

    bool isTabGroupCollapsed(const QString &name) const;
    void setTabGroupCollapsed(const QString &name, bool collapsed);

    /// Whether the group ignores the "only show live tabs" filter.
    bool isTabGroupAlwaysVisible(const QString &name) const;
    void setTabGroupAlwaysVisible(const QString &name, bool alwaysVisible);
    void toggleTabGroupAlwaysVisible(const QString &name);
    void toggleTabGroupCollapsed(const QString &name);

    /// @}

    // Update layout and tab visibility
    void refresh();

protected:
    bool getShowTabs() const;
    void setShowTabs(bool value);

    void scaleChangedEvent(float scale_) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *) override;

    DrawnButton *addButton_;

    template <typename T>
    T *addCustomButton(auto &&...args)
    {
        auto *btn = new T(std::forward<decltype(args)>(args)..., this);
        this->customButtons_.push_back(btn);

        return btn;
    }

    struct Item {
        NotebookTab *tab{};
        QWidget *page{};
        QWidget *selectedWidget{};

        /// Set while laying out: this item has to start a new row. Used to
        /// keep tab groups on rows of their own.
        bool startsNewRow{false};
    };

    const QList<Item> items()
    {
        return this->items_;
    }

    /**
     * @brief Apply the given tab visibility filter
     *
     * An empty function can be provided to denote that no filter will be applied
     *
     * Tabs will be redrawn after this function is called.
     **/
    void setTabVisibilityFilter(TabVisibilityFilter filter);

    /**
     * @brief shouldShowTab has the final say whether a tab should be visible right now.
     **/
    bool shouldShowTab(const NotebookTab *tab) const;

    void performLayout(bool animate = false);

    void sortTabsAlphabetically();

private:
    struct LayoutContext {
        int left = 0;
        int right = 0;
        int bottom = 0;
        float scale = 0;
        int tabHeight = 0;
        int minimumTabAreaSpace = 0;
        int addButtonWidth = 0;
        int lineThickness = 0;
        int tabSpacer = 0;
        /// Air between two rows of tabs, so a wrapped tab bar does not read as
        /// one solid block
        int rowSpacer = 0;

        int buttonWidth = 0;
        int buttonHeight = 0;

        std::span<Item> items;
    };

    void performHorizontalLayout(const LayoutContext &ctx, bool animated);
    void performVerticalLayout(const LayoutContext &ctx, bool animated);

    /**
     * @brief Show a popup informing the user of some big tab visibility changes
     **/
    void showTabVisibilityInfoPopup();

    /**
     * @brief Updates the visibility state of all tabs
     **/
    void updateTabVisibility();
    void resizeAddButton();

    struct TabGroup {
        QString name;
        bool collapsed = false;
        /// Exempts the group from "only show live tabs" - the tabs stay on
        /// screen whether or not their channel is live.
        bool alwaysVisible = false;
        QColor color;
        NotebookTabGroupHeader *header = nullptr;
    };

    TabGroup *findTabGroup(const QString &name);
    const TabGroup *findTabGroup(const QString &name) const;

    /// Drops empty groups, refreshes the headers and moves the tabs of each
    /// group next to each other. Triggers a re-layout.
    void syncTabGroups();
    void reorderGroupedTabs();

    /// Pulls the members of every group back together. Must run after anything
    /// that reorders items_, otherwise a group's tabs end up scattered and its
    /// header no longer marks where the group starts.
    void keepTabGroupsContiguous();

    /// Works out which group the tab at @a index belongs to after it was
    /// moved: a tab dropped between two members of the same group joins it,
    /// a tab dropped anywhere else ends up ungrouped.
    void updateTabGroupFromNeighbours(int index);

    /// Whether @a tab is hidden because its group is collapsed. The selected
    /// tab always stays visible, matching the behaviour of the tab visibility
    /// filter.
    bool isTabHiddenByGroup(const NotebookTab *tab) const;

    /// Whether @a tab is shown regardless of the tab visibility filter because
    /// its group is pinned open.
    bool isTabPinnedByGroup(const NotebookTab *tab) const;

    /// Whether @a group's header belongs on screen. A header whose group has
    /// nothing to show is a label for nothing, so it goes too, unless the
    /// group is pinned open. Collapsing does not count as having nothing to
    /// show - the header is the only way back out of it.
    bool shouldShowTabGroupHeader(const TabGroup &group) const;

    bool containsPage(QWidget *page);
    Item *findItem(QWidget *page);

    static bool containsChild(const QObject *obj, const QObject *child);
    NotebookTab *getTabFromPage(QWidget *page);

    // Returns the number of buttons in `customButtons_` that are visible
    size_t visibleButtonCount() const;

    QList<Item> items_;
    std::vector<TabGroup> tabGroups_;
    QMenu *menu_ = nullptr;
    QWidget *selectedPage_ = nullptr;

    std::vector<Button *> customButtons_;

    bool allowUserTabManagement_ = false;
    bool showTabs_ = true;
    bool showAddButton_ = false;
    int lineOffset_ = 20;
    bool lockNotebookLayout_ = false;

    bool refreshPaused_ = false;
    bool refreshRequested_ = false;

    NotebookTabLocation tabLocation_ = NotebookTabLocation::Top;

    QAction *lockNotebookLayoutAction_;
    QAction *tabGroupsOnOwnRowAction_;
    QAction *toggleTopMostAction_;

    // This filter, if set, is used to figure out the visibility of
    // the tabs in this notebook.
    TabVisibilityFilter tabVisibilityFilter_;
};

class SplitNotebook : public Notebook
{
public:
    SplitNotebook(Window *parent);

    SplitContainer *addPage(bool select = false);
    SplitContainer *getOrAddSelectedPage();
    /// Returns `nullptr` when no page is selected.
    SplitContainer *getSelectedPage();
    void select(QWidget *page, bool focusPage = true) override;
    void themeChangedEvent() override;

    void addNotebookActionsToMenu(QMenu *menu) override;

    void forEachSplit(const std::function<void(Split *)> &cb);

    /**
     * Toggles between the "Show all tabs" and "Hide all tabs" tab visibility states
     */
    void toggleTabVisibility();

    QAction *showAllTabsAction;
    QAction *onlyShowLiveTabsAction;
    QAction *hideAllTabsAction;

protected:
    void showEvent(QShowEvent *event) override;

private:
    QAction *sortTabsAlphabeticallyAction_;

    void addCustomButtons();

    pajlada::Signals::SignalHolder signalHolder_;

    // Main window on Windows has basically a duplicate of this in Window
    PixmapButton *streamerModeIcon_{};
    void updateStreamerModeIcon();

    void setLockNotebookLayout(bool value) override;
};

}  // namespace chatterino
