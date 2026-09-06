// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/helper/NotebookTab.hpp"

#include <QMenu>
#include <QString>

namespace chatterino {

class Notebook;

/**
 * @brief The clickable header drawn in front of a group of tabs.
 *
 * A header is not a page of the Notebook - it never enters the Notebook's item
 * list and therefore never takes part in tab selection, closing or reordering.
 * The Notebook injects it into the layout right before the first tab of its
 * group.
 *
 * Clicking the header collapses or expands the group, double clicking renames
 * it.
 */
class NotebookTabGroupHeader : public NotebookTab
{
    Q_OBJECT

public:
    NotebookTabGroupHeader(Notebook *notebook, QString groupName);

    const QString &getGroupName() const;
    void setGroupName(const QString &name);

    bool isCollapsed() const;
    void setCollapsed(bool collapsed);

    /// The number of tabs shown in the header's label
    void setMemberCount(int count);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    /// A header can never be closed, so it never reserves room for an X button
    bool hasXButton() const override;

    /// Headers are drawn as a fully rounded chip so they read as a label for
    /// the group rather than as another tab.
    bool hasFullyRoundedCorners() const override;

    /// Group names are drawn bold so the header reads as a heading
    bool usesBoldTitle() const override;

private:
    void updateTitle();
    void showGroupRenameDialog();

    QString groupName_;
    bool collapsed_ = false;
    int memberCount_ = 0;
    /// Set while the left button is held down, so the header can be dragged
    bool mouseDown_ = false;
    /// Set once a press turned into a drag, so releasing does not also toggle
    bool dragged_ = false;
    /// Set by a double click so the release that follows does not toggle again
    bool suppressNextClick_ = false;

    QMenu groupMenu_;
    QMenu *colorMenu_{};
};

}  // namespace chatterino
