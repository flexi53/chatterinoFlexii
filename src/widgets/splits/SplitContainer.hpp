// SPDX-FileCopyrightText: 2016 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/WindowDescriptors.hpp"
#include "widgets/BaseWidget.hpp"
#include "widgets/splits/SplitCommon.hpp"

#include <pajlada/signals/signal.hpp>
#include <pajlada/signals/signalholder.hpp>
#include <QColor>
#include <QDragEnterEvent>
#include <QRectF>
#include <QRect>
#include <QWidget>

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

class QJsonObject;

namespace chatterino {

class Split;
class NotebookTab;
class Notebook;

//
// Note: This class is a spaghetti container. There is a lot of spaghetti code
// inside but it doesn't expose any of it publicly.
//

class SplitContainer final : public BaseWidget
{
    Q_OBJECT

public:
    struct Node;

    struct Position final {
    private:
        Position() = default;
        Position(Node *relativeNode, SplitDirection direction)
            : relativeNode_(relativeNode)
            , direction_(direction)
        {
        }

        Node *relativeNode_{nullptr};
        SplitDirection direction_{SplitDirection::Right};

        friend struct Node;
        friend class SplitContainer;
    };

private:
    struct DropRect final {
        QRect rect;
        Position position;

        DropRect(const QRect &_rect, const Position &_position)
            : rect(_rect)
            , position(_position)
        {
        }
    };

    struct ResizeRect final {
        QRect rect;
        Node *node;
        bool vertical;

        ResizeRect(const QRect &_rect, Node *_node, bool _vertical)
            : rect(_rect)
            , node(_node)
            , vertical(_vertical)
        {
        }
    };

public:
    struct Node final : public std::enable_shared_from_this<Node> {
        Node();
        Node(Split *_split, Node *_parent);

        enum class Type {
            EmptyRoot,
            Split,
            VerticalContainer,
            HorizontalContainer,
        };

        Type getType() const;
        Split *getSplit() const;
        Node *getParent() const;
        qreal getHorizontalFlex() const;
        qreal getVerticalFlex() const;
        /// ChattiFlexii: how much empty room follows this one inside its
        /// container, as a share of that container - 0 where it is followed
        /// by the next chat right away
        qreal getGapAfter() const;
        const std::vector<std::shared_ptr<Node>> &getChildren();

    private:
        bool isOrContainsNode(Node *_node);
        Node *findNodeContainingSplit(Split *_split);
        void insertSplitRelative(Split *_split, SplitDirection _direction);
        void nestSplitIntoCollection(Split *_split, SplitDirection _direction);
        void insertNextToThis(Split *_split, SplitDirection _direction);
        void setSplit(Split *_split);
        Position releaseSplit();
        qreal getFlex(bool isVertical);
        qreal getSize(bool isVertical);
        qreal getChildrensTotalFlex(bool isVertical);
        void layout(bool addSpacing, float _scale,
                    std::vector<DropRect> &dropRects_,
                    std::vector<ResizeRect> &resizeRects);

        // Clamps the flex values ensuring they're never below 0
        void clamp();

        static Type toContainerType(SplitDirection _dir);

        Type type_;
        Split *split_;
        Node *preferedFocusTarget_{};
        Node *parent_;
        QRectF geometry_;
        qreal flexH_ = 1;
        qreal flexV_ = 1;
        /// ChattiFlexii: empty room dragged in behind this one, as a share
        /// of the container it lies in. It comes out of this node's own
        /// place, so what follows keeps where it is.
        qreal gapAfter_ = 0;
        std::vector<std::shared_ptr<Node>> children_;

        friend class SplitContainer;
    };

private:
    class DropOverlay final : public QWidget
    {
    public:
        DropOverlay(SplitContainer *_parent = nullptr);

        void setRects(std::vector<SplitContainer::DropRect> _rects);

        pajlada::Signals::NoArgSignal dragEnded;

    protected:
        void paintEvent(QPaintEvent *event) override;
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dragMoveEvent(QDragMoveEvent *event) override;
        void dragLeaveEvent(QDragLeaveEvent *event) override;
        void dropEvent(QDropEvent *event) override;

    private:
        std::vector<DropRect> rects_;
        QPoint mouseOverPoint_;
        SplitContainer *parent_;
    };

    class ResizeHandle final : public QWidget
    {
    public:
        SplitContainer *parent;
        Node *node{};

        void setVertical(bool isVertical);
        /// ChattiFlexii: the empty room behind the one before the border,
        /// instead of moving the border itself
        void dragGap(QMouseEvent *event, Node *before);
        ResizeHandle(SplitContainer *_parent = nullptr);
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;
        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;

