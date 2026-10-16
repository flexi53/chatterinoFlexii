// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/TwitchActionsPopup.hpp"

#include "providers/twitch/ProfilePictures.hpp"
#include "providers/twitch/pubsubmessages/Polls.hpp"
#include "providers/twitch/pubsubmessages/Predictions.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchWebBadges.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/RoundPixmap.hpp"
#include "util/UiStyle.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/Label.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QShowEvent>
#include <QAction>
#include <QTimer>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSizeGrip>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <utility>

namespace chatterino {

namespace {

constexpr int AVATAR_SIZE = 44;
constexpr int TILE_WIDTH = 124;
constexpr int TILE_HEIGHT = 112;
constexpr int TILE_PICTURE = 36;
constexpr int TILE_COLUMNS = 3;
constexpr int MAX_POLL_CHOICES = 5;
constexpr int MAX_OUTCOMES = 10;

/// The pages under the section buttons, in the order they are added
enum Page {
    LoginPage = 0,
    RewardsPage = 1,
    PollPage = 2,
    PredictionPage = 3,
};

QString points(qint64 value)
{
    return QLocale(QLocale::German).toString(value);
}

QString rgba(const QColor &color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

/// The colour Twitch gives its points, a touch deeper on a light theme
QColor pointsColor()
{
    return getTheme()->isLightTheme() ? QColor("#7a3fd1") : QColor("#bf94ff");
}

/// Twitch's coin, as the points beside the input draw it
void drawCoin(QPainter &painter, const QRectF &box, const QColor &color)
{
    QPen pen(color);
    pen.setWidthF(std::max(1.2, box.width() * 0.12));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const auto inset = box.width() * 0.08;
    const auto ring = box.adjusted(inset, inset, -inset, -inset);
    painter.drawArc(ring, 120 * 16, 300 * 16);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const auto dot = ring.width() * 0.16;
    painter.drawEllipse(ring.center(), dot, dot);
}

/// One reward as Twitch shows it: a tile in the reward's own colour with
/// its picture, its name and what it costs. Pale where it cannot be had
/// now.
class RewardTile : public QWidget
{
public:
    RewardTile(modgql::Reward reward, bool affordable, QWidget *parent)
        : QWidget(parent)
        , reward_(std::move(reward))
        , usable_(this->reward_.enabled && this->reward_.inStock &&
                  affordable)
    {
        this->setFixedSize(TILE_WIDTH, TILE_HEIGHT);
        this->setCursor(this->usable_ ? Qt::PointingHandCursor
                                      : Qt::ArrowCursor);

        QString tip = this->reward_.title;
        if (!this->reward_.prompt.trimmed().isEmpty())
        {
            tip += QStringLiteral("\n\n") + this->reward_.prompt.trimmed();
        }
        if (!this->reward_.enabled)
        {
            tip += QStringLiteral("\n\nGerade pausiert.");
        }
        else if (!this->reward_.inStock)
        {
            tip += QStringLiteral("\n\nGerade nicht verfügbar.");
        }
        else if (!affordable)
        {
            tip += QStringLiteral("\n\nDafür reichen die Punkte nicht.");
        }
        this->setToolTip(tip);

        if (!this->reward_.imageUrl.isEmpty())
        {
            QPointer<RewardTile> self(this);
            webbadges::picture(this->reward_.imageUrl, this,
                               [self](const QPixmap &picture) {
                                   if (self)
                                   {
                                       self->picture_ = picture;
                                       self->update();
                                   }
                               });
        }
    }

    std::function<void()> onClick;

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        if (!this->usable_)
        {
            painter.setOpacity(0.42);
        }

        QColor fill(this->reward_.color);
        if (!fill.isValid())
        {
            fill = QColor("#9147ff");
        }
        if (this->hovered_ && this->usable_)
        {
            fill = fill.lighter(112);
        }
        const QRectF whole = QRectF(this->rect()).adjusted(1, 1, -1, -1);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(whole, 8, 8);

        // Writing that reads on the reward's colour, light or dark
        const auto ink =
            fill.lightnessF() > 0.62 ? QColor("#18181b") : QColor(Qt::white);

        const QRectF pictureBox((whole.width() - TILE_PICTURE) / 2, 10,
                                TILE_PICTURE, TILE_PICTURE);
        if (!this->picture_.isNull())
        {
            painter.drawPixmap(pictureBox, this->picture_,
                               QRectF(this->picture_.rect()));
        }
        else
        {
            drawCoin(painter, pictureBox.adjusted(6, 6, -6, -6), ink);
        }

        auto font = this->font();
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(ink);
        const QRectF titleBox(6, 50, whole.width() - 12, 34);
        painter.drawText(titleBox,
                         Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                         this->reward_.title);

        // What it costs, on a darker strip at the bottom
        const auto cost = points(this->reward_.cost);
        font.setBold(false);
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const qreal coinSide = metrics.height() * 0.8;
        const qreal width = coinSide + 4 + metrics.horizontalAdvance(cost) + 14;
        const QRectF pill((whole.width() - width) / 2, whole.bottom() - 24,
                          width, 18);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 70));
        painter.drawRoundedRect(pill, 9, 9);
        const QRectF coin(pill.left() + 7, pill.center().y() - coinSide / 2,
                          coinSide, coinSide);
        drawCoin(painter, coin, Qt::white);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(coin.right() + 4, pill.top(),
                                pill.right() - coin.right() - 4, pill.height()),
                         Qt::AlignVCenter | Qt::AlignLeft, cost);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && this->usable_ &&
            this->onClick && this->rect().contains(event->position().toPoint()))
        {
            auto onClick = this->onClick;
            onClick();
        }
    }

    void enterEvent(QEnterEvent * /*event*/) override
    {
        this->hovered_ = true;
        this->update();
    }

    void leaveEvent(QEvent * /*event*/) override
    {
        this->hovered_ = false;
        this->update();
    }

