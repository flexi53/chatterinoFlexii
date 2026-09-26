// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/HeaderPreview.hpp"

#include "Application.hpp"
#include "common/network/NetworkCommon.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/ProfilePictures.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/UiStyle.hpp"
#include "widgets/buttons/DrawnButton.hpp"
#include "widgets/buttons/LabelButton.hpp"
#include "widgets/buttons/SvgButton.hpp"
#include "widgets/Label.hpp"
#include "widgets/splits/SplitHeaderExtras.hpp"

#include <QApplication>
#include <QHelpEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace chatterino {

namespace {

using headerparts::Part;

/// Room above and below the header, and for the line under it
constexpr int MARGIN = 4;
constexpr int NOTE_HEIGHT = 16;

/// What the preview's title says: your own name, or any
QString sampleName()
{
    auto account = getApp()->getAccounts()->twitch.getCurrent();
    if (account == nullptr || account->isAnon())
    {
        return QStringLiteral("kanal");
    }
    return account->getUserName();
}

/// The title as a live stream would have it, with what is switched on
/// The title of the preview, and which stretches of it carry a colour
QString sampleTitle(std::vector<headerparts::Run> *runs = nullptr)
{
    TwitchChannel::StreamStatus status;
    status.live = true;
    status.streamType = QStringLiteral("live");
    status.uptime = QStringLiteral("2h 13m");
    status.viewerCount = 1234;
    status.game = QStringLiteral("Just Chatting");
    status.title = QStringLiteral("Titel des Streams");
    // Made-up numbers, so the checkboxes show what they do
    const headerparts::Extras extras{
        .followers = 48250,
        .chatters = 1730,
        .messagesPerMinute = 42,
        .viewerTrend = 0.18,
    };
    return headerparts::composeTitle(
        sampleName(), headerparts::titleAfterName(status, extras, runs),
        headerparts::isShown(Part::Picture), runs);
}

/// Stands in for a picture until the real one is there
QPixmap placeholder(QSize size, QColor top, QColor bottom)
{
    QPixmap pixmap(size);
    {
        QPainter painter(&pixmap);
        QLinearGradient gradient(0, 0, 0, size.height());
        gradient.setColorAt(0, top);
        gradient.setColorAt(1, bottom);
        painter.fillRect(pixmap.rect(), gradient);
    }
    return pixmap;
}

}  // namespace

