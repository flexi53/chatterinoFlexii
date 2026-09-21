// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/BadgePicker.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "widgets/buttons/BadgeButton.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"

#include <QAbstractButton>
#include <QEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace chatterino {

namespace {

constexpr int COLUMNS = 6;
constexpr int TILE = 40;
constexpr int PICTURE = 28;
constexpr int WIDTH = 300;

/// One badge to choose, as a tile: its picture, a ring when it is worn
class BadgeTile : public QAbstractButton
{
public:
    BadgeTile(const webbadges::Badge &badge, bool worn, QColor hover,
              QColor accent, QWidget *parent)
        : QAbstractButton(parent)
        , worn_(worn)
        , hover_(std::move(hover))
        , accent_(std::move(accent))
    {
        this->setFixedSize(TILE, TILE);
        this->setCursor(Qt::PointingHandCursor);
        this->setAttribute(Qt::WA_Hover);
        this->setToolTip(badge.title.isEmpty() ? badge.setID : badge.title);

        const QPointer<BadgeTile> guard(this);
        webbadges::picture(badge.image, this, [guard](const QPixmap &picture) {
            if (!guard.isNull())
            {
                guard->picture_ = picture;
                guard->update();
            }
        });
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::HoverEnter ||
            event->type() == QEvent::HoverLeave)
        {
            this->update();
        }
        return QAbstractButton::event(event);
    }

    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const QRectF box = QRectF(this->rect()).adjusted(1, 1, -1, -1);
        if (this->underMouse() || this->worn_)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(this->hover_);
            painter.drawRoundedRect(box, 6, 6);
        }
        if (this->worn_)
        {
            painter.setPen(QPen(this->accent_, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(box.adjusted(1, 1, -1, -1), 5, 5);
        }

        if (!this->picture_.isNull())
        {
            QRectF target(0, 0, PICTURE, PICTURE);
            target.moveCenter(box.center());
            painter.drawPixmap(target, this->picture_,
                               QRectF(this->picture_.rect()));
        }
    }

private:
    bool worn_;
    QColor hover_;
    QColor accent_;
    QPixmap picture_;
};

/// The head of a part of the window, to fold it open and shut: an arrow,
/// the heading and where it counts, and at the right the badge worn, so it
/// says what is chosen even while folded
class SectionHeader : public QAbstractButton
{
public:
    SectionHeader(QString heading, QString where,
                  const std::optional<webbadges::Badge> &worn, bool open,
                  QColor text, QColor dim, QColor hover, QWidget *parent)
        : QAbstractButton(parent)
        , heading_(std::move(heading))
        , where_(std::move(where))
        , open_(open)
        , text_(std::move(text))
        , dim_(std::move(dim))
        , hover_(std::move(hover))
    {
        this->setFixedHeight(40);
        this->setCursor(Qt::PointingHandCursor);
        this->setAttribute(Qt::WA_Hover);
        this->setOpen(open);
        if (worn)
        {
            this->wornTitle_ = worn->title;
            const QPointer<SectionHeader> guard(this);
            webbadges::picture(worn->image, this,
                               [guard](const QPixmap &picture) {
                                   if (!guard.isNull())
                                   {
                                       guard->picture_ = picture;
                                       guard->update();
                                   }
                               });
        }
    }