        friend class SplitContainer;

    private:
        void resetFlex();

        bool vertical_{};
        bool isMouseDown_ = false;
        /// ChattiFlexii: while the border may be dragged without a key, the
        /// handle only shows itself under the mouse
        bool mouseOver_ = false;
    };

public:
    SplitContainer(Notebook *parent);

    Split *appendNewSplit(bool openChannelNameDialog);

    /// ChattiFlexii: whether this tab only opened because the browser went
    /// to that channel. Such a tab goes again as soon as the browser moves
    /// on, and it is never written into the saved layout.
    bool isTemporary() const;
    void setTemporary(bool temporary);

    struct InsertOptions {
        /// Position must be set alone, as if it's set it will override direction & relativeNode with its underlying values
        std::optional<Position> position{};

        /// Will be used to figure out the relative node, so relative node or position must not be set if using this
        Split *relativeSplit{nullptr};

        Node *relativeNode{nullptr};
        std::optional<SplitDirection> direction{};
    };

    // Insert split into the base node of this container
    // Default values for each field must be specified due to these bugs:
    //  - https://bugs.llvm.org/show_bug.cgi?id=36684
    //  - https://gcc.gnu.org/bugzilla/show_bug.cgi?id=96645
    void insertSplit(Split *split, InsertOptions &&options = InsertOptions{
                                       .position = std::nullopt,
                                       .relativeSplit = nullptr,
                                       .relativeNode = nullptr,
                                       .direction = std::nullopt,
                                   });

    // Returns a pointer to the selected split
    Split *getSelectedSplit() const;
    Position releaseSplit(Split *split);
    Position deleteSplit(Split *split);

    void selectNextSplit(SplitDirection direction);
    void setSelected(Split *split);

    std::vector<Split *> getSplits() const;

    void refreshTab();

    NotebookTab *getTab() const;
    Node *getBaseNode();

    void setTab(NotebookTab *tab);
    void hideResizeHandles();
    void resetMouseStatus();

    NodeDescriptor buildDescriptor() const;
    void applyFromDescriptor(const NodeDescriptor &rootNode);

    void popup();

    /// ChattiFlexii: how much empty room follows a node, in pixels. @a share
    /// is what was dragged in, @a containerSize the size of the container
    /// along the direction it lays out in, @a slotSize what the node and its
    /// room have together, and @a minSize what has to be left of the node
    /// itself. A chat can be pushed small, never out of sight.
    static qreal gapSize(qreal share, qreal containerSize, qreal slotSize,
                         qreal minSize);

    /// ChattiFlexii: fills empty room between two chats the way the strip
    /// under the tabs is filled - the same colour, or the same gradient
    /// where one is set, so the room reads as a piece of the window showing
    /// through. Aussehen -> Splits can put a colour of its own there.
    /// @a on is the widget being painted, so a picture spread over the
    /// window shows the piece that belongs at this spot
    static void fillGap(QPainter &painter, const QRectF &where, Theme *theme,
                        const QWidget *on);

protected:
    void paintEvent(QPaintEvent *event) override;

    void focusInEvent(QFocusEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

private:
    NodeDescriptor buildDescriptorRecursively(const Node *currentNode) const;
    void applyFromDescriptorRecursively(const NodeDescriptor &rootNode,
                                        Node *baseNode);

    void layout();
    void selectSplitRecursive(Node *node, SplitDirection direction);
    void focusSplitRecursive(Node *node);
    void setPreferedTargetRecursive(Node *node);
    void paintSplitBorder(Node *node, QPainter *painter);
    /// ChattiFlexii: fills what lies between two chats where room was
    /// dragged in - nothing draws there otherwise
    void paintGaps(Node *node, QPainter *painter);

    void addSplit(Split *split);

    Split *getTopRightSplit(Node &node);

    void refreshTabTitle();
    void refreshTabLiveStatus();

    std::vector<DropRect> dropRects_;
    DropOverlay overlay_;
    std::vector<std::unique_ptr<ResizeHandle>> resizeHandles_;
    /// ChattiFlexii: only there while the browser is on that channel
    bool temporary_ = false;
    QPoint mouseOverPoint_;

    std::shared_ptr<Node> baseNode_;
    Split *selected_{};
    Split *topRight_{};
    bool disableLayouting_{};

    NotebookTab *tab_;
    std::vector<Split *> splits_;

    std::unordered_map<Split *, pajlada::Signals::SignalHolder>
        connectionsPerSplit_;

    pajlada::Signals::SignalHolder signalHolder_;

    // Specifies whether the user is currently dragging something over this container
    bool isDragging_ = false;
};

}  // namespace chatterino
