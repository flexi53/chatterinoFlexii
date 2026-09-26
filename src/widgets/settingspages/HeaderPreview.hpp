// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BaseWidget.hpp"
#include "widgets/splits/HeaderParts.hpp"

#include <pajlada/signals/signalholder.hpp>
#include <QPixmap>
#include <QRect>

#include <map>
#include <optional>
#include <vector>

namespace chatterino {

class ActivityGraph;
class DrawnButton;
class HeaderPicture;
class HeaderTitle;
class Label;
class LabelButton;
class SvgButton;

/// Buttons -> Title bar: the split header as it will look, made of the
/// header's own parts. A part is dragged to another place, and the edge of
/// the curve facing the title makes the curve wider or narrower - both
/// land in the settings when the mouse lets go.
class HeaderPreview : public BaseWidget
{
public:
    explicit HeaderPreview(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    struct Placed {
        headerparts::Part part;
        QRect rect;
    };
    /// Where each part stands right now
    const std::vector<Placed> &placed() const;
    /// Where the curve's edge can be taken hold of - empty without a curve
    QRect grip() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;
    void scaleChangedEvent(float scale) override;
    void themeChangedEvent() override;

private:
    /// Works out where everything goes, from the order being shown and
    /// the share of the curve
    void relayout();
    /// The order and the curve's share back to what the settings say
    void reload();

    int headerHeight() const;
    QRect headerRect() const;
    std::optional<headerparts::Part> partAt(QPoint pos) const;
    bool onGrip(QPoint pos) const;

    /// An edge a part can be taken hold of by: its right one makes the part
    /// wider or narrower, its left one the room before it
    struct Edge {
        headerparts::Part part;
        bool right;

        bool operator==(const Edge &other) const = default;
    };
    /// The edge under @a pos, if there is one
    std::optional<Edge> edgeAt(QPoint pos) const;
    /// Where that edge stands, for the handle drawn on it
    QRect edgeRect(Edge edge) const;
    /// How much wider @a part is drawn than usual, while being dragged the
    /// value the mouse is at
    int deltaOf(headerparts::Part part) const;
    /// The share the curve has while its edge is at @a x
    int shareAt(int x) const;
    QWidget *widgetFor(headerparts::Part part) const;
    /// Takes a picture of each part as it looks now - painted from those,
    /// as a widget can not be painted into another while that one paints
    void takePictures();
    /// The same, once the layout has settled. Taking them right away can
    /// lay the page out anew, which would lay this out and take them again.
    void takePicturesSoon();

    std::vector<headerparts::Part> order_;
    int share_{};
    std::vector<Placed> placed_;
    /// What each part looks like - kept by part, so a part being dragged
    /// keeps its picture wherever it goes
    std::map<headerparts::Part, QPixmap> pictures_;
    bool picturesPending_{false};
    bool takingPictures_{false};
    /// The room the title and the curve have together
    int shared_{};

    std::optional<headerparts::Part> pressed_;
    QPoint pressedAt_;
    bool movingPart_{false};
    bool movingGrip_{false};
    bool hoverGrip_{false};

    /// The edge under the mouse, and the one being dragged with what it
    /// started at - while dragging these say what is drawn, the settings
    /// only hear about it once the mouse is let go
    std::optional<Edge> hoverEdge_;
    std::optional<Edge> movingEdge_;
    int edgeStart_{};
    int spacing_{};
    std::map<headerparts::Part, int> deltas_;

    HeaderPicture *picture_{};
    HeaderPicture *cover_{};
    HeaderTitle *title_{};
    ActivityGraph *activity_{};
    LabelButton *mode_{};
    SvgButton *moderation_{};
    SvgButton *chatters_{};
    SvgButton *tracker_{};
    DrawnButton *menu_{};
    DrawnButton *add_{};

    pajlada::Signals::SignalHolder connections_;
};

}  // namespace chatterino