private:
    modgql::Reward reward_;
    bool usable_ = true;
    bool hovered_ = false;
    QPixmap picture_;
};

QLabel *mutedLabel(const QString &text = {})
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setStyleSheet(
        QStringLiteral("color: %1;")
            .arg(getTheme()->messages.textColors.system.name()));
    return label;
}

}  // namespace

TwitchActionsPopup::TwitchActionsPopup(
    const std::shared_ptr<TwitchChannel> &channel, QWidget *parent)
    : BasePopup(
          {
              BaseWindow::EnableCustomFrame,
              BaseWindow::DisableLayoutSave,
              BaseWindow::BoundsCheckOnShow,
          },
          parent)
    , channel_(channel)
    , channelLogin_(channel->getName().toLower())
    , channelId_(channel->roomId())
    , moderates_(channel->isMod() || channel->isBroadcaster())
{
    this->setWindowTitle(
        QStringLiteral("Kanalpunkte – #%1").arg(this->channelLogin_));
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setMinimumWidth(TILE_WIDTH * TILE_COLUMNS + 80);

    auto *layout = new QVBoxLayout(this->getLayoutContainer());
    // Look -> Style: Compact keeps less room, as the alert windows do
    const auto margin = uistyle::alertMargin();
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(uistyle::compact() ? 5 : 8);

    this->buildHead(layout);

    // For moderators the window does more than points - what it shows is
    // picked here, in the same quiet buttons the rest uses
    this->sections_ = new QWidget;
    {
        auto *row = new QHBoxLayout(this->sections_);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        this->sectionGroup_ = new QButtonGroup(this);
        this->sectionGroup_->setExclusive(true);
        for (const auto &[text, page] :
             std::initializer_list<std::pair<const char *, int>>{
                 {"Belohnungen", RewardsPage},
                 {"Umfrage starten", PollPage},
                 {"Wette starten", PredictionPage}})
        {
            auto *button = new QPushButton(QString::fromUtf8(text));
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            this->quietButtons_.push_back(button);
            this->sectionGroup_->addButton(button, page);
            row->addWidget(button);
        }
        row->addStretch(1);
        this->sectionGroup_->button(RewardsPage)->setChecked(true);
        QObject::connect(this->sectionGroup_, &QButtonGroup::idClicked, this,
                         [this](int page) {
                             this->pages_->setCurrentIndex(page);
                             this->say({});
                         });
    }
    this->sections_->setVisible(this->moderates_);
    layout->addWidget(this->sections_);

    this->pages_ = new QStackedWidget;
    this->loginPage_ = this->buildLoginPage();
    this->pages_->addWidget(this->loginPage_);
    this->rewardsPage_ = this->buildRewardsPage();
    this->pages_->addWidget(this->rewardsPage_);
    this->pages_->addWidget(this->moderates_ ? this->buildPollPage()
                                             : new QWidget);
    this->pages_->addWidget(this->moderates_ ? this->buildPredictionPage()
                                             : new QWidget);
    layout->addWidget(this->pages_, 1);

    auto *bottom = new QHBoxLayout;
    this->status_ = new QLabel;
    this->status_->setWordWrap(true);
    bottom->addWidget(this->status_, 1);
    // Something to grab, since the window's size is meant to be changed
    bottom->addWidget(new QSizeGrip(this), 0, Qt::AlignBottom | Qt::AlignRight);
    layout->addLayout(bottom);

    // What is running comes from the channel itself, as the banner shows it
    this->connections_.managedConnect(channel->pollChanged, [this] {
        this->showState();
    });
    this->connections_.managedConnect(channel->predictionChanged, [this] {
        this->showState();
    });
    this->connections_.managedConnect(modgql::loginChanged(), [this] {
        this->loggedIn_ = modgql::currentLogin().has_value();
        this->showState();
        this->refreshPoints();
    });

    this->applyStyles();

    // The size a viewer last dragged it to
    const auto width = getSettings()->twitchExtrasWidth.getValue();
    const auto height = getSettings()->twitchExtrasHeight.getValue();
    if (width > 0 && height > 0)
    {
        this->resize(width, height);
    }
    else
    {
        this->resize(this->minimumWidth(), 460);
    }

    QPointer<TwitchActionsPopup> self(this);
    modgql::whenReady(this, [self](bool loggedIn) {
        if (!self)
        {
            return;
        }
        self->loggedIn_ = loggedIn;
        self->showState();
        self->refreshPoints();
    });
    this->showState();
}