    void setOpen(bool open)
    {
        this->open_ = open;
        this->setToolTip(open ? QStringLiteral("Einklappen")
                              : QStringLiteral("Aufklappen"));
        this->update();
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::HoverEnter ||
            event->type() == QEvent::HoverLeave)
        {
            this->update();
        }
        return QAbstractButton::event(event);
    }

    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const QRectF box(this->rect());
        if (this->underMouse())
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(this->hover_);
            painter.drawRoundedRect(box, 6, 6);
        }

        // The arrow: pointing right while folded, down while open
        const QPointF tip(12, box.center().y());
        QPainterPath arrow;
        if (this->open_)
        {
            arrow.moveTo(tip + QPointF(-4, -2));
            arrow.lineTo(tip + QPointF(0, 2));
            arrow.lineTo(tip + QPointF(4, -2));
        }
        else
        {
            arrow.moveTo(tip + QPointF(-2, -4));
            arrow.lineTo(tip + QPointF(2, 0));
            arrow.lineTo(tip + QPointF(-2, 4));
        }
        painter.setPen(
            QPen(this->dim_, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(arrow);

        // What is worn, at the right
        qreal right = box.right() - 8;
        if (!this->picture_.isNull())
        {
            const QRectF picture(right - 20, box.center().y() - 10, 20, 20);
            painter.drawPixmap(picture, this->picture_,
                               QRectF(this->picture_.rect()));
            right = picture.left() - 6;
        }
        const auto plain = this->font();
        QFontMetricsF plainMetrics(plain);
        const auto worn = plainMetrics.elidedText(this->wornTitle_.isEmpty()
                                                      ? QStringLiteral("keins")
                                                      : this->wornTitle_,
                                                  Qt::ElideRight, 110);
        const auto wornWidth = plainMetrics.horizontalAdvance(worn);
        painter.setFont(plain);
        painter.setPen(this->dim_);
        painter.drawText(
            QRectF(right - wornWidth, box.top(), wornWidth + 1, box.height()),
            Qt::AlignVCenter | Qt::AlignRight, worn);
        right -= wornWidth + 10;

        // The heading, and under it where it counts
        auto bold = plain;
        bold.setBold(true);
        const QRectF text(24, box.top() + 3, right - 24, box.height() - 6);
        painter.setFont(bold);
        painter.setPen(this->text_);
        painter.drawText(text, Qt::AlignLeft | Qt::AlignTop,
                         QFontMetricsF(bold).elidedText(
                             this->heading_, Qt::ElideRight, text.width()));
        auto small = plain;
        small.setPointSizeF(plain.pointSizeF() - 1);
        painter.setFont(small);
        painter.setPen(this->dim_);
        painter.drawText(text, Qt::AlignLeft | Qt::AlignBottom,
                         QFontMetricsF(small).elidedText(
                             this->where_, Qt::ElideRight, text.width()));
    }

private:
    QString heading_;
    QString where_;
    bool open_;
    QColor text_;
    QColor dim_;
    QColor hover_;
    QString wornTitle_;
    QPixmap picture_;
};

/// How your name looks in the chat: the badges worn here, then the name
class Preview : public QWidget
{
public:
    Preview(const std::optional<webbadges::Badge> &channel,
            const std::optional<webbadges::Badge> &global, QColor text,
            QColor background, QWidget *parent)
        : QWidget(parent)
        , text_(std::move(text))
        , background_(std::move(background))
    {
        this->setFixedHeight(34);
        auto account = getApp()->getAccounts()->twitch.getCurrent();
        this->name_ = account->getUserName();
        this->color_ = account->color();

        const QPointer<Preview> guard(this);
        for (const auto &badge : {channel, global})
        {
            if (!badge)
            {
                continue;
            }
            const auto index = this->pictures_.size();
            this->pictures_.append(QPixmap());
            webbadges::picture(badge->image, this,
                               [guard, index](const QPixmap &picture) {
                                   if (!guard.isNull())
                                   {
                                       guard->pictures_[index] = picture;
                                       guard->update();
                                   }
                               });
        }
    }

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(Qt::NoPen);
        painter.setBrush(this->background_);
        painter.drawRoundedRect(QRectF(this->rect()), 6, 6);

        qreal x = 10;
        const qreal middle = this->height() / 2.0;
        for (const auto &picture : this->pictures_)
        {
            if (!picture.isNull())
            {
                painter.drawPixmap(QRectF(x, middle - 9, 18, 18), picture,
                                   QRectF(picture.rect()));
            }
            x += 22;
        }

        auto font = this->font();
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(this->color_.isValid() ? this->color_ : this->text_);
        const QFontMetricsF metrics(font);
        painter.drawText(QPointF(x + 2, middle + (metrics.ascent() / 2) - 1),
                         this->name_);
        const auto after = x + 2 + metrics.horizontalAdvance(this->name_);

        auto plain = this->font();
        painter.setFont(plain);
        painter.setPen(this->text_);
        painter.drawText(
            QPointF(after, middle + (QFontMetricsF(plain).ascent() / 2) - 1),
            QStringLiteral(": Hallo Chat!"));
    }

private:
    QString name_;
    QColor color_;
    QColor text_;
    QColor background_;
    QVector<QPixmap> pictures_;
};

}  // namespace

