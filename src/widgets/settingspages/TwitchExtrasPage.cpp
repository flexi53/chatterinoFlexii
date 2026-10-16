// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/TwitchExtrasPage.hpp"

#include "providers/twitch/api/ModGql.hpp"
#include "singletons/Settings.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace chatterino {

namespace {

/// A button that puts a section back the way ChattiFlexii starts out
void addStandardButton(GeneralPageView &layout, const QString &tooltip,
                       std::function<void()> reset)
{
    auto *button = new QPushButton("Standard");
    button->setToolTip(tooltip);
    QObject::connect(button, &QPushButton::clicked, std::move(reset));
    layout.addWidget(button);
}

}  // namespace

TwitchExtrasPage::TwitchExtrasPage()
    : view_(GeneralPageView::withoutNavigation(this))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(this->view_);

    this->initLogin(*this->view_);
    this->initBanner(*this->view_);
    this->initWindow(*this->view_);
    this->view_->addStretch();

    this->loginPoll_.setSingleShot(true);

    this->managedConnections_.managedConnect(modgql::loginChanged(), [this] {
        this->showLoginState();
    });
    QPointer<TwitchExtrasPage> self(this);
    modgql::whenReady(this, [self](bool) {
        if (self)
        {
            self->showLoginState();
        }
    });
}

void TwitchExtrasPage::initLogin(GeneralPageView &layout)
{
    layout.addTitle("Anmeldung als Twitch-App");
    layout.addDescription(
        "Twitch lässt Umfragen und Vorhersagen über seine offene "
        "Schnittstelle nur den Streamer starten - und abstimmen, wetten oder "
        "Kanalpunkte einlösen dort gar nicht. Die Webseite und Twitchs eigene "
        "Apps können das. ChattiFlexii meldet sich deshalb wie Moltorino als "
        "Twitchs TV-App an: Du bekommst einen Code und gibst ihn auf "
        "twitch.tv/activate ein - kein Passwort, kein Token zum Kopieren.");
    layout.addDescription(
        "Das ist eine Anmeldung für sich, getrennt von dem Account, mit dem "
        "du chattest - melde dich mit dem an, mit dem du mitmachen willst. "
        "Sie bleibt im Schlüsselbund dieses Computers, nie in den "
        "Einstellungen, einem Export oder dem Abgleich. Twitch sieht diesen "
        "Weg für andere Apps nicht vor; ändert Twitch etwas, kann er ohne "
        "Vorwarnung aufhören zu gehen.");

    this->loginState_ = new QLabel;
    this->loginState_->setWordWrap(true);
    this->loginState_->setTextFormat(Qt::RichText);
    this->loginState_->setOpenExternalLinks(true);
    this->loginState_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    layout.addWidget(this->loginState_,
                     {"twitch", "app", "tv", "login", "anmelden"});

    this->loginButton_ = new QPushButton("Bei Twitch anmelden");
    this->logoutButton_ = new QPushButton("Abmelden");
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(this->loginButton_);
    row->addWidget(this->logoutButton_);
    row->addStretch(1);
    auto *rowWidget = new QWidget;
    rowWidget->setLayout(row);
    layout.addWidget(rowWidget, {"twitch", "app", "tv", "login", "anmelden",
                                 "abmelden"});

    QObject::connect(this->loginButton_, &QPushButton::clicked, this, [this] {
        this->startLogin();
    });
    QObject::connect(this->logoutButton_, &QPushButton::clicked, this, [] {
        modgql::logout();
    });

    this->showLoginState();
}

void TwitchExtrasPage::initBanner(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Im Banner");
    layout.addDescription(
        "Das Banner mit der laufenden Umfrage oder Vorhersage stellst du "
        "unter Aussehen → Splits → Banner ein. Mit der Anmeldung oben kannst "
        "du dort direkt mitmachen: eine Zeile anklicken stimmt dafür oder "
        "setzt Punkte darauf. Als Mod bekommst du dazu Beenden, Sperren und "
        "Abbrechen, und ein Klick auf einen Ausgang lässt ihn gewinnen.");

    SettingWidget::checkbox("Im Banner abstimmen und wetten",
                            s.voteBannerActions)
        ->setTooltip("Ohne Haken bleibt das Banner reine Anzeige.")
        ->addKeywords({"umfrage", "poll", "vorhersage", "prediction",
                       "abstimmen", "wetten", "banner"})
        ->addTo(layout);
    SettingWidget::lineEdit("Einsätze zur Auswahl", s.predictionStakes,
                            "10, 100, 500, 1000, 5000")
        ->setTooltip("Was beim Klick auf einen Ausgang zur Wahl steht, durch "
                     "Kommas getrennt. „Eigener Einsatz …“ gibt es immer "
                     "dazu.")
        ->addKeywords({"einsatz", "punkte", "vorhersage", "wetten"})
        ->addTo(layout);

    addStandardButton(layout, "Mitmachen im Banner an, mit den Einsätzen vom "
                              "Anfang",
                      [&s] {
                          s.voteBannerActions.setValue(
                              s.voteBannerActions.getDefaultValue());
                          s.predictionStakes.setValue(
                              s.predictionStakes.getDefaultValue());
                      });
}