void TwitchActionsPopup::placeAbove(const QWidget *anchor)
{
    // Where it was last moved to, as the alert windows do - bounds checked,
    // so a place on a screen since unplugged still ends up on one
    if (getSettings()->twitchExtrasPositionSaved)
    {
        this->moveTo(QPoint(getSettings()->twitchExtrasX.getValue(),
                            getSettings()->twitchExtrasY.getValue()),
                     widgets::BoundsChecking::DesiredPosition);
        return;
    }
    if (anchor == nullptr)
    {
        return;
    }
    const auto corner = anchor->mapToGlobal(QPoint(anchor->width(), 0));
    this->moveTo(QPoint(corner.x() - this->width(),
                        corner.y() - this->height() - 6),
                 widgets::BoundsChecking::DesiredPosition);
}

void TwitchActionsPopup::showEvent(QShowEvent *event)
{
    BasePopup::showEvent(event);
    // What it came up with, so a size or place given to it by hand can be
    // told apart - the first time only
    if (!this->shownSize_.isValid())
    {
        this->shownSize_ = this->size();
        this->shownPos_ = this->pos();
    }
}

void TwitchActionsPopup::buildHead(QVBoxLayout *layout)
{
    // The channel, laid out like the top of a user card, with the balance
    // where the alert windows have their headline
    auto *head = new QHBoxLayout;
    head->setSpacing(12);

    this->avatar_ = new QLabel;
    this->avatar_->setFixedSize(AVATAR_SIZE, AVATAR_SIZE);
    head->addWidget(this->avatar_, 0, Qt::AlignTop);
    {
        const auto ratio = this->devicePixelRatioF();
        const auto side = qRound(AVATAR_SIZE * ratio);
        QPointer<QLabel> avatar(this->avatar_);
        profilepictures::pixmap(this->channelLogin_, side, this,
                                [avatar, side, ratio](const QPixmap &picture) {
                                    if (!avatar)
                                    {
                                        return;
                                    }
                                    auto round = roundPixmap(picture, side);
                                    round.setDevicePixelRatio(ratio);
                                    avatar->setPixmap(round);
                                });
    }

    auto *who = new QVBoxLayout;
    who->setSpacing(2);
    this->name_ = new Label(profilepictures::displayName(this->channelLogin_));
    this->name_->setFontStyle(FontStyle::UiMediumBold);
    this->name_->setPadding(QMargins(0, 0, 0, 0));
    who->addWidget(this->name_);
    this->details_ = mutedLabel(
        this->moderates_
            ? QStringLiteral("#%1 · du bist hier Mod").arg(this->channelLogin_)
            : QStringLiteral("#%1").arg(this->channelLogin_));
    who->addWidget(this->details_);
    who->addStretch(1);
    head->addLayout(who, 1);
    {
        QPointer<Label> name(this->name_);
        profilepictures::whenKnown(this->channelLogin_, this,
                                   [name](const TwitchProfile &profile) {
                                       if (name)
                                       {
                                           name->setText(profile.displayName);
                                       }
                                   });
    }

    auto *balance = new QVBoxLayout;
    balance->setSpacing(0);
    this->balanceLabel_ = new QLabel;
    auto font = this->balanceLabel_->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.45);
    this->balanceLabel_->setFont(font);
    this->balanceLabel_->setAlignment(Qt::AlignRight);
    balance->addWidget(this->balanceLabel_);
    this->balanceCaption_ = mutedLabel(QStringLiteral("Kanalpunkte"));
    this->balanceCaption_->setWordWrap(false);
    this->balanceCaption_->setAlignment(Qt::AlignRight);
    balance->addWidget(this->balanceCaption_);
    balance->addStretch(1);
    head->addLayout(balance);

    layout->addLayout(head);
}

