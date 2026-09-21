// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/BadgePicker.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Theme.hpp"
#include "widgets/buttons/BadgeButton.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"

#include <QAbstractButton>
#include <QEvent>
#include <QGridLayout>
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
    this->content_->setSpacing(10);
    outer->addLayout(this->content_);

    this->status_ = new QLabel;
    this->status_->setWordWrap(true);
    this->status_->hide();
    outer->addWidget(this->status_);

    this->rebuild();

    if (auto *screen = anchor != nullptr ? anchor->screen() : nullptr)
    {
        this->setScreen(screen);
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
    // Take out what was there, to lay it out anew
    while (auto *item = this->content_->takeAt(0))
    {
        if (auto *widget = item->widget())
        {
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
                             bool global) {
        auto *box = new QWidget;
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 4, 0, 0);
        layout->setSpacing(2);

        auto *title = new QLabel(heading);
        auto font = title->font();
        font.setBold(true);
        title->setFont(font);
        title->setStyleSheet(QStringLiteral("color: %1").arg(c.text.name()));
        layout->addWidget(title);
        layout->addWidget(dimLabel(where));

        if (badges.empty())
        {
            layout->addWidget(dimLabel(QStringLiteral("Keins zur Auswahl.")));
            return box;
        }

        auto *grid = new QGridLayout;
        grid->setContentsMargins(0, 6, 0, 0);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(6);
        int index = 0;
        for (const auto &badge : badges)
        {
            auto *tile = new BadgeTile(badge, worn && *worn == badge, c.hover,
                                       c.accent, box);
            QObject::connect(tile, &QAbstractButton::clicked, this,
                             [this, badge, global] {
                                 this->wear(badge, global);
                             });
            grid->addWidget(tile, index / COLUMNS, index % COLUMNS);
            ++index;
        }
        grid->setColumnStretch(COLUMNS, 1);
        layout->addLayout(grid);
        return box;
    };

    this->content_->addWidget(
        section(QStringLiteral("Kanal-Abzeichen"),
                QStringLiteral("Nur in #%1 - Sub, Bits, Mod und Co.")
                    .arg(this->channelName_),
                choices.channel, choices.channelWorn, false));
    this->content_->addWidget(
        section(QStringLiteral("Globale Abzeichen"),
                QStringLiteral("In allen Kanälen, neben dem Kanal-Abzeichen"),
                choices.global, choices.globalWorn, true));
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
    this->adjustSize();
    if (this->anchor_.isNull())
    {
        return;
    }
    const QRect button(this->anchor_->mapToGlobal(QPoint(0, 0)),
                       this->anchor_->size());
    const auto *screen = this->anchor_->screen();
    const auto area = screen != nullptr ? screen->availableGeometry() : button;
    this->move(BadgeButton::placeMenu(button, this->sizeHint(), area));
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
