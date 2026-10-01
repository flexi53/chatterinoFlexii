// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitBanner.hpp"

#include "singletons/Theme.hpp"
#include "widgets/splits/Split.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QVBoxLayout>

using namespace std::chrono_literals;
using namespace Qt::Literals;

namespace chatterino {

namespace {

/// ChattiFlexii: the split a banner lies in, so its card can carry the
/// colour of that split's title bar - focused or not
const Split *splitOf(const QWidget *widget)
{
    for (const auto *w = widget; w != nullptr; w = w->parentWidget())
    {
        if (const auto *split = dynamic_cast<const Split *>(w))
        {
            return split;
        }
    }
    return nullptr;
}

}  // namespace

SplitBanner::SplitBanner(QWidget *parent)
    : BaseWidget(parent)
    , headerLabel_(new QLabel(this))
    , countdownLabel_(new QLabel(this))
    , headerRow_(new QHBoxLayout())
    , contentBox_(new QVBoxLayout())
    , countdownTimer_(new QTimer(this))
    , autoHideTimer_(new QTimer(this))
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *outerBox = new QVBoxLayout(this);
    outerBox->setContentsMargins(0, 0, 0, 0);
    outerBox->setSpacing(0);

    this->contentBox_->setContentsMargins(8, 6, 8, 6);
    this->contentBox_->setSpacing(3);

    this->headerRow_->setSpacing(4);
    this->headerRow_->addWidget(this->headerLabel_);
    this->headerRow_->addStretch(1);
    this->contentBox_->addLayout(this->headerRow_);

    outerBox->addLayout(this->contentBox_);

    // ChattiFlexii: no line under it any more - the card ends by itself,
    // and the chat shows through around it

    this->countdownLabel_->setStyleSheet(MUTED_STYLE);
    this->countdownLabel_->hide();

    this->countdownTimer_->setInterval(1s);
    QObject::connect(this->countdownTimer_, &QTimer::timeout, this, [this] {
        this->tickCountdown();
    });

    this->autoHideTimer_->setSingleShot(true);
    QObject::connect(this->autoHideTimer_, &QTimer::timeout, this, [this] {
        this->autoHide();
    });
}

QLabel *SplitBanner::headerLabel() const
{
    return this->headerLabel_;
}

QHBoxLayout *SplitBanner::headerRow() const
{
    return this->headerRow_;
}

QVBoxLayout *SplitBanner::contentBox() const
{
    return this->contentBox_;
}

QLabel *SplitBanner::countdownLabel() const
{
    return this->countdownLabel_;
}

void SplitBanner::setDismissed(bool dismissed)
{
    this->dismissed_ = dismissed;
}

bool SplitBanner::isDismissed() const
{
    return this->dismissed_;
}

void SplitBanner::showUnlessDismissed()
{
    if (this->dismissed_)
    {
        return;
    }
    this->show();
}

void SplitBanner::startCountdown()
{
    this->tickCountdown();
    this->countdownTimer_->start();
}

void SplitBanner::stopCountdown()
{
    this->countdownTimer_->stop();
}

void SplitBanner::startAutoHide(std::chrono::milliseconds delay)
{
    this->autoHideTimer_->start(delay);
}

void SplitBanner::stopAutoHide()
{
    this->autoHideTimer_->stop();
}

void SplitBanner::tickCountdown()
{
}

void SplitBanner::autoHide()
{
    this->hide();
}

QString SplitBanner::formatCountdown(qint64 millis)
{
    const qint64 totalSecs = (millis + 999) / 1000;  // round up
    const qint64 hours = totalSecs / 3600;
    const qint64 mins = (totalSecs % 3600) / 60;
    const qint64 secs = totalSecs % 60;

    if (hours > 0)
    {
        return u"%1:%2:%3"_s.arg(hours)
            .arg(mins, 2, 10, QChar(u'0'))
            .arg(secs, 2, 10, QChar(u'0'));
    }

    return u"%1:%2"_s.arg(mins, 2, 10, QChar(u'0'))
        .arg(secs, 2, 10, QChar(u'0'));
}

void SplitBanner::scaleChangedEvent(float newScale)
{
    QFont headerFont = this->headerLabel_->font();
    headerFont.setPointSizeF(9.5F * newScale);
    this->headerLabel_->setFont(headerFont);
    this->countdownLabel_->setFont(headerFont);

    // ChattiFlexii: what is written keeps its distance from the card's own
    // edge, which lies GAP inside the widget
    this->contentBox_->setContentsMargins(
        int((8 + GAP) * newScale), int((6 + GAP) * newScale),
        int((8 + GAP) * newScale), int(6 * newScale));
}

void SplitBanner::mousePressEvent(QMouseEvent *event)
{
    // ignore to disable the parent's right click menu
}

void SplitBanner::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    auto *theme = getTheme();

    // ChattiFlexii: a card set on the chat - a few pixels of air at the
    // sides and above it, so the chat shows through, and corners that are
    // rounded rather than cut
    const qreal gap = GAP * this->scale();
    const qreal radius = RADIUS * this->scale();
    const QRectF card = QRectF(this->rect()).adjusted(gap, gap, -gap, 0);

    // ChattiFlexii: the card carries the colour of the title bar, one to
    // one. Where that colour is see-through - Aussehen -> Farben - the bar
    // shows the chat through itself, so the card takes what that comes out
    // as. It stays solid either way: the card lies on the chat, and
    // messages running through the writing would not be readable.
    // The bar above lights up while one types in this split, and the card
    // goes along with it
    const auto *split = splitOf(this);
    auto fill = split != nullptr && split->hasFocus()
                    ? theme->splits.header.focusedBackground
                    : theme->splits.header.background;
    if (fill.alpha() < 255)
    {
        const auto under = theme->splits.background;
        const qreal share = fill.alphaF();
        fill = QColor::fromRgbF(
            under.redF() * (1 - share) + fill.redF() * share,
            under.greenF() * (1 - share) + fill.greenF() * share,
            under.blueF() * (1 - share) + fill.blueF() * share);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(card, radius, radius);
}

}  // namespace chatterino