QWidget *TwitchActionsPopup::buildLoginPage()
{
    // Until the login is there, the window shows only that, rather than
    // tiles and forms that look as if they should work
    auto *page = new QWidget;
    auto *box = new QVBoxLayout(page);
    box->setContentsMargins(16, 16, 16, 16);
    box->addStretch(1);
    auto *title = new QLabel(QStringLiteral("Noch nicht angemeldet"));
    auto font = title->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.1);
    title->setFont(font);
    box->addWidget(title, 0, Qt::AlignHCenter);
    auto *text = mutedLabel(QStringLiteral(
        "Kanalpunkte, Umfragen und Wetten laufen über eine eigene "
        "Anmeldung als Twitch-App. Die machst du einmal unter "
        "Einstellungen → Twitch-Extras: Code holen, auf twitch.tv/activate "
        "eingeben, fertig. Danach geht es hier von selbst weiter."));
    text->setAlignment(Qt::AlignHCenter);
    box->addWidget(text);
    box->addSpacing(8);
    auto *open = new QPushButton(QStringLiteral("Jetzt anmelden …"));
    open->setCursor(Qt::PointingHandCursor);
    this->accentButtons_.push_back(open);
    QObject::connect(open, &QPushButton::clicked, this, [this] {
        SettingsDialog::showDialog(this, SettingsDialogPreference::TwitchExtras);
    });
    box->addWidget(open, 0, Qt::AlignHCenter);
    box->addStretch(1);
    return page;
}

QWidget *TwitchActionsPopup::buildRewardsPage()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // The window's own background through it, not a white box
    scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget "
                                         "> QWidget { background: "
                                         "transparent; }"));

    auto *inner = new QWidget;
    auto *box = new QVBoxLayout(inner);
    box->setContentsMargins(0, 0, 0, 0);
    this->rewardsEmpty_ = mutedLabel();
    this->rewardsEmpty_->hide();
    box->addWidget(this->rewardsEmpty_);
    this->rewardGrid_ = new QGridLayout;
    this->rewardGrid_->setSpacing(8);
    this->rewardGrid_->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    box->addLayout(this->rewardGrid_);
    box->addStretch(1);
    scroll->setWidget(inner);
    return scroll;
}

QVBoxLayout *TwitchActionsPopup::addCard(QVBoxLayout *layout,
                                         const QString &title, QWidget **card)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    this->cards_.push_back(frame);
    auto *inside = new QVBoxLayout(frame);
    inside->setContentsMargins(12, 10, 12, 12);
    inside->setSpacing(6);
    auto *heading = new QLabel(title);
    auto font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    inside->addWidget(heading);
    layout->addWidget(frame);
    if (card != nullptr)
    {
        *card = frame;
    }
    return inside;
}

QButtonGroup *TwitchActionsPopup::addChips(
    QVBoxLayout *layout,
    std::initializer_list<std::pair<const char *, int>> choices, int picked)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(4);
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    for (const auto &[text, seconds] : choices)
    {
        auto *chip = new QPushButton(QString::fromUtf8(text));
        chip->setCheckable(true);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setChecked(seconds == picked);
        this->quietButtons_.push_back(chip);
        group->addButton(chip, seconds);
        row->addWidget(chip);
    }
    row->addStretch(1);
    layout->addLayout(row);
    return group;
}