BadgePicker::BadgePicker(const QString &channelName, const QString &channelId,
                         QWidget *anchor)
    : QWidget(anchor, Qt::Popup | Qt::FramelessWindowHint)
    , channelName_(channelName)
    , channelId_(channelId)
    , anchor_(anchor)
{
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setFixedWidth(WIDTH);

    const auto c = colors();
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 14, 16, 14);
    outer->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("Chat-Identität"));
    auto font = title->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() + 1);
    title->setFont(font);
    title->setStyleSheet(QStringLiteral("color: %1").arg(c.text.name()));
    outer->addWidget(title);

    this->content_ = new QVBoxLayout;
    this->content_->setContentsMargins(0, 0, 0, 0);
    this->content_->setSpacing(6);
    outer->addLayout(this->content_);

    this->status_ = new QLabel;
    this->status_->setWordWrap(true);
    this->status_->hide();
    outer->addWidget(this->status_);

    this->rebuild();

    if (anchor != nullptr)
    {
        auto *screen = QGuiApplication::screenAt(
            anchor->mapToGlobal(anchor->rect().center()));
        if (screen == nullptr)
        {
            screen = anchor->screen();
        }
        if (screen != nullptr)
        {
            this->setScreen(screen);
        }
    }
    this->place();
}

BadgePicker::Colors BadgePicker::colors()
{
    if (getTheme()->isLightTheme())
    {
        return {
            .background = QColor("#ffffff"),
            .border = QColor("#dedee3"),
            .text = QColor("#0e0e10"),
            .dim = QColor("#53535f"),
            .hover = QColor("#f0f0f4"),
            .accent = QColor("#9147ff"),
            .error = QColor("#d6283b"),
        };
    }
    return {
        .background = QColor("#18181b"),
        .border = QColor("#2f2f35"),
        .text = QColor("#efeff1"),
        .dim = QColor("#adadb8"),
        .hover = QColor("#26262c"),
        .accent = QColor("#a970ff"),
        .error = QColor("#ff6b7a"),
    };
}

void BadgePicker::setChoices(const webbadges::Choices &choices)
{
    this->choices_ = choices;
    this->rebuild();
    this->place();
}

