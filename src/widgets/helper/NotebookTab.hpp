// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Common.hpp"
#include "widgets/buttons/Button.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/Notebook.hpp"

#include <pajlada/settings/setting.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QColor>
#include <QFont>
#include <QMenu>
#include <QPainterPath>
#include <QPropertyAnimation>

#include <functional>

namespace chatterino {

inline constexpr int NOTEBOOK_TAB_HEIGHT = 28;

class SplitContainer;

class NotebookTab : public Button
{
    Q_OBJECT

public:
    explicit NotebookTab(Notebook *notebook);

    void updateSize();

    QWidget *page{};

    void setCustomTitle(const QString &title);
    void resetCustomTitle();
    bool hasCustomTitle() const;
    const QString &getCustomTitle() const;
    void setDefaultTitle(const QString &title);
    const QString &getDefaultTitle() const;
    const QString &getTitle() const;

    /// The name of the tab group this tab belongs to, or an empty string if
    /// this tab is not grouped.
    const QString &getGroupName() const;
    void setGroupName(const QString &name);
    bool isInGroup() const;

    /// A user-chosen colour marking this tab. Returns an invalid QColor when
    /// the tab is not marked.
    const QColor &getCustomColor() const;
    void setCustomColor(const QColor &color);
    bool hasCustomColor() const;
    /// Whether the group this tab belongs to is currently collapsed. Always
    /// false for ungrouped tabs.
    bool isGroupCollapsed() const;

    /// Whether the group this tab belongs to is shown regardless of the
    /// notebook's tab visibility filter. Always false for ungrouped tabs.
    bool isGroupAlwaysVisible() const;

    bool isSelected() const;
    void setSelected(bool value);

    void setInLastRow(bool value);
    void setTabLocation(NotebookTabLocation location);

    /**
     * @brief Sets the live status of this tab
     *
     * Returns true if the live status was changed, false if nothing changed.
     **/
    bool setLive(bool isLive);

    /**
     * @brief Sets the rerun status of this tab
     *
     * Returns true if the rerun status was changed, false if nothing changed.
     **/
    bool setRerun(bool isRerun);

    /**
     * @brief Returns true if any split in this tab is live
     **/
    bool isLive() const;

    /**
     * @brief Sets the highlight state of this tab clearing highlight sources
     *
     * Obeys the HighlightsEnabled setting and highlight states hierarchy
     */
    void setHighlightState(HighlightState style);
    /**
     * @brief Updates the highlight state and highlight sources of this tab
     *
     * Obeys the HighlightsEnabled setting and the highlight state hierarchy and tracks the highlight state update sources
     */
    void updateHighlightState(HighlightState style,
                              const ChannelView &channelViewSource);
    void copyHighlightStateAndSourcesFrom(const NotebookTab *sourceTab);
    void setHighlightsEnabled(const bool &newVal);
    void newHighlightSourceAdded(const ChannelView &channelViewSource);
    bool hasHighlightsEnabled() const;
    HighlightState highlightState() const;

    void moveAnimated(QPoint targetPos, bool animated = true);

    QRect getDesiredRect() const;
    void tabSizeChanged();

    void growWidth(int width);
    int normalTabWidth() const;

protected:
    /// The Notebook this tab lives in. Available to subclasses such as
    /// NotebookTabGroupHeader.
    Notebook *notebook() const;

    /// Whether all four corners are rounded instead of only the two on the
    /// notebook's outer edge. Group headers use this to read as a label
    /// rather than as another tab.
    virtual bool hasFullyRoundedCorners() const;

    /// Whether the title is drawn in bold. Group headers use this to stand out
    /// from the tabs that belong to them.
    virtual bool usesBoldTitle() const;

    /// The font the title is drawn with, honouring usesBoldTitle()
    QFont titleFont() const;

    /// Fills @a menu with the shared colour palette. @a apply is handed the
    /// chosen colour, or an invalid QColor when the user removes the marking.
    static void buildColorMenu(
        QMenu *menu, QWidget *parent, const QColor &current,
        const std::function<void(const QColor &)> &apply);

    void themeChangedEvent() override;

    void paintEvent(QPaintEvent *) override;
    void paintContent(QPainter &painter) override
    {
    }

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *) override;

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    /// This exists as an alias to its base classes update, and is virtual
    /// to allow for mocking
    virtual void update();

private:
    void showRenameDialog();
    void rebuildTabGroupMenu();
    void rebuildTabColorMenu();

    /// The rounded outline of this tab. Everything painting the tab body is
    /// clipped to it so nothing bleeds past the rounded corners.
    QPainterPath tabShapePath(const QRectF &rect, float scale) const;

    virtual bool hasXButton() const;
    bool shouldDrawXButton() const;
    QRect getXRect() const;
    void titleUpdated();

    int normalTabWidthForHeight(int height) const;

    bool shouldMessageHighlight(const ChannelView &channelViewSource) const;

    using HighlightSources =
        std::unordered_map<ChannelView::ChannelViewID, HighlightState>;
    HighlightSources highlightSources_;

    void removeHighlightStateChangeSources(const HighlightSources &toRemove);
    void removeHighlightSource(const ChannelView::ChannelViewID &source);
    void updateHighlightStateDueSourcesChange();

    void recreateCloseMultipleTabsMenu(NotebookTabLocation tabLocation);

    QPropertyAnimation positionChangedAnimation_;
    QPoint positionAnimationDesiredPoint_;

    Notebook *notebook_;

    QString customTitle_;
    QString defaultTitle_;
    QString groupName_;
    QColor customColor_;

    bool selected_{};
    bool mouseOver_{};
    bool mouseDown_{};
    bool mouseOverX_{};
    bool mouseDownX_{};
    bool isInLastRow_{};
    int mouseWheelDelta_ = 0;
    NotebookTabLocation tabLocation_ = NotebookTabLocation::Top;

    HighlightState highlightState_ = HighlightState::None;
    bool highlightEnabled_ = true;
    QAction *highlightNewMessagesAction_;

    bool isLive_{};
    bool isRerun_{};

    int growWidth_ = 0;

    QMenu menu_;
    QMenu *closeMultipleTabsMenu_{};
    QMenu *tabGroupMenu_{};
    QMenu *tabColorMenu_{};
    QAction *closeTabsBeforeSelectedAction_{};
    QAction *closeTabsAfterSelectedAction_{};

    pajlada::Signals::SignalHolder managedConnections_;
};

}  // namespace chatterino