void TwitchExtrasPage::initWindow(GeneralPageView &layout)
{
    layout.addTitle("Umfragen, Vorhersagen und Kanalpunkte");
    layout.addDescription(
        "Neben dem Eingabefeld steht dein Kontostand an Kanalpunkten im "
        "Kanal. Ein Klick darauf öffnet die Belohnungen zum Einlösen - und "
        "als Mod das Starten von Umfragen und Vorhersagen. Ob die Anzeige da "
        "ist und wo sie steht, stellst du unter Eingabefeld → Knöpfe ein.");
}

void TwitchExtrasPage::showLoginState()
{
    if (this->waiting_)
    {
        return;
    }
    const auto login = modgql::currentLogin();
    if (login)
    {
        this->loginState_->setText(
            QStringLiteral("Angemeldet als <b>%1</b>.")
                .arg(login->login.toHtmlEscaped()));
    }
    else
    {
        this->loginState_->setText(QStringLiteral("Nicht angemeldet."));
    }
    this->loginButton_->setEnabled(!login.has_value());
    this->logoutButton_->setEnabled(login.has_value());
}

void TwitchExtrasPage::startLogin()
{
    this->waiting_ = true;
    this->loginButton_->setEnabled(false);
    this->loginState_->setText(QStringLiteral("Frage Twitch nach einem Code …"));

    QPointer<TwitchExtrasPage> self(this);
    modgql::startDeviceLogin(
        [self](const modgql::DeviceCode &code) {
            if (!self)
            {
                return;
            }
            self->loginState_->setText(
                QStringLiteral(
                    "Gib auf <a href=\"%1\">%1</a> den Code <b "
                    "style=\"font-size:15px\">%2</b> ein - die Seite ist "
                    "schon im Browser offen. Hier merkt man von selbst, wenn "
                    "es geklappt hat.")
                    .arg(code.verificationUri.toHtmlEscaped(),
                         code.userCode.toHtmlEscaped()));
            QDesktopServices::openUrl(QUrl(code.verificationUri));
            self->waitForLogin(code.deviceCode, code.intervalSeconds);
        },
        [self](const QString &error) {
            if (!self)
            {
                return;
            }
            self->waiting_ = false;
            self->showLoginState();
            self->loginState_->setText(
                QStringLiteral("<span style=\"color:#ff6b6b\">%1</span>")
                    .arg(error.toHtmlEscaped()));
        });
}

void TwitchExtrasPage::waitForLogin(const QString &deviceCode,
                                    int intervalSeconds)
{
    this->loginPoll_.disconnect();
    QObject::connect(
        &this->loginPoll_, &QTimer::timeout, this,
        [this, deviceCode, intervalSeconds] {
            QPointer<TwitchExtrasPage> self(this);
            modgql::pollDeviceLogin(
                deviceCode, [self, deviceCode, intervalSeconds](
                                modgql::DeviceState state,
                                const QString &error) {
                    if (!self)
                    {
                        return;
                    }
                    switch (state)
                    {
                        case modgql::DeviceState::Pending:
                            self->waitForLogin(deviceCode, intervalSeconds);
                            break;
                        case modgql::DeviceState::SlowDown:
                            self->waitForLogin(deviceCode,
                                               intervalSeconds + 5);
                            break;
                        case modgql::DeviceState::Done:
                            self->waiting_ = false;
                            self->showLoginState();
                            break;
                        case modgql::DeviceState::Failed:
                            self->waiting_ = false;
                            self->showLoginState();
                            self->loginState_->setText(
                                QStringLiteral(
                                    "<span style=\"color:#ff6b6b\">%1</span>")
                                    .arg(error.toHtmlEscaped()));
                            break;
                    }
                });
        });
    this->loginPoll_.start(intervalSeconds * 1000);
}

bool TwitchExtrasPage::filterElements(const QString &query)
{
    return this->view_->filterElements(query) || query.isEmpty();
}

}  // namespace chatterino