HeaderPreview::HeaderPreview(QWidget *parent)
    : BaseWidget(parent)
{
    this->setMouseTracking(true);

    this->picture_ = new HeaderPicture(HeaderPicture::Shape::Round, 3, this);
    this->picture_->setPicture(
        placeholder({32, 32}, QColor(145, 70, 255), QColor(100, 65, 165)));
    this->cover_ = new HeaderPicture(HeaderPicture::Shape::Cover, 3, this);
    this->cover_->setPicture(
        placeholder({52, 72}, QColor(90, 90, 110), QColor(50, 50, 60)));

    this->title_ = new HeaderTitle(this, sampleTitle());
    this->title_->setCentered(true);
    this->title_->setPadding(QMargins{});
    this->title_->setShouldElide(true);

    this->activity_ = new ActivityGraph(this);
    this->activity_->showSample();

    this->mode_ = new LabelButton("slow", this);
    this->moderation_ = new SvgButton(
        {
            .dark = ":/buttons/moderationDisabled-darkMode.svg",
            .light = ":/buttons/moderationDisabled-lightMode.svg",
        },
        this, {5, 5});
    this->chatters_ = new SvgButton(
        {
            .dark = ":/buttons/chatters-darkMode.svg",
            .light = ":/buttons/chatters-lightMode.svg",
        },
        this, {4, 4});
    this->tracker_ = new SvgButton(
        {
            .dark = ":/buttons/tracker-darkMode.svg",
            .light = ":/buttons/tracker-lightMode.svg",
        },
        this, {5, 5});
    this->menu_ = new DrawnButton(DrawnButton::Symbol::Kebab, {}, this);
    this->add_ = new DrawnButton(DrawnButton::Symbol::Plus,
                                 {
                                     .padding = 3,
                                     .thickness = 1,
                                 },
                                 this);

    // They are only painted from here, never shown on their own
    for (auto *widget : std::initializer_list<QWidget *>{
             this->picture_, this->cover_, this->title_, this->activity_,
             this->mode_, this->moderation_, this->chatters_, this->tracker_,
             this->menu_, this->add_})
    {
        widget->hide();
        widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    // The real pictures, once they are there
    const auto login = sampleName();
    if (login != QStringLiteral("kanal"))
    {
        profilepictures::pixmap(login, 32, this, [this](const QPixmap &p) {
            this->picture_->setPicture(p);
            this->picture_->hide();
            this->takePicturesSoon();
        });
    }
    NetworkRequest(
        QStringLiteral(
            "https://static-cdn.jtvnw.net/ttv-boxart/509658-52x72.jpg"),
        NetworkRequestType::Get)
        .cache()
        .caller(this)
        .onSuccess([this](const NetworkResult &result) {
            QPixmap cover;
            if (cover.loadFromData(result.getData()))
            {
                this->cover_->setPicture(cover);
                this->cover_->hide();
                this->takePicturesSoon();
            }
        })
        .execute();

    auto *s = getSettings();
    const auto reload = [this] {
        this->reload();
    };
    s->splitHeaderOrder.connect(reload, this->connections_, false);
    s->splitHeaderHidden.connect(reload, this->connections_, false);
    s->splitHeaderPictures.connect(reload, this->connections_, false);
    s->splitHeaderActivity.connect(reload, this->connections_, false);
    s->splitHeaderActivityShare.connect(reload, this->connections_, false);
    s->headerLiveMarker.connect(reload, this->connections_, false);
    s->headerChannelName.connect(reload, this->connections_, false);
    s->uiStyle.connect(
        [this] {
            this->updateGeometry();
            this->reload();
        },
        this->connections_, false);
    s->headerUptime.connect(reload, this->connections_, false);
    s->headerViewerCount.connect(reload, this->connections_, false);
    s->headerViewerTrend.connect(reload, this->connections_, false);
    s->headerFollowers.connect(reload, this->connections_, false);
    s->headerChatters.connect(reload, this->connections_, false);
    s->headerMessageRate.connect(reload, this->connections_, false);
    s->headerColors.connect(reload, this->connections_, false);
    s->splitHeaderSpacing.connect(reload, this->connections_, false);
    s->splitHeaderWidths.connect(reload, this->connections_, false);
    s->headerGame.connect(reload, this->connections_, false);
    s->headerStreamTitle.connect(reload, this->connections_, false);

    this->themeChangedEvent();
    this->reload();
}

QSize HeaderPreview::sizeHint() const
{
    return {int(420 * this->scale()), this->headerHeight() + 2 * MARGIN +
                                          int(NOTE_HEIGHT * this->scale())};
}

QSize HeaderPreview::minimumSizeHint() const
{
    return {int(240 * this->scale()), this->sizeHint().height()};
}

const std::vector<HeaderPreview::Placed> &HeaderPreview::placed() const
{
    return this->placed_;
}

int HeaderPreview::headerHeight() const
{
    // As high as the header's buttons are wide - lower in Compact
    return int(uistyle::headerHeight() * this->scale());
}

QRect HeaderPreview::headerRect() const
{
    return {0, MARGIN, this->width(), this->headerHeight()};
}

QWidget *HeaderPreview::widgetFor(Part part) const
{
    switch (part)
    {
        case Part::Picture:
            return this->picture_;
        case Part::Cover:
            return this->cover_;
        case Part::Title:
            return this->title_;
        case Part::Activity:
            return this->activity_;
        case Part::Mode:
            return this->mode_;
        case Part::Moderation:
            return this->moderation_;
        case Part::Chatters:
            return this->chatters_;
        case Part::Tracker:
            return this->tracker_;
        case Part::Menu:
            return this->menu_;
        case Part::Add:
            return this->add_;
    }
    return nullptr;
}

void HeaderPreview::reload()
{
    if (this->movingPart_ || this->movingGrip_)
    {
        return;
    }
    this->order_ = headerparts::order();
    this->share_ = getSettings()->splitHeaderActivityShare;
    this->spacing_ = headerparts::spacing();
    this->deltas_.clear();
    std::vector<headerparts::Run> runs;
    const auto text = sampleTitle(&runs);
    this->title_->setRuns(runs);
    this->title_->setText(text);
    this->relayout();
    this->update();
}

void HeaderPreview::relayout()
{
    // Before everything is built there is nothing to place
    if (this->add_ == nullptr)
    {
        return;
    }
    const auto scale = this->scale();
    const auto header = this->headerRect();
    const int button = this->headerHeight();
    const int titleSpace = int(2 * scale);

    std::vector<Part> shown;
    for (const auto part : this->order_)
    {
        if (headerparts::isShown(part))
        {
            shown.push_back(part);
        }
    }

    // Everything but the title and the curve keeps its own width, as in
    // the header itself
    // Buttons -> Titelleiste: each part can be dragged wider or narrower
    const auto widened = [&](Part part, int usual) {
        return std::max(int(headerparts::LEAST_WIDTH * scale),
                        usual + int(this->deltaOf(part) * scale));
    };
    this->picture_->setExtraWidth(this->deltaOf(Part::Picture));
    this->cover_->setExtraWidth(this->deltaOf(Part::Cover));

    const auto fixedWidth = [&](Part part) {
        switch (part)
        {
            case Part::Picture:
                return this->picture_->width();
            case Part::Cover:
                return this->cover_->width();
            case Part::Title:
                return titleSpace;
            case Part::Activity:
                return 0;
            case Part::Mode:
                return this->mode_->sizeHint().width();
            case Part::Moderation:
            case Part::Chatters:
            case Part::Tracker:
            case Part::Menu:
                return widened(part, button);
            case Part::Add:
                return widened(part,
                               int((uistyle::compact() ? 13 : 16) * scale));
        }
        return 0;
    };
    this->activity_->setFixedHeight(button);
    int fixed = int(8 * scale);
    for (const auto part : shown)
    {
        fixed += fixedWidth(part);
    }
    // The room between the parts is taken from what the title and the curve
    // have to share - otherwise the last parts are pushed off the edge
    if (shown.size() > 1)
    {
        fixed += int(this->spacing_ * scale) * int(shown.size() - 1);
    }
    this->shared_ = std::max(header.width() - fixed, 0);

    int curve = 0;
    if (std::find(shown.begin(), shown.end(), Part::Activity) != shown.end())
    {
        const auto needed = int(
            std::ceil(getApp()
                          ->getFonts()
                          ->getFontMetrics(this->title_->getFontStyle(), scale)
                          .horizontalAdvance(this->title_->getText())));
        const auto own = this->activity_->ownWidth();
        auto wanted =
            headerparts::curveWidth(this->shared_, needed, own, this->share_,
                                    int(headerparts::TITLE_KEEPS * scale));
        if (wanted <= 0)
        {
            wanted = own;
        }
        curve = std::max(std::min(wanted, this->shared_),
                         this->activity_->minimumSizeHint().width());
    }
    const int title = std::max(this->shared_ - curve, 0);

    this->placed_.clear();
    int x = header.left() + int(8 * scale);
    for (const auto part : shown)
    {
        const int width = part == Part::Title      ? title
                          : part == Part::Activity ? curve
                                                   : fixedWidth(part);
        this->placed_.push_back(
            {part, QRect(x, header.top(), width, header.height())});
        x += width + (part == Part::Title ? titleSpace : 0) +
             int(this->spacing_ * scale);

        // Sized for painting; the pictures keep their own size
        auto *widget = this->widgetFor(part);
        if (part != Part::Picture && part != Part::Cover)
        {
            widget->resize(width, part == Part::Activity
                                      ? this->activity_->sizeHint().height()
                                      : header.height());
        }
    }
    this->takePicturesSoon();
}

void HeaderPreview::takePicturesSoon()
{
    if (this->picturesPending_)
    {
        return;
    }
    this->picturesPending_ = true;
    QTimer::singleShot(0, this, [this] {
        this->picturesPending_ = false;
        this->takePictures();
    });
}

void HeaderPreview::takePictures()
{
    // Only what can be seen is worth a picture; showing it takes them
    if (!this->isVisible() || this->takingPictures_)
    {
        return;
    }
    this->takingPictures_ = true;
    const auto placed = this->placed_;
    std::map<Part, QPixmap> pictures;
    for (const auto &part : placed)
    {
        auto *widget = this->widgetFor(part.part);
        if (widget->width() <= 0 || widget->height() <= 0)
        {
            continue;
        }
        // Without a background of its own, so the header shows through
        const auto ratio = this->devicePixelRatioF();
        QPixmap picture(widget->size() * ratio);
        picture.setDevicePixelRatio(ratio);
        picture.fill(Qt::transparent);
        widget->render(&picture, QPoint(), QRegion(), QWidget::DrawChildren);
        pictures[part.part] = picture;
    }
    this->pictures_ = std::move(pictures);
    this->takingPictures_ = false;
    this->update();
}

QRect HeaderPreview::grip() const
{
    const Placed *title = nullptr;
    const Placed *curve = nullptr;
    for (const auto &placed : this->placed_)
    {
        if (placed.part == Part::Title)
        {
            title = &placed;
        }
        if (placed.part == Part::Activity)
        {
            curve = &placed;
        }
    }
    if (title == nullptr || curve == nullptr)
    {
        return {};
    }

    // The edge that faces the title
    const bool titleFirst = title->rect.center().x() < curve->rect.center().x();
    const int edge = titleFirst ? curve->rect.left() : curve->rect.right() + 1;
    const int half = int(4 * this->scale());
    return {edge - half, curve->rect.top(), 2 * half, curve->rect.height()};
}

bool HeaderPreview::onGrip(QPoint pos) const
{
    return this->grip().contains(pos);
}

int HeaderPreview::shareAt(int x) const
{
    const auto grip = this->grip();
    if (grip.isEmpty() || this->shared_ <= 0)
    {
        return this->share_;
    }
    QRect curve;
    QRect title;
    for (const auto &placed : this->placed_)
    {
        if (placed.part == Part::Activity)
        {
            curve = placed.rect;
        }
        if (placed.part == Part::Title)
        {
            title = placed.rect;
        }
    }
    const bool titleFirst = title.center().x() < curve.center().x();
    const int width = titleFirst ? curve.right() + 1 - x : x - curve.left();
    return std::clamp(
        int(std::lround(100.0 * double(width) / double(this->shared_))),
        headerparts::LEAST_SHARE, headerparts::MOST_SHARE);
}

int HeaderPreview::deltaOf(Part part) const
{
    const auto found = this->deltas_.find(part);
    if (found != this->deltas_.end())
    {
        return found->second;
    }
    return headerparts::widthDelta(part);
}

std::optional<HeaderPreview::Edge> HeaderPreview::edgeAt(QPoint pos) const
{
    const int reach = int(4 * this->scale());
    for (size_t i = 0; i < this->placed_.size(); i++)
    {
        const auto &placed = this->placed_.at(i);
        if (!headerparts::canResize(placed.part))
        {
            continue;
        }
        if (std::abs(pos.x() - (placed.rect.right() + 1)) <= reach)
        {
            return Edge{.part = placed.part, .right = true};
        }
        // The room before it - the first part has nothing before it
        if (i > 0 && std::abs(pos.x() - placed.rect.left()) <= reach)
        {
            return Edge{.part = placed.part, .right = false};
        }
    }
    return std::nullopt;
}

QRect HeaderPreview::edgeRect(Edge edge) const
{
    for (const auto &placed : this->placed_)
    {
        if (placed.part != edge.part)
        {
            continue;
        }
        const int x = edge.right ? placed.rect.right() + 1 : placed.rect.left();
        const int half = int(3 * this->scale());
        return {x - half, placed.rect.top(), 2 * half, placed.rect.height()};
    }
    return {};
}

std::optional<Part> HeaderPreview::partAt(QPoint pos) const
{
    for (const auto &placed : this->placed_)
    {
        if (placed.rect.contains(pos))
        {
            return placed.part;
        }
    }
    return std::nullopt;
}

void HeaderPreview::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    const auto header = this->headerRect();
    const auto &colors = this->theme->splits.header;

    // As the header draws itself - without the frame in Flat
    painter.fillRect(header, colors.background);
    if (!uistyle::flat())
    {
        painter.setPen(colors.border);
        painter.drawRect(header.adjusted(0, 0, -1, -2));
        painter.fillRect(header.left(), header.bottom(), header.width(), 1,
                         colors.background);
    }

    const QColor accent(0, 171, 244);
    for (const auto &placed : this->placed_)
    {
        auto *widget = this->widgetFor(placed.part);
        auto at = placed.rect.topLeft();
        at.ry() += (placed.rect.height() - widget->height()) / 2;
        // A part just switched on has its picture a moment later
        if (auto it = this->pictures_.find(placed.part);
            it != this->pictures_.end())
        {
            painter.drawPixmap(at, it->second);
        }

        if (this->movingPart_ && this->pressed_ == placed.part)
        {
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(accent, 1.5));
            auto fill = accent;
            fill.setAlpha(40);
            painter.setBrush(fill);
            painter.drawRoundedRect(QRectF(placed.rect).adjusted(1, 2, -1, -2),
                                    3, 3);
            painter.setBrush(Qt::NoBrush);
            painter.setRenderHint(QPainter::Antialiasing, false);
        }
    }

    // The handle on an edge being taken hold of, or about to be
    const auto shownEdge = this->movingEdge_ ? this->movingEdge_
                                             : this->hoverEdge_;
    if (shownEdge)
    {
        const auto rect = this->edgeRect(*shownEdge);
        if (!rect.isEmpty())
        {
            painter.setPen(QPen(accent, 1));
            const int middle = rect.center().y();
            const int reach = int(5 * this->scale());
            const int x = rect.center().x();
            painter.drawLine(x, middle - reach, x, middle + reach);
            // Which way it goes: wider, or further apart
            const int arm = int(3 * this->scale());
            if (shownEdge->right)
            {
                painter.drawLine(x - arm, middle, x + arm, middle);
            }
            else
            {
                painter.drawLine(x - arm, middle - arm, x - arm, middle + arm);
                painter.drawLine(x + arm, middle - arm, x + arm, middle + arm);
            }
        }
    }

    // The handle on the curve's edge, a little brighter under the mouse
    const auto grip = this->grip();
    if (!grip.isEmpty())
    {
        auto color =
            this->hoverGrip_ || this->movingGrip_ ? accent : colors.text;
        if (!(this->hoverGrip_ || this->movingGrip_))
        {
            color.setAlpha(110);
        }
        painter.setPen(QPen(color, 1));
        const int middle = grip.center().y();
        const int reach = int(5 * this->scale());
        const int x = grip.center().x();
        painter.drawLine(x - 1, middle - reach, x - 1, middle + reach);
        painter.drawLine(x + 1, middle - reach, x + 1, middle + reach);
    }

    // What the curve is set to
    if (!grip.isEmpty())
    {
        auto dim = colors.text;
        dim.setAlpha(150);
        painter.setPen(dim);
        painter.setFont(
            getApp()->getFonts()->getFont(FontStyle::UiMedium, this->scale()));
        const QRect note(0, header.bottom() + MARGIN, this->width(),
                         int(NOTE_HEIGHT * this->scale()));
        painter.drawText(
            note, Qt::AlignLeft | Qt::AlignVCenter,
            this->share_ <= 0
                ? QStringLiteral("Breite der Kurve: automatisch - die Hälfte "
                                 "des Platzes, den der Titel frei lässt")
                : QStringLiteral("Breite der Kurve: %1 % des Platzes neben "
                                 "dem Titel")
                      .arg(this->share_));
    }
}