QWidget *TwitchActionsPopup::buildPollPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    this->pollRunning_ = mutedLabel(QStringLiteral(
        "Gerade läuft eine Umfrage - Stand, Abstimmen und Beenden findest du "
        "im Banner über dem Chat. Ist sie vorbei, kannst du hier die nächste "
        "starten."));
    layout->addWidget(this->pollRunning_);

    auto *card = this->addCard(layout, QStringLiteral("Neue Umfrage"),
                               &this->pollCard_);
    this->pollTitle_ = new QLineEdit;
    this->pollTitle_->setMaxLength(60);
    this->pollTitle_->setPlaceholderText(QStringLiteral("Frage"));
    card->addWidget(this->pollTitle_);
    for (int i = 0; i < MAX_POLL_CHOICES; i++)
    {
        auto *choice = new QLineEdit;
        choice->setMaxLength(25);
        choice->setPlaceholderText(
            i < 2 ? QStringLiteral("Antwort %1").arg(i + 1)
                  : QStringLiteral("Antwort %1 (frei lassen, wenn nicht "
                                   "gebraucht)")
                        .arg(i + 1));
        this->pollChoices_.push_back(choice);
        card->addWidget(choice);
    }

    card->addWidget(mutedLabel(QStringLiteral("Dauer")));
    this->pollDuration_ = this->addChips(card,
                                         {{"30 s", 30},
                                          {"1 min", 60},
                                          {"2 min", 120},
                                          {"3 min", 180},
                                          {"5 min", 300},
                                          {"10 min", 600}},
                                         120);

    auto *pointsRow = new QHBoxLayout;
    this->pollPointsOn_ =
        new QCheckBox(QStringLiteral("Weitere Stimmen für Kanalpunkte"));
    this->pollPoints_ = new QSpinBox;
    this->pollPoints_->setRange(1, 1000000);
    this->pollPoints_->setValue(100);
    this->pollPoints_->setSuffix(QStringLiteral(" Punkte"));
    this->pollPoints_->setEnabled(false);
    QObject::connect(this->pollPointsOn_, &QCheckBox::toggled,
                     this->pollPoints_, &QSpinBox::setEnabled);
    pointsRow->addWidget(this->pollPointsOn_);
    pointsRow->addWidget(this->pollPoints_);
    pointsRow->addStretch(1);
    card->addLayout(pointsRow);

    auto *start = new QPushButton(QStringLiteral("Umfrage starten"));
    start->setCursor(Qt::PointingHandCursor);
    this->accentButtons_.push_back(start);
    QObject::connect(start, &QPushButton::clicked, this, [this] {
        this->createPoll();
    });
    card->addSpacing(4);
    card->addWidget(start, 0, Qt::AlignRight);

    layout->addStretch(1);
    return page;
}

QWidget *TwitchActionsPopup::buildPredictionPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    this->predictionRunning_ = mutedLabel(QStringLiteral(
        "Gerade läuft eine Wette - Sperren, Abbrechen und den Gewinner "
        "wählen geht im Banner über dem Chat. Ist sie entschieden, kannst du "
        "hier die nächste starten."));
    layout->addWidget(this->predictionRunning_);

    auto *card = this->addCard(layout, QStringLiteral("Neue Wette"),
                               &this->predictionCard_);
    this->predictionTitle_ = new QLineEdit;
    this->predictionTitle_->setMaxLength(45);
    this->predictionTitle_->setPlaceholderText(
        QStringLiteral("Was wird passieren?"));
    card->addWidget(this->predictionTitle_);

    this->predictionOutcomeBox_ = new QVBoxLayout;
    this->predictionOutcomeBox_->setSpacing(6);
    card->addLayout(this->predictionOutcomeBox_);
    const auto addOutcome = [this] {
        auto *outcome = new QLineEdit;
        outcome->setMaxLength(25);
        outcome->setPlaceholderText(
            QStringLiteral("Ausgang %1").arg(this->predictionOutcomes_.size() +
                                             1));
        this->predictionOutcomes_.push_back(outcome);
        this->predictionOutcomeBox_->addWidget(outcome);
        this->addOutcome_->setVisible(
            static_cast<int>(this->predictionOutcomes_.size()) < MAX_OUTCOMES);
    };
    this->addOutcome_ = new QPushButton(QStringLiteral("+ Ausgang"));
    this->addOutcome_->setCursor(Qt::PointingHandCursor);
    this->quietButtons_.push_back(this->addOutcome_);
    QObject::connect(this->addOutcome_, &QPushButton::clicked, this,
                     addOutcome);
    addOutcome();
    addOutcome();
    card->addWidget(this->addOutcome_, 0, Qt::AlignLeft);

    card->addWidget(mutedLabel(QStringLiteral("Tippen offen für")));
    this->predictionWindow_ = this->addChips(card,
                                             {{"30 s", 30},
                                              {"1 min", 60},
                                              {"2 min", 120},
                                              {"5 min", 300},
                                              {"10 min", 600},
                                              {"30 min", 1800}},
                                             120);

    auto *start = new QPushButton(QStringLiteral("Wette starten"));
    start->setCursor(Qt::PointingHandCursor);
    this->accentButtons_.push_back(start);
    QObject::connect(start, &QPushButton::clicked, this, [this] {
        this->createPrediction();
    });
    card->addSpacing(4);
    card->addWidget(start, 0, Qt::AlignRight);

    layout->addStretch(1);
    return page;
}