void BadgePicker::rebuild()
{
    // Take out what was there, to lay it out anew. Gone from sight at once:
    // it is only deleted once the click that led here is over, and until
    // then it would lie under what comes in its place.
    while (auto *item = this->content_->takeAt(0))
    {
        if (auto *widget = item->widget())
        {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    const auto c = colors();
    const auto dimLabel = [&c](const QString &text) {
        auto *label = new QLabel(text);
        label->setWordWrap(true);
        label->setStyleSheet(QStringLiteral("color: %1").arg(c.dim.name()));
        return label;
    };

    if (!this->choices_)
    {
        this->content_->addWidget(dimLabel(QStringLiteral("Lade …")));
        return;
    }
    const auto &choices = *this->choices_;
    if (!choices.problem.isEmpty())
    {
        this->content_->addWidget(dimLabel(choices.problem));
        auto *settings =
            new QPushButton(QStringLiteral("Einstellungen öffnen"));
        QObject::connect(settings, &QPushButton::clicked, this, [this] {
            SettingsDialog::showDialog(this->anchor_.data());
            this->close();
        });
        this->content_->addWidget(settings);
        return;
    }

    this->content_->addWidget(new Preview(
        choices.channelWorn, choices.globalWorn, c.text, c.hover, nullptr));

    const auto section = [this, &c, &dimLabel](
                             const QString &heading, const QString &where,
                             const std::vector<webbadges::Badge> &badges,
                             const std::optional<webbadges::Badge> &worn,
                             bool global, BoolSetting &open) {
        auto *box = new QWidget;
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);

        // Folded shut it is only its head - what is worn is still there.
        // Folding only hides the tiles; nothing is built anew for it.
        auto *head = new SectionHeader(heading, where, worn, open.getValue(),
                                       c.text, c.dim, c.hover, box);
        layout->addWidget(head);
        auto *body = new QWidget(box);
        auto *bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(body);
        body->setVisible(open.getValue());
        QObject::connect(head, &QAbstractButton::clicked, this,
                         [this, &open, head, body] {
                             open.setValue(!open.getValue());
                             head->setOpen(open.getValue());
                             body->setVisible(open.getValue());
                             this->place();
                         });

        if (badges.empty())
        {
            bodyLayout->addWidget(
                dimLabel(QStringLiteral("Keins zur Auswahl.")));
            return box;
        }

        auto *grid = new QGridLayout;
        grid->setContentsMargins(4, 4, 0, 4);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(6);
        int index = 0;
        for (const auto &badge : badges)
        {
            auto *tile = new BadgeTile(badge, worn && *worn == badge, c.hover,
                                       c.accent, body);
            QObject::connect(tile, &QAbstractButton::clicked, this,
                             [this, badge, global] {
                                 this->wear(badge, global);
                             });
            grid->addWidget(tile, index / COLUMNS, index % COLUMNS);
            ++index;
        }
        grid->setColumnStretch(COLUMNS, 1);
        bodyLayout->addLayout(grid);
        return box;
    };

    auto *s = getSettings();
    this->content_->addWidget(section(
        QStringLiteral("Kanal-Abzeichen"),
        QStringLiteral("Nur in #%1").arg(this->channelName_), choices.channel,
        choices.channelWorn, false, s->badgePickerChannelOpen));
    this->content_->addWidget(section(
        QStringLiteral("Globale Abzeichen"), QStringLiteral("In allen Kanälen"),
        choices.global, choices.globalWorn, true, s->badgePickerGlobalOpen));
}

void BadgePicker::wear(const webbadges::Badge &badge, bool global)
{
    if (!this->choices_)
    {
        return;
    }

    // Shown as worn at once - Twitch takes a moment before it says so
    const auto before = *this->choices_;
    auto &worn =
        global ? this->choices_->globalWorn : this->choices_->channelWorn;
    if (worn && *worn == badge)
    {
        return;
    }
    worn = badge;
    this->rebuild();
    if (this->onWorn)
    {
        this->onWorn(this->choices_->shown());
    }

    const auto c = colors();
    this->status_->setStyleSheet(QStringLiteral("color: %1").arg(c.dim.name()));
    this->status_->setText(QStringLiteral("Wird gewechselt …"));
    this->status_->show();
    this->place();

    const QPointer<BadgePicker> guard(this);
    const auto title = badge.title;
    webbadges::choose(
        this->channelId_, badge, global, this,
        [guard, before, title, global](const QString &problem) {
            if (guard.isNull())
            {
                return;
            }
            const auto c = colors();
            if (problem.isEmpty())
            {
                guard->status_->setText(
                    global ? QStringLiteral("„%1“ trägst du jetzt in allen "
                                            "Kanälen.")
                                 .arg(title)
                           : QStringLiteral("„%1“ trägst du jetzt in #%2.")
                                 .arg(title, guard->channelName_));
                return;
            }

            // Back to what it was, and why
            guard->choices_ = before;
            guard->rebuild();
            if (guard->onWorn)
            {
                guard->onWorn(before.shown());
            }
            guard->status_->setStyleSheet(
                QStringLiteral("color: %1").arg(c.error.name()));
            guard->status_->setText(QStringLiteral("Nicht gewechselt: ") +
                                    problem);
            guard->place();
        });
}

void BadgePicker::place()
{
    // As high as what it holds now - smaller as well as larger, which
    // adjustSize alone does not always manage for a window
    // Every part measured afresh first: a part that just folded would
    // otherwise still count with its old height
    for (auto *child : this->findChildren<QWidget *>())
    {
        if (child->layout() != nullptr)
        {
            child->layout()->invalidate();
        }
        // Also forgets the size its parent keeps for it
        child->updateGeometry();
    }
    auto *layout = this->layout();
    layout->invalidate();
    layout->activate();
    const int height = layout->hasHeightForWidth()
                           ? layout->totalHeightForWidth(WIDTH)
                           : layout->totalSizeHint().height();
    this->resize(WIDTH, height);
    this->follow();
}

void BadgePicker::follow()
{
    if (this->anchor_.isNull())
    {
        return;
    }
    const QRect button(this->anchor_->mapToGlobal(QPoint(0, 0)),
                       this->anchor_->size());
    // The screen the button is seen on, found by where it is
    auto *screen = QGuiApplication::screenAt(button.center());
    if (screen == nullptr)
    {
        screen = this->anchor_->screen();
    }
    const auto area = screen != nullptr ? screen->availableGeometry() : button;
    // Placed by the size it has - it grows once the badges are there, and
    // has to move up with it rather than hang down past the button
    this->move(BadgeButton::placeMenu(button, this->size(), area));
}

void BadgePicker::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    this->follow();
}

void BadgePicker::paintEvent(QPaintEvent * /*event*/)
{
    // A card with rounded corners, as Twitch draws its own
    const auto c = colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(c.border, 1));
    painter.setBrush(c.background);
    painter.drawRoundedRect(QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            8, 8);
}

}  // namespace chatterino