void HeaderPreview::resizeEvent(QResizeEvent * /*event*/)
{
    this->relayout();
}

void HeaderPreview::showEvent(QShowEvent * /*event*/)
{
    this->takePicturesSoon();
}

void HeaderPreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }
    this->pressedAt_ = event->pos();
    if (this->onGrip(event->pos()))
    {
        this->movingGrip_ = true;
        return;
    }
    if (const auto edge = this->edgeAt(event->pos()))
    {
        this->movingEdge_ = edge;
        this->edgeStart_ =
            edge->right ? this->deltaOf(edge->part) : this->spacing_;
        return;
    }
    this->pressed_ = this->partAt(event->pos());
}

void HeaderPreview::mouseMoveEvent(QMouseEvent *event)
{
    const auto pos = event->pos();

    if (this->movingGrip_)
    {
        this->share_ = this->shareAt(pos.x());
        this->relayout();
        this->update();
        return;
    }

    if (this->movingEdge_)
    {
        // How far the mouse went, in the pixels the settings keep
        const auto moved =
            int(std::lround(double(pos.x() - this->pressedAt_.x()) /
                            double(this->scale())));
        if (this->movingEdge_->right)
        {
            this->deltas_[this->movingEdge_->part] =
                std::clamp(this->edgeStart_ + moved, -headerparts::MOST_DELTA,
                           headerparts::MOST_DELTA);
        }
        else
        {
            this->spacing_ = std::clamp(this->edgeStart_ + moved, 0,
                                        headerparts::MOST_SPACING);
        }
        this->relayout();
        this->update();
        return;
    }

    if (this->pressed_ && !this->movingPart_ &&
        (pos - this->pressedAt_).manhattanLength() >=
            QApplication::startDragDistance())
    {
        this->movingPart_ = true;
        this->setCursor(Qt::ClosedHandCursor);
    }

    if (this->movingPart_)
    {
        // Among the others, the part goes behind each one whose middle the
        // mouse has passed
        const auto part = *this->pressed_;
        std::vector<Part> others;
        size_t index = 0;
        for (const auto &placed : this->placed_)
        {
            if (placed.part == part)
            {
                continue;
            }
            others.push_back(placed.part);
            if (placed.rect.center().x() < pos.x())
            {
                index++;
            }
        }

        // The same place in the whole order, the hidden parts included
        auto order = this->order_;
        order.erase(std::find(order.begin(), order.end(), part));
        auto at = order.begin();
        if (index == 0 && !others.empty())
        {
            at = std::find(order.begin(), order.end(), others.front());
        }
        else if (index > 0)
        {
            at = std::next(
                std::find(order.begin(), order.end(), others.at(index - 1)));
        }
        order.insert(at, part);

        if (order != this->order_)
        {
            this->order_ = order;
            this->relayout();
        }
        this->update();
        return;
    }

    const bool onGrip = this->onGrip(pos);
    if (onGrip != this->hoverGrip_)
    {
        this->hoverGrip_ = onGrip;
        this->update();
    }
    const auto edge = onGrip ? std::nullopt : this->edgeAt(pos);
    if (edge != this->hoverEdge_)
    {
        this->hoverEdge_ = edge;
        this->update();
    }
    if (onGrip || edge)
    {
        this->setCursor(Qt::SizeHorCursor);
    }
    else if (this->partAt(pos))
    {
        this->setCursor(Qt::OpenHandCursor);
    }
    else
    {
        this->unsetCursor();
    }
}