void TwitchActionsPopup::applyStyles()
{
    const auto *theme = getTheme();
    const auto quiet = uistyle::quietButtonStyle();
    for (auto *button : this->quietButtons_)
    {
        button->setStyleSheet(quiet);
    }
    const auto accent = theme->accent;
    for (auto *button : this->accentButtons_)
    {
        button->setStyleSheet(uistyle::accentButtonStyle(accent));
        auto font = button->font();
        font.setBold(true);
        button->setFont(font);
        auto *glow = new QGraphicsDropShadowEffect(button);
        glow->setOffset(0, 0);
        glow->setBlurRadius(14);
        glow->setColor(accent);
        button->setGraphicsEffect(glow);
    }

    // A box like the alert windows' reason box, only quieter - it holds a
    // form, not a warning
    const auto light = theme->isLightTheme();
    const auto line = light ? QColor(0, 0, 0, 40) : QColor(255, 255, 255, 30);
    const auto fill = light ? QColor(0, 0, 0, 10) : QColor(255, 255, 255, 10);
    for (auto *card : this->cards_)
    {
        card->setStyleSheet(
            QStringLiteral("QFrame#card { background: %1; border: 1px solid "
                           "%2; border-left: 4px solid %3; border-radius: "
                           "6px; }")
                .arg(rgba(fill), rgba(line), accent.name()));
    }

    this->balanceLabel_->setStyleSheet(
        QStringLiteral("color: %1;").arg(pointsColor().name()));
    const auto muted =
        QStringLiteral("color: %1;").arg(theme->messages.textColors.system.name());
    this->details_->setStyleSheet(muted);
    this->balanceCaption_->setStyleSheet(muted);
}

void TwitchActionsPopup::themeChangedEvent()
{
    BasePopup::themeChangedEvent();
    if (this->balanceLabel_ != nullptr)
    {
        this->applyStyles();
    }
}

void TwitchActionsPopup::hideEvent(QHideEvent *event)
{
    // Kept only where it was changed by hand, as the alert windows do
    if (this->shownSize_.isValid() && this->size() != this->shownSize_)
    {
        getSettings()->twitchExtrasWidth.setValue(this->width());
        getSettings()->twitchExtrasHeight.setValue(this->height());
    }
    if (this->shownSize_.isValid() && this->pos() != this->shownPos_)
    {
        getSettings()->twitchExtrasX.setValue(this->x());
        getSettings()->twitchExtrasY.setValue(this->y());
        getSettings()->twitchExtrasPositionSaved.setValue(true);
    }
    BasePopup::hideEvent(event);
}

void TwitchActionsPopup::showState()
{
    this->sections_->setVisible(this->moderates_ && this->loggedIn_);
    if (!this->loggedIn_)
    {
        this->pages_->setCurrentIndex(LoginPage);
        this->balanceLabel_->clear();
        this->balanceCaption_->hide();
        return;
    }
    this->balanceCaption_->show();
    if (this->pages_->currentIndex() == LoginPage)
    {
        this->pages_->setCurrentIndex(this->sectionGroup_->checkedId() > 0
                                          ? this->sectionGroup_->checkedId()
                                          : RewardsPage);
    }

    if (!this->moderates_)
    {
        return;
    }
    const auto channel = this->channel_.lock();
    const auto *poll = channel ? channel->currentPoll() : nullptr;
    const auto *prediction = channel ? channel->currentPrediction() : nullptr;

    const bool pollRuns = poll != nullptr && poll->running();
    this->pollRunning_->setVisible(pollRuns);
    this->pollCard_->setEnabled(!pollRuns);

    // A locked one still waits for its result, so it is not over yet
    const bool predictionRuns =
        prediction != nullptr &&
        (prediction->status == PubSubPrediction::Status::Active ||
         prediction->status == PubSubPrediction::Status::Locked);
    this->predictionRunning_->setVisible(predictionRuns);
    this->predictionCard_->setEnabled(!predictionRuns);
}

