// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/NotebookTabGroupHeader.hpp"

#include "widgets/Notebook.hpp"

#include <QInputDialog>
#include <QMouseEvent>

namespace chatterino {

NotebookTabGroupHeader::NotebookTabGroupHeader(Notebook *notebook,
                                               QString groupName)
    : NotebookTab(notebook)
    , groupName_(std::move(groupName))
    , groupMenu_(this)
{
    this->groupMenu_.addAction("Rename Group", [this] {
        this->showGroupRenameDialog();
    });

    this->groupMenu_.addAction("Collapse/Expand Group", [this] {
        this->notebook()->toggleTabGroupCollapsed(this->groupName_);
    });

    this->alwaysVisibleAction_ =
        this->groupMenu_.addAction("Always Show Group", [this] {
            this->notebook()->toggleTabGroupAlwaysVisible(this->groupName_);
        });
    this->alwaysVisibleAction_->setCheckable(true);
    this->alwaysVisibleAction_->setToolTip(
        "Keep this group's tabs on screen even when \"Only show live tabs\" "
        "is on and the channels are offline.");

    this->colorMenu_ = new QMenu("Group Color", &this->groupMenu_);
    this->groupMenu_.addMenu(this->colorMenu_);

    // The palette is rebuilt every time so the current colour shows as checked
    QObject::connect(&this->groupMenu_, &QMenu::aboutToShow, this, [this] {
        this->alwaysVisibleAction_->setChecked(
            this->notebook()->isTabGroupAlwaysVisible(this->groupName_));

        this->colorMenu_->clear();
        NotebookTab::buildColorMenu(
            this->colorMenu_, this,
            this->notebook()->tabGroupColor(this->groupName_),
            [this](const QColor &color) {
                this->notebook()->setTabGroupColor(this->groupName_, color);
            });
    });

    this->groupMenu_.addSeparator();

    this->groupMenu_.addAction("Ungroup Tabs", [this] {
        this->notebook()->dissolveTabGroup(this->groupName_);
    });

    this->updateTitle();
}

const QString &NotebookTabGroupHeader::getGroupName() const
{
    return this->groupName_;
}

void NotebookTabGroupHeader::setGroupName(const QString &name)
{
    if (this->groupName_ == name)
    {
        return;
    }

    this->groupName_ = name;
    this->updateTitle();
}

bool NotebookTabGroupHeader::isCollapsed() const
{
    return this->collapsed_;
}

void NotebookTabGroupHeader::setCollapsed(bool collapsed)
{
    if (this->collapsed_ == collapsed)
    {
        return;
    }

    this->collapsed_ = collapsed;
    this->updateTitle();
}

void NotebookTabGroupHeader::setMemberCount(int count)
{
    if (this->memberCount_ == count)
    {
        return;
    }

    this->memberCount_ = count;
    this->updateTitle();
}

void NotebookTabGroupHeader::updateTitle()
{
    // U+25B8 / U+25BE - a small triangle pointing right when the group is
    // collapsed and down when it is expanded.
    const auto *arrow = this->collapsed_ ? "▸" : "▾";

    this->setDefaultTitle(QStringLiteral("%1 %2 (%3)")
                              .arg(QString::fromUtf8(arrow), this->groupName_,
                                   QString::number(this->memberCount_)));
}

void NotebookTabGroupHeader::showGroupRenameDialog()
{
    bool accepted = false;
    auto newName = QInputDialog::getText(this, "Rename Group",
                                         "Group name:", QLineEdit::Normal,
                                         this->groupName_, &accepted)
                       .trimmed();

    if (!accepted || newName.isEmpty() || newName == this->groupName_)
    {
        return;
    }

    this->notebook()->renameTabGroup(this->groupName_, newName);
}

void NotebookTabGroupHeader::mousePressEvent(QMouseEvent *event)
{
    switch (event->button())
    {
        case Qt::LeftButton:
            // Collapsing happens on release, so that dragging the group does
            // not collapse it on the way.
            this->mouseDown_ = true;
            this->dragged_ = false;
            this->suppressNextClick_ = false;
            break;

        case Qt::RightButton:
            this->groupMenu_.popup(event->globalPosition().toPoint() +
                                   QPoint(0, 8));
            break;

        default:
            break;
    }

    this->update();
}

void NotebookTabGroupHeader::mouseReleaseEvent(QMouseEvent *event)
{
    const bool wasClick = this->mouseDown_ && !this->dragged_ &&
                          event->button() == Qt::LeftButton &&
                          this->rect().contains(event->pos());

    this->mouseDown_ = false;

    if (wasClick && !this->suppressNextClick_)
    {
        this->notebook()->toggleTabGroupCollapsed(this->groupName_);
    }
    this->suppressNextClick_ = false;

    // Swallowed otherwise - a header is not a tab, so it must not be closed by
    // a middle click the way NotebookTab handles it.
    event->accept();
}

void NotebookTabGroupHeader::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // The first click of the double click already toggled the group, so
        // undo that and keep the release that follows from toggling again.
        this->notebook()->toggleTabGroupCollapsed(this->groupName_);
        this->suppressNextClick_ = true;
        this->showGroupRenameDialog();
    }
}

void NotebookTabGroupHeader::mouseMoveEvent(QMouseEvent *event)
{
    // Dragging the header moves the whole group, the same way dragging a tab
    // moves that tab.
    if (this->mouseDown_ && this->notebook()->getAllowUserTabManagement())
    {
        const auto relPoint = this->mapToParent(event->pos());

        if (!this->getDesiredRect().contains(relPoint))
        {
            int index = -1;
            auto *targetPage =
                this->notebook()->tabAt(relPoint, index, this->width());

            if (targetPage != nullptr && index != -1)
            {
                this->dragged_ = true;
                this->notebook()->moveTabGroup(this->groupName_, index);
            }
        }
    }

    event->accept();
}

void NotebookTabGroupHeader::dragEnterEvent(QDragEnterEvent *event)
{
    event->ignore();
}

void NotebookTabGroupHeader::dropEvent(QDropEvent *event)
{
    event->ignore();
}

void NotebookTabGroupHeader::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}

bool NotebookTabGroupHeader::hasXButton() const
{
    return false;
}

bool NotebookTabGroupHeader::hasFullyRoundedCorners() const
{
    return true;
}

bool NotebookTabGroupHeader::usesBoldTitle() const
{
    return true;
}

}  // namespace chatterino