void HeaderPreview::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }

    const bool grip = this->movingGrip_;
    const bool part = this->movingPart_;
    const auto edge = this->movingEdge_;
    this->movingGrip_ = false;
    this->movingPart_ = false;
    this->movingEdge_.reset();
    this->pressed_.reset();

    // Only now do the settings hear of it - one value, not one per pixel
    if (edge)
    {
        if (edge->right)
        {
            headerparts::setWidthDelta(edge->part, this->deltaOf(edge->part));
        }
        else
        {
            headerparts::setSpacing(this->spacing_);
        }
    }
    if (grip)
    {
        getSettings()->splitHeaderActivityShare.setValue(this->share_);
    }
    if (part)
    {
        headerparts::setOrder(this->order_);
    }

    this->reload();
    this->mouseMoveEvent(event);
}

void HeaderPreview::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }

    // Back to half of what the title leaves free
    if (this->onGrip(event->pos()))
    {
        this->movingGrip_ = false;
        getSettings()->splitHeaderActivityShare.setValue(0);
        this->reload();
        return;
    }

    // An edge back to how wide the part is by itself, or no room at all
    if (const auto edge = this->edgeAt(event->pos()))
    {
        this->movingEdge_.reset();
        if (edge->right)
        {
            headerparts::setWidthDelta(edge->part, 0);
        }
        else
        {
            headerparts::setSpacing(0);
        }
        this->reload();
    }
}