void TwitchActionsPopup::refreshPoints()
{
    if (!this->loggedIn_)
    {
        return;
    }
    QPointer<TwitchActionsPopup> self(this);
    modgql::channelPoints(
        this->channelLogin_,
        [self](const modgql::ChannelPoints &channelPoints) {
            if (self)
            {
                self->showPoints(channelPoints);
            }
        },
        [self](const QString &error) {
            if (!self)
            {
                return;
            }
            self->balanceLabel_->clear();
            self->rewardsEmpty_->setText(error);
            self->rewardsEmpty_->show();
        });
}

void TwitchActionsPopup::showPoints(const modgql::ChannelPoints &channelPoints)
{
    if (this->channelId_.isEmpty())
    {
        this->channelId_ = channelPoints.channelId;
    }
    this->balance_ = channelPoints.balance;
    this->balanceLabel_->setText(points(channelPoints.balance));

    while (auto *item = this->rewardGrid_->takeAt(0))
    {
        if (auto *widget = item->widget())
        {
            widget->deleteLater();
        }
        delete item;
    }

    this->rewardsEmpty_->setVisible(channelPoints.rewards.empty());
    this->rewardsEmpty_->setText(
        QStringLiteral("Dieser Kanal hat keine eigenen Belohnungen."));

    int index = 0;
    for (const auto &reward : channelPoints.rewards)
    {
        auto *tile = new RewardTile(
            reward, channelPoints.balance >= reward.cost, nullptr);
        tile->onClick = [this, reward] {
            this->redeem(reward);
        };
        this->rewardGrid_->addWidget(tile, index / TILE_COLUMNS,
                                     index % TILE_COLUMNS);
        index++;
    }
}

void TwitchActionsPopup::redeem(const modgql::Reward &reward)
{
    // Twitch's own "choose an emote": the channel's sub emotes to pick from
    if (reward.type == QStringLiteral("CHOSEN_SUB_EMOTE_UNLOCK"))
    {
        this->pickEmote(reward);
        return;
    }

    QString text;
    if (reward.needsText)
    {
        bool ok = false;
        text = QInputDialog::getText(
            this, reward.title,
            reward.prompt.trimmed().isEmpty() ? QStringLiteral("Dein Text dazu")
                                              : reward.prompt.trimmed(),
            QLineEdit::Normal, {}, &ok);
        if (!ok)
        {
            return;
        }
        if (text.trimmed().isEmpty())
        {
            this->say(QStringLiteral("Die Belohnung braucht einen Text."),
                      true);
            return;
        }
    }
    else
    {
        // One click more, as on twitch.tv - points spent are gone
        QMenu menu(this);
        menu.addAction(QStringLiteral("„%1“ für %2 Punkte einlösen")
                           .arg(reward.title, points(reward.cost)));
        menu.addAction(QStringLiteral("Doch nicht"))->setData(false);
        auto *picked = menu.exec(QCursor::pos());
        if (picked == nullptr || picked->data().isValid())
        {
            return;
        }
    }

    this->sendRedeem(reward, text, {});
}

void TwitchActionsPopup::pickEmote(const modgql::Reward &reward)
{
    this->say(QStringLiteral("Hole die Emotes des Kanals …"));
    QPointer<TwitchActionsPopup> self(this);
    modgql::subEmotes(
        this->channelId_,
        [self, reward](const std::vector<modgql::SubEmote> &emotes) {
            if (!self)
            {
                return;
            }
            if (emotes.empty())
            {
                self->say(QStringLiteral("Der Kanal hat keine Sub-Emotes."),
                          true);
                return;
            }
            self->say({});

            // The pictures first, so the list shows what is picked - it
            // opens after a moment even where some do not come
            auto menu = std::make_shared<QMenu>(self.data());
            menu->addSection(QStringLiteral("Für %1 Punkte freischalten")
                                 .arg(points(reward.cost)));
            auto waiting = std::make_shared<int>(static_cast<int>(emotes.size()));
            auto opened = std::make_shared<bool>(false);
            const auto open = [self, menu, opened, reward] {
                if (!self || *opened)
                {
                    return;
                }
                *opened = true;
                auto *picked = menu->exec(QCursor::pos());
                if (picked != nullptr && picked->data().isValid())
                {
                    self->sendRedeem(reward, {}, picked->data().toString());
                }
            };
            for (const auto &emote : emotes)
            {
                auto *action = menu->addAction(emote.token);
                action->setData(emote.id);
                QPointer<QAction> guarded(action);
                webbadges::picture(
                    QStringLiteral("https://static-cdn.jtvnw.net/emoticons/"
                                   "v2/%1/default/dark/2.0")
                        .arg(emote.id),
                    self.data(),
                    [guarded, waiting, open](const QPixmap &picture) {
                        if (guarded)
                        {
                            guarded->setIcon(QIcon(picture));
                        }
                        if (--*waiting <= 0)
                        {
                            open();
                        }
                    });
            }
            QTimer::singleShot(1500, self.data(), open);
        },
        [self](const QString &error) {
            if (self)
            {
                self->say(error, true);
            }
        });
}

