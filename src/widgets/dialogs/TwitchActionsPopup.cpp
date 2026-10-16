// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/TwitchActionsPopup.hpp"

#include "providers/twitch/pubsubmessages/Polls.hpp"
#include "providers/twitch/pubsubmessages/Predictions.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include <initializer_list>
#include <utility>

namespace chatterino {

namespace {

void clearLayout(QVBoxLayout *layout)
{
    while (auto *item = layout->takeAt(0))
    {
        if (auto *widget = item->widget())
        {
            widget->deleteLater();
        }
        delete item;
    }
}

QString points(qint64 value)
{
    return QLocale(QLocale::German).toString(value);
}

/// The lines typed into a box, trimmed, the empty ones left out
QStringList linesOf(const QPlainTextEdit *edit)
{
    QStringList lines;
    for (const auto &line : edit->toPlainText().split(u'\n'))
    {
        const auto trimmed = line.trimmed();
        if (!trimmed.isEmpty())
        {
            lines.append(trimmed);
        }
    }
    return lines;
}

QLabel *boldLabel(const QString &text)
{
    auto *label = new QLabel(text);
    auto font = label->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.1);
    label->setFont(font);
    label->setWordWrap(true);
    return label;
}

QComboBox *durationBox(
    std::initializer_list<std::pair<const char *, int>> choices, int current)
{
    auto *box = new QComboBox;
    for (const auto &[label, seconds] : choices)
    {
        box->addItem(QString::fromUtf8(label), seconds);
    }
    box->setCurrentIndex(current);
    return box;
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
    this->setWindowTitle(QStringLiteral("Twitch – #%1").arg(this->channelLogin_));
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setMinimumSize(440, 440);

    auto *layout = new QVBoxLayout(this->getLayoutContainer());
    layout->setContentsMargins(10, 10, 10, 10);

    // Without the login nothing here works - it is made under Twitch-Extras.
    // Until then the window shows only that, rather than greyed out tabs
    // that look as if they should work
    this->loginNote_ = new QWidget;
    {
        auto *box = new QVBoxLayout(this->loginNote_);
        box->setContentsMargins(20, 20, 20, 20);
        box->addStretch(1);
        box->addWidget(boldLabel(QStringLiteral("Noch nicht angemeldet")), 0,
                       Qt::AlignHCenter);
        auto *text = new QLabel(QStringLiteral(
            "Kanalpunkte, Umfragen und Vorhersagen laufen über eine eigene "
            "Anmeldung als Twitch-App. Die machst du einmal unter "
            "Einstellungen → Twitch-Extras: Code holen, auf "
            "twitch.tv/activate eingeben, fertig. Danach geht es hier "
            "von selbst weiter."));
        text->setWordWrap(true);
        text->setAlignment(Qt::AlignHCenter);
        box->addWidget(text);
        auto *open = new QPushButton(QStringLiteral("Jetzt anmelden …"));
        open->setDefault(true);
        open->setMinimumHeight(32);
        QObject::connect(open, &QPushButton::clicked, this, [this] {
            SettingsDialog::showDialog(this,
                                       SettingsDialogPreference::TwitchExtras);
        });
        box->addSpacing(8);
        box->addWidget(open, 0, Qt::AlignHCenter);
        box->addStretch(1);
    }
    this->loginNote_->hide();
    layout->addWidget(this->loginNote_, 1);

    this->tabs_ = new QTabWidget;
    this->tabs_->addTab(this->buildPointsTab(), QStringLiteral("Kanalpunkte"));
    if (this->moderates_)
    {
        this->tabs_->addTab(this->buildPollTab(),
                            QStringLiteral("Umfrage starten"));
        this->tabs_->addTab(this->buildPredictionTab(),
                            QStringLiteral("Vorhersage starten"));
    }
    else
    {
        // One tab needs no row of tabs
        this->tabs_->tabBar()->hide();
    }
    layout->addWidget(this->tabs_, 1);

    this->status_ = new QLabel;
    this->status_->setWordWrap(true);
    layout->addWidget(this->status_);

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

QWidget *TwitchActionsPopup::buildPointsTab()
{
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    auto *top = new QHBoxLayout;
    this->pointsBalance_ = boldLabel(QString());
    top->addWidget(this->pointsBalance_, 1);
    auto *reload = new QPushButton(QStringLiteral("Neu laden"));
    QObject::connect(reload, &QPushButton::clicked, this, [this] {
        this->refreshPoints();
    });
    top->addWidget(reload);
    layout->addLayout(top);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *list = new QWidget;
    this->pointsList_ = new QVBoxLayout(list);
    this->pointsList_->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(list);
    layout->addWidget(scroll, 1);
    return tab;
}

QWidget *TwitchActionsPopup::buildPollTab()
{
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    this->pollRunning_ = new QLabel(QStringLiteral(
        "Gerade läuft eine Umfrage - Stand, Abstimmen und Beenden findest du "
        "im Banner über dem Chat. Ist sie vorbei, kannst du hier die nächste "
        "starten."));
    this->pollRunning_->setWordWrap(true);
    layout->addWidget(this->pollRunning_);

    this->pollForm_ = new QWidget;
    auto *form = new QFormLayout(this->pollForm_);
    form->setContentsMargins(0, 0, 0, 0);
    this->pollTitle_ = new QLineEdit;
    this->pollTitle_->setMaxLength(60);
    this->pollTitle_->setPlaceholderText(QStringLiteral("Worum geht's?"));
    form->addRow(QStringLiteral("Frage"), this->pollTitle_);
    this->pollChoices_ = new QPlainTextEdit;
    this->pollChoices_->setPlaceholderText(
        QStringLiteral("Eine Antwort pro Zeile, 2 bis 5"));
    this->pollChoices_->setFixedHeight(
        this->pollChoices_->fontMetrics().lineSpacing() * 5 + 14);
    form->addRow(QStringLiteral("Antworten"), this->pollChoices_);
    this->pollDuration_ = durationBox({{"30 Sekunden", 30},
                                       {"1 Minute", 60},
                                       {"2 Minuten", 120},
                                       {"3 Minuten", 180},
                                       {"5 Minuten", 300},
                                       {"10 Minuten", 600},
                                       {"30 Minuten", 1800}},
                                      2);
    form->addRow(QStringLiteral("Dauer"), this->pollDuration_);
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
    form->addRow(QString(), pointsRow);
    auto *start = new QPushButton(QStringLiteral("Umfrage starten"));
    form->addRow(QString(), start);
    QObject::connect(start, &QPushButton::clicked, this, [this] {
        this->createPoll();
    });
    layout->addWidget(this->pollForm_);

    layout->addStretch(1);
    return tab;
}

QWidget *TwitchActionsPopup::buildPredictionTab()
{
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    this->predictionRunning_ = new QLabel(QStringLiteral(
        "Gerade läuft eine Vorhersage - Sperren, Abbrechen und den Gewinner "
        "wählen geht im Banner über dem Chat. Ist sie entschieden, kannst du "
        "hier die nächste starten."));
    this->predictionRunning_->setWordWrap(true);
    layout->addWidget(this->predictionRunning_);

    this->predictionForm_ = new QWidget;
    auto *form = new QFormLayout(this->predictionForm_);
    form->setContentsMargins(0, 0, 0, 0);
    this->predictionTitle_ = new QLineEdit;
    this->predictionTitle_->setMaxLength(45);
    this->predictionTitle_->setPlaceholderText(
        QStringLiteral("Was wird passieren?"));
    form->addRow(QStringLiteral("Frage"), this->predictionTitle_);
    this->predictionOutcomes_ = new QPlainTextEdit;
    this->predictionOutcomes_->setPlaceholderText(
        QStringLiteral("Ein Ausgang pro Zeile, 2 bis 10"));
    this->predictionOutcomes_->setFixedHeight(
        this->predictionOutcomes_->fontMetrics().lineSpacing() * 5 + 14);
    form->addRow(QStringLiteral("Ausgänge"), this->predictionOutcomes_);
    this->predictionWindow_ = durationBox({{"30 Sekunden", 30},
                                           {"1 Minute", 60},
                                           {"2 Minuten", 120},
                                           {"5 Minuten", 300},
                                           {"10 Minuten", 600},
                                           {"15 Minuten", 900},
                                           {"30 Minuten", 1800}},
                                          2);
    form->addRow(QStringLiteral("Tippen offen"), this->predictionWindow_);
    auto *start = new QPushButton(QStringLiteral("Vorhersage starten"));
    form->addRow(QString(), start);
    QObject::connect(start, &QPushButton::clicked, this, [this] {
        this->createPrediction();
    });
    layout->addWidget(this->predictionForm_);

    layout->addStretch(1);
    return tab;
}

void TwitchActionsPopup::showState()
{
    this->loginNote_->setVisible(!this->loggedIn_);
    this->tabs_->setVisible(this->loggedIn_);

    if (!this->moderates_)
    {
        return;
    }
    const auto channel = this->channel_.lock();
    const auto *poll = channel ? channel->currentPoll() : nullptr;
    const auto *prediction = channel ? channel->currentPrediction() : nullptr;

    const bool pollRuns = poll != nullptr && poll->running();
    this->pollRunning_->setVisible(pollRuns);
    this->pollForm_->setEnabled(!pollRuns);

    // A locked one still waits for its result, so it is not over yet
    const bool predictionRuns =
        prediction != nullptr &&
        (prediction->status == PubSubPrediction::Status::Active ||
         prediction->status == PubSubPrediction::Status::Locked);
    this->predictionRunning_->setVisible(predictionRuns);
    this->predictionForm_->setEnabled(!predictionRuns);
}

void TwitchActionsPopup::refreshPoints()
{
    if (!this->loggedIn_)
    {
        this->pointsBalance_->clear();
        clearLayout(this->pointsList_);
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
            self->pointsBalance_->clear();
            clearLayout(self->pointsList_);
            self->say(error, true);
        });
}

void TwitchActionsPopup::showPoints(const modgql::ChannelPoints &channelPoints)
{
    if (this->channelId_.isEmpty())
    {
        this->channelId_ = channelPoints.channelId;
    }
    this->pointsBalance_->setText(
        QStringLiteral("%1 Kanalpunkte").arg(points(channelPoints.balance)));

    clearLayout(this->pointsList_);
    if (channelPoints.rewards.empty())
    {
        this->pointsList_->addWidget(new QLabel(
            QStringLiteral("Dieser Kanal hat keine eigenen Belohnungen.")));
    }
    for (const auto &reward : channelPoints.rewards)
    {
        auto *box = new QFrame;
        box->setFrameShape(QFrame::StyledPanel);
        auto *boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(8, 6, 8, 6);

        auto *top = new QHBoxLayout;
        auto *title = new QLabel(
            QStringLiteral("<b>%1</b>  ·  %2")
                .arg(reward.title.toHtmlEscaped(), points(reward.cost)));
        title->setTextFormat(Qt::RichText);
        title->setWordWrap(true);
        top->addWidget(title, 1);
        auto *redeem = new QPushButton(QStringLiteral("Einlösen"));
        const bool affordable = channelPoints.balance >= reward.cost;
        redeem->setEnabled(reward.enabled && reward.inStock && affordable);
        if (!reward.enabled)
        {
            redeem->setToolTip(QStringLiteral("Gerade pausiert."));
        }
        else if (!reward.inStock)
        {
            redeem->setToolTip(QStringLiteral("Gerade nicht verfügbar."));
        }
        else if (!affordable)
        {
            redeem->setToolTip(
                QStringLiteral("Dafür reichen die Punkte nicht."));
        }
        top->addWidget(redeem);
        boxLayout->addLayout(top);

        if (!reward.prompt.trimmed().isEmpty())
        {
            auto *prompt = new QLabel(reward.prompt);
            prompt->setWordWrap(true);
            prompt->setEnabled(false);
            boxLayout->addWidget(prompt);
        }
        QLineEdit *text = nullptr;
        if (reward.needsText)
        {
            text = new QLineEdit;
            text->setPlaceholderText(QStringLiteral("Dein Text dazu"));
            boxLayout->addWidget(text);
        }

        QObject::connect(
            redeem, &QPushButton::clicked, this, [this, reward, text, redeem] {
                const auto typed = text != nullptr ? text->text() : QString();
                if (reward.needsText && typed.trimmed().isEmpty())
                {
                    this->say(QStringLiteral("Die Belohnung braucht einen "
                                             "Text."),
                              true);
                    return;
                }
                redeem->setEnabled(false);
                QPointer<TwitchActionsPopup> self(this);
                modgql::redeemReward(
                    this->channelId_, reward, typed,
                    [self, title = reward.title](qint64) {
                        if (self)
                        {
                            self->say(QStringLiteral("„%1“ eingelöst.")
                                          .arg(title));
                            self->refreshPoints();
                        }
                    },
                    [self](const QString &error) {
                        if (self)
                        {
                            self->say(error, true);
                            self->refreshPoints();
                        }
                    });
            });

        this->pointsList_->addWidget(box);
    }
    this->pointsList_->addStretch(1);
}

void TwitchActionsPopup::createPoll()
{
    const auto title = this->pollTitle_->text().trimmed();
    const auto choices = linesOf(this->pollChoices_);
    if (title.isEmpty())
    {
        this->say(QStringLiteral("Die Umfrage braucht eine Frage."), true);
        return;
    }
    if (choices.size() < 2 || choices.size() > 5)
    {
        this->say(QStringLiteral("Eine Umfrage hat 2 bis 5 Antworten."), true);
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
        this->channelId_, title, choices,
        this->pollDuration_->currentData().toInt(), perVote,
        [self] {
            if (self)
            {
                self->say(QStringLiteral("Umfrage gestartet - sie erscheint "
                                         "gleich im Banner."));
                self->pollTitle_->clear();
                self->pollChoices_->clear();
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
    const auto outcomes = linesOf(this->predictionOutcomes_);
    if (title.isEmpty())
    {
        this->say(QStringLiteral("Die Vorhersage braucht eine Frage."), true);
        return;
    }
    if (outcomes.size() < 2 || outcomes.size() > 10)
    {
        this->say(QStringLiteral("Eine Vorhersage hat 2 bis 10 Ausgänge."),
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
        this->channelId_, title, outcomes,
        this->predictionWindow_->currentData().toInt(),
        [self] {
            if (self)
            {
                self->say(QStringLiteral("Vorhersage gestartet - sie "
                                         "erscheint gleich im Banner."));
                self->predictionTitle_->clear();
                self->predictionOutcomes_->clear();
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
    this->status_->setStyleSheet(error ? QStringLiteral("color: #ff6b6b;")
                                       : QString());
    this->status_->setText(text);
}

}  // namespace chatterino