void HeaderPreview::leaveEvent(QEvent * /*event*/)
{
    this->hoverGrip_ = false;
    this->hoverEdge_.reset();
    this->unsetCursor();
    this->update();
}

bool HeaderPreview::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        auto *help = static_cast<QHelpEvent *>(event);
        QString text;
        if (this->onGrip(help->pos()))
        {
            text = QStringLiteral("Ziehen: die Kurve breiter oder schmaler "
                                  "machen\nDoppelklick: wieder automatisch");
        }
        else if (const auto part = this->partAt(help->pos()))
        {
            text = headerparts::info(*part).name +
                   QStringLiteral(" - ziehen zum Verschieben");
        }

        if (text.isEmpty())
        {
            QToolTip::hideText();
        }
        else
        {
            QToolTip::showText(help->globalPos(), text, this);
        }
        return true;
    }
    return BaseWidget::event(event);
}

void HeaderPreview::scaleChangedEvent(float /*scale*/)
{
    this->updateGeometry();
    this->relayout();
}

void HeaderPreview::themeChangedEvent()
{
    QPalette palette;
    palette.setColor(QPalette::WindowText, this->theme->splits.header.text);
    this->title_->setPalette(palette);

    const auto background = this->theme->splits.header.background;
    this->add_->setOptions({
        .background = background,
        .backgroundHover = background,
    });
    this->takePicturesSoon();
    this->update();
}

}  // namespace chatterino