void TwitchActionsPopup::sendRedeem(const modgql::Reward &reward,
                                    const QString &text,
                                    const QString &emoteId)
{
    QPointer<TwitchActionsPopup> self(this);
    modgql::redeemReward(
        this->channelId_, reward, text,
        [self, title = reward.title](qint64) {
            if (self)
            {
                self->say(QStringLiteral("„%1“ eingelöst.").arg(title));
                self->refreshPoints();
            }
        },
        [self](const QString &error) {
            if (self)
            {
                self->say(error, true);
                self->refreshPoints();
            }
        },
        emoteId);
}

void TwitchActionsPopup::createPoll()
{
    const auto title = this->pollTitle_->text().trimmed();
    QStringList choices;
    for (const auto *choice : this->pollChoices_)
    {
        if (!choice->text().trimmed().isEmpty())
        {
            choices.append(choice->text().trimmed());
        }
    }
    if (title.isEmpty())
    {
        this->say(QStringLiteral("Die Umfrage braucht eine Frage."), true);
        return;
    }
    if (choices.size() < 2)
    {
        this->say(QStringLiteral("Eine Umfrage braucht mindestens zwei "
                                 "Antworten."),
                  true);
        return;
    }
    if (this->channelId_.isEmpty())
    {
        this->say(QStringLiteral("Die Kanal-ID ist noch nicht bekannt - "
                                 "versuch es gleich nochmal."),
                  true);
        return;
    }

    std::optional<int> perVote;
    if (this->pollPointsOn_->isChecked())
    {
        perVote = this->pollPoints_->value();
    }
    QPointer<TwitchActionsPopup> self(this);
    modgql::createPoll(
        this->channelId_, title, choices, this->pollDuration_->checkedId(),
        perVote,
        [self] {
            if (!self)
            {
                return;
            }
            self->say(QStringLiteral("Umfrage gestartet - sie erscheint "
                                     "gleich im Banner."));
            self->pollTitle_->clear();
            for (auto *choice : self->pollChoices_)
            {
                choice->clear();
            }
        },
        [self](const QString &error) {
            if (self)
            {
                self->say(error, true);
            }
        });
}

void TwitchActionsPopup::createPrediction()
{
    const auto title = this->predictionTitle_->text().trimmed();
    QStringList outcomes;
    for (const auto *outcome : this->predictionOutcomes_)
    {
        if (!outcome->text().trimmed().isEmpty())
        {
            outcomes.append(outcome->text().trimmed());
        }
    }
    if (title.isEmpty())
    {
        this->say(QStringLiteral("Die Wette braucht eine Frage."), true);
        return;
    }
    if (outcomes.size() < 2)
    {
        this->say(QStringLiteral("Eine Wette braucht mindestens zwei "
                                 "Ausgänge."),
                  true);
        return;
    }
    if (this->channelId_.isEmpty())
    {
        this->say(QStringLiteral("Die Kanal-ID ist noch nicht bekannt - "
                                 "versuch es gleich nochmal."),
                  true);
        return;
    }

    QPointer<TwitchActionsPopup> self(this);
    modgql::createPrediction(
        this->channelId_, title, outcomes, this->predictionWindow_->checkedId(),
        [self] {
            if (!self)
            {
                return;
            }
            self->say(QStringLiteral("Wette gestartet - sie erscheint "
                                     "gleich im Banner."));
            self->predictionTitle_->clear();
            for (auto *outcome : self->predictionOutcomes_)
            {
                outcome->clear();
            }
        },
        [self](const QString &error) {
            if (self)
            {
                self->say(error, true);
            }
        });
}

void TwitchActionsPopup::say(const QString &text, bool error)
{
    this->status_->setStyleSheet(
        error ? QStringLiteral("color: #ff6b6b;")
              : QStringLiteral("color: %1;")
                    .arg(getTheme()->messages.textColors.system.name()));
    this->status_->setText(text);
}

}  // namespace chatterino
