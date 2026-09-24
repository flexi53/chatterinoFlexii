// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/BadgesPage.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/badgealerts/BadgeAlerts.hpp"
#include "common/Credentials.hpp"
#include "providers/badgebase/BadgeBase.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchWebBadges.hpp"
#include "singletons/Settings.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/settingspages/PageSections.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <initializer_list>
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

/// A field for something secret with its three buttons, and a line for
/// what happened
struct SecretRow {
    QLineEdit *field;
    QPushButton *save;
    QPushButton *test;
    QPushButton *forget;
    QLabel *status;
};

SecretRow addSecretRow(GeneralPageView &layout, const QString &placeholder,
                       const QString &forgetText, const QStringList &keywords)
{
    SecretRow row{
        .field = new QLineEdit,
        .save = new QPushButton("Speichern"),
        .test = new QPushButton("Testen"),
        .forget = new QPushButton(forgetText),
        .status = new QLabel,
    };
    row.field->setEchoMode(QLineEdit::Password);
    row.field->setPlaceholderText(placeholder);

    auto *line = new QHBoxLayout;
    line->setContentsMargins(0, 0, 0, 0);
    line->addWidget(row.field, 1);
    line->addWidget(row.save);
    line->addWidget(row.test);
    line->addWidget(row.forget);
    auto *lineWidget = new QWidget;
    lineWidget->setLayout(line);
    layout.addWidget(lineWidget, keywords);

    row.status->setWordWrap(true);
    row.status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout.addWidget(row.status, keywords);
    return row;
}

}  // namespace

BadgesPage::BadgesPage()
    : view_(GeneralPageView::withoutNavigation(this))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(this->view_);

    this->initAlerts(*this->view_);
    this->initSwitching(*this->view_);
    this->view_->addStretch();
}

void BadgesPage::initAlerts(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Neue Badges");
    layout.addDescription(
        "Ein eigener Tab, der meldet, wenn es ein Badge zu holen gibt: was "
        "jetzt verfügbar ist, was bald kommt und was bald endet - auf Wunsch "
        "nur, was dir noch fehlt. Er schaut jede Viertelstunde nach und "
        "sagt nichts doppelt. Mit dem Haken geht der Tab auf.");

    SettingWidget::checkbox("Neue Badges melden", s.badgeAlertsEnabled)
        ->setTooltip("Öffnet den Tab „Neue Badges“ und schaut ab jetzt jede "
                     "Viertelstunde nach. Den Tab kannst du schließen und "
                     "mit dem Knopf darunter wieder holen.")
        ->addKeywords({"badge", "badgebase", "neu", "melden"})
        ->addTo(layout);
    // Ticked, the tab is there at once
    s.badgeAlertsEnabled.connect(
        [](const bool enabled, auto) {
            if (enabled)
            {
                BadgeAlerts::openTab();
            }
        },
        this->managedConnections_, false);

    {
        auto *open = new QPushButton("Tab öffnen");
        auto *now = new QPushButton("Jetzt nachsehen");
        now->setToolTip("Fragt gleich, statt auf die nächste Viertelstunde "
                        "zu warten");
        QObject::connect(open, &QPushButton::clicked, [] {
            BadgeAlerts::openTab();
        });
        QObject::connect(now, &QPushButton::clicked, [] {
            BadgeAlerts::openTab();
            BadgeAlerts::instance().checkNow();
        });
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(open);
        row->addWidget(now);
        row->addStretch(1);
        auto *rowWidget = new QWidget;
        rowWidget->setLayout(row);
        layout.addWidget(rowWidget, {"badge", "tab"});
    }

    layout.addSubtitle("Was gemeldet wird");
    SettingWidget::checkbox("Jetzt verfügbar", s.badgeAlertsAvailable)
        ->setTooltip("Sobald ein Badge zu holen ist, mit Enddatum und ob es "
                     "etwas kostet.")
        ->addTo(layout);
    SettingWidget::checkbox("Kommt bald", s.badgeAlertsUpcoming)
        ->setTooltip("Was BadgeBase schon kennt, das aber erst noch kommt - "
                     "mit Startdatum.")
        ->addTo(layout);
    SettingWidget::checkbox("Endet bald", s.badgeAlertsEnding)
        ->setTooltip("Einen Tag, bevor ein Badge nicht mehr zu holen ist.")
        ->addTo(layout);
    SettingWidget::checkbox("Auch kostenpflichtige (z. B. mit Sub)",
                            s.badgeAlertsPaid)
        ->setTooltip("Badges, die etwas kosten - meist ein Sub oder ein Kauf. "
                     "Ohne den Haken meldet der Tab nur, was es kostenlos "
                     "gibt. Welches was ist, steht vorne an jeder Meldung.")
        ->addKeywords({"kostenlos", "kostenpflichtig", "sub", "paid", "free"})
        ->addTo(layout);
    SettingWidget::checkbox("Nur, was mir noch fehlt", s.badgeAlertsOnlyMissing)
        ->setTooltip("BadgeBase weiß, welche Badges du schon hast - dazu "
                     "kommt dann nichts mehr. Das gilt für „Jetzt "
                     "verfügbar“ und „Endet bald“.")
        ->addKeywords({"fehlt", "missing"})
        ->addTo(layout);
    SettingWidget::checkbox("Auch neue Badges von Twitch selbst",
                            s.badgeAlertsTwitch)
        ->setTooltip("Jedes neue globale Badge, sobald Twitch es freischaltet "
                     "- auch ohne BadgeBase-Schlüssel, dann aber ohne "
                     "Termine.")
        ->addKeywords({"twitch", "global"})
        ->addTo(layout);
    SettingWidget::checkbox("Ton bei einer Meldung", s.badgeAlertsSound)
        ->setTooltip("Kommen mehrere Meldungen zusammen, spielt der Ton der "
                     "dringendsten: erst „Endet bald“, dann „Jetzt "
                     "verfügbar“, dann „Kommt bald“, dann „Neu bei Twitch“.")
        ->addKeywords({"ton", "sound"})
        ->addTo(layout);
    {
        // A sound of its own per kind, so it is clear without looking
        const auto soundRow = [this, &layout, &s](const QString &name,
                                                  QStringSetting &setting) {
            auto *row = new QWidget;
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(20, 0, 0, 0);
            auto *label = new QLabel(name + ":");
            label->setMinimumWidth(130);
            rowLayout->addWidget(label);
            rowLayout->addWidget(pagesections::soundChooser(
                row, setting, this->managedConnections_));
            layout.addWidget(row, {"ton", "sound", name});

            s.badgeAlertsSound.connect(
                [row](const bool on, auto) {
                    row->setEnabled(on);
                },
                this->managedConnections_);
        };
        soundRow("Jetzt verfügbar", s.badgeSoundAvailable);
        soundRow("Kommt bald", s.badgeSoundUpcoming);
        soundRow("Endet bald", s.badgeSoundEnding);
        soundRow("Neu bei Twitch", s.badgeSoundTwitch);
    }

    layout.addSubtitle("Farben");
    SettingWidget::checkbox("Meldungen farbig hinterlegen",
                            s.badgeAlertsColored)
        ->setTooltip("Jede Art von Meldung auf ihrer Farbe - so siehst du auf "
                     "einen Blick, was verfügbar ist, was kommt und was bald "
                     "endet. Gilt für die Meldungen ab jetzt.")
        ->addKeywords({"farbe", "farbig", "hintergrund"})
        ->addTo(layout);
    {
        const auto colorRow = [this, &layout, &s](const QString &name,
                                                  QStringSetting &setting) {
            auto *label = new QLabel(name + ":");
            auto *button = new ColorButton(QColor(setting.getValue()));
            button->setFixedSize(50, 24);
            auto *row = new QHBoxLayout;
            row->setContentsMargins(20, 0, 0, 0);
            row->addWidget(label);
            row->addStretch(1);
            row->addWidget(button);
            auto *rowWidget = new QWidget;
            rowWidget->setLayout(row);
            layout.addWidget(rowWidget, {"farbe", name});

            QObject::connect(button, &ColorButton::clicked, [button, &setting] {
                auto *dialog = new ColorPickerDialog(QColor(setting), button);
                QObject::connect(
                    dialog, &ColorPickerDialog::colorConfirmed, button,
                    [&setting](const QColor &picked) {
                        if (picked.isValid())
                        {
                            setting.setValue(picked.name(QColor::HexArgb));
                        }
                    });
                dialog->show();
            });
            setting.connect(
                [button](const QString &value, auto) {
                    button->setColor(QColor(value));
                },
                this->managedConnections_, false);
            // Greyed out while the messages are not coloured
            s.badgeAlertsColored.connect(
                [rowWidget](const bool colored, auto) {
                    rowWidget->setEnabled(colored);
                },
                this->managedConnections_);
        };
        colorRow("Jetzt verfügbar", s.badgeColorAvailable);
        colorRow("Kommt bald", s.badgeColorUpcoming);
        colorRow("Endet bald", s.badgeColorEnding);
        colorRow("Neu bei Twitch", s.badgeColorTwitch);
    }

    layout.addSubtitle("Wo die Schlüssel liegen");
    layout.addDescription(
        "Der Browser-Login und der BadgeBase-Schlüssel liegen normalerweise "
        "im Schlüsselbund des Systems. Dort fragt macOS bei jeder neuen "
        "Version einmal nach deinem Passwort, weil sich die Signatur des "
        "Programms ändert - bei zwei Schlüsseln also zweimal.");
    {
        auto *without = new QCheckBox("Ohne Schlüsselbund speichern");
        without->setToolTip(
            "Legt beide neben die Einstellungen, verschlüsselt und nur für "
            "dein Benutzerkonto lesbar. Dann fragt macOS nie wieder. Die "
            "Datei ist an diesen Mac gebunden - auf einem anderen Rechner "
            "oder in einem Backup ist sie wertlos. Gegen Programme, die "
            "unter deinem Konto laufen, schützt sie nicht; dein "
            "Twitch-Login liegt ohnehin so in den Einstellungen.");
        without->setChecked(s.keepSecretsLocally);
        layout.addWidget(without,
                         {"schlüsselbund", "keychain", "passwort", "lokal"});

        auto *where = new QLabel;
        where->setWordWrap(true);
        layout.addWidget(where, {"schlüsselbund"});
        const auto showWhere = [where] {
            where->setText(getSettings()->keepSecretsLocally
                               ? "Liegen neben den Einstellungen, an diesen "
                                 "Mac gebunden."
                               : "Liegen im Schlüsselbund des Systems.");
        };
        showWhere();

        // Moved over as they are, so nothing has to be entered again:
        // first read, then put in the new place, and only then take out of
        // the old one - and never take out what could not be read, or it
        // would be gone for good
        QObject::connect(
            without, &QCheckBox::toggled, this,
            [this, where, showWhere](const bool local) {
                if (local == getSettings()->keepSecretsLocally)
                {
                    return;
                }
                where->setText("Wird verschoben …");

                webbadges::load(this, [this, local, where, showWhere](
                                          const QString &token) {
                    badgebase::loadKey(
                        this, [local, token, where,
                               showWhere](const QString &key) {
                            getSettings()->keepSecretsLocally.setValue(local);

                            QStringList moved;
                            QStringList missing;
                            if (token.isEmpty())
                            {
                                missing.append("Browser-Login");
                            }
                            else if (webbadges::store(token))
                            {
                                moved.append("Browser-Login");
                            }
                            if (key.isEmpty())
                            {
                                missing.append("BadgeBase-Schlüssel");
                            }
                            else if (badgebase::storeKey(key))
                            {
                                moved.append("BadgeBase-Schlüssel");
                            }

                            // Only now out of the old place, and only what
                            // really arrived in the new one
                            for (const auto &what : moved)
                            {
                                if (what == "Browser-Login")
                                {
                                    if (local)
                                    {
                                        Credentials::eraseFromKeychain(
                                            "twitchweb", "browser");
                                    }
                                    else
                                    {
                                        Credentials::eraseFromLocal(
                                            "twitchweb", "browser");
                                    }
                                }
                                else
                                {
                                    if (local)
                                    {
                                        Credentials::eraseFromKeychain(
                                            "badgebase", "key");
                                    }
                                    else
                                    {
                                        Credentials::eraseFromLocal(
                                            "badgebase", "key");
                                    }
                                }
                            }

                            showWhere();
                            if (!moved.isEmpty() || !missing.isEmpty())
                            {
                                auto text = where->text();
                                if (!moved.isEmpty())
                                {
                                    text += " Verschoben: " +
                                            moved.join(", ") + ".";
                                }
                                if (!missing.isEmpty())
                                {
                                    text += " Nicht gefunden und daher nicht "
                                            "angerührt: " +
                                            missing.join(", ") + ".";
                                }
                                where->setText(text);
                            }
                        });
                });
            });
    }

    layout.addSubtitle("BadgeBase-Schlüssel");
    layout.addDescription(
        "Für Termine, „kommt bald“ und „fehlt dir noch“. Jeder braucht einen "
        "eigenen - kostenlos unter "
        "<a href=\"https://badgebase.de/login/\">badgebase.de/login</a>. "
        "Er bleibt auf diesem Computer - im Schlüsselbund oder, wenn du es "
        "oben umstellst, verschlüsselt daneben - nie in die Einstellungen, "
        "einen Export, ein Backup oder "
        "den Abgleich - und gehört nicht in einen Chat.");
    const auto key =
        addSecretRow(layout, "Schlüssel hier einfügen", "Schlüssel löschen",
                     {"badgebase", "schlüssel", "key"});

    if (!badgebase::canStore())
    {
        for (auto *widget : std::initializer_list<QWidget *>{
                 key.field, key.save, key.test, key.forget})
        {
            widget->setEnabled(false);
        }
        key.status->setText("Geht nur mit dem Schlüsselbund des Systems - in "
                            "der portablen Version nicht.");
    }
    else
    {
        auto *status = key.status;
        badgebase::loadKey(status, [status](const QString &stored) {
            status->setText(stored.isEmpty()
                                ? "Kein Schlüssel gespeichert."
                                : "Ein Schlüssel ist im Schlüsselbund.");
        });
        QObject::connect(
            key.save, &QPushButton::clicked, status,
            [field = key.field, status] {
                const auto text = field->text().trimmed();
                field->clear();
                if (!badgebase::storeKey(text))
                {
                    status->setText("Das sieht nicht nach einem "
                                    "BadgeBase-Schlüssel aus.");
                    return;
                }
                status->setText("Gespeichert im Schlüsselbund. Jetzt Testen.");
            });
        QObject::connect(key.test, &QPushButton::clicked, status, [status] {
            status->setText("Frage BadgeBase …");
            const QPointer<QLabel> guard(status);
            badgebase::loadKey(status, [guard](const QString &stored) {
                if (guard.isNull())
                {
                    return;
                }
                if (stored.isEmpty())
                {
                    guard->setText("Kein Schlüssel gespeichert.");
                    return;
                }
                badgebase::get(
                    stored, "/badges?status=claimable", guard.data(),
                    [guard, stored](const QJsonObject &answer) {
                        if (guard.isNull())
                        {
                            return;
                        }
                        const auto all = badgebase::badgesIn(answer);
                        const auto base =
                            QStringLiteral("Der Schlüssel gilt - gerade %1 "
                                           "Badges zu holen")
                                .arg(all.size());
                        guard->setText(base + ".");
                        auto account =
                            getApp()->getAccounts()->twitch.getCurrent();
                        if (account == nullptr || account->isAnon())
                        {
                            return;
                        }
                        badgebase::get(
                            stored,
                            QStringLiteral("/user/%1/missing")
                                .arg(QString::fromUtf8(QUrl::toPercentEncoding(
                                    account->getUserName().toLower()))),
                            guard.data(),
                            [guard, base](const QJsonObject &answer) {
                                if (!guard.isNull())
                                {
                                    guard->setText(
                                        base +
                                        QStringLiteral(", dir fehlen davon "
                                                       "%1.")
                                            .arg(badgebase::badgesIn(answer)
                                                     .size()));
                                }
                            },
                            [](const QString &) {});
                    },
                    [guard](const QString &why) {
                        if (!guard.isNull())
                        {
                            guard->setText(why);
                        }
                    });
            });
        });
        QObject::connect(key.forget, &QPushButton::clicked, status, [status] {
            badgebase::eraseKey();
            status->setText("Schlüssel gelöscht.");
        });
    }

    addStandardButton(
        layout,
        "Was gemeldet wird und die Farben wieder so wie am Anfang - der "
        "Schlüssel bleibt",
        [&s] {
            for (auto *setting :
                 {&s.badgeAlertsAvailable, &s.badgeAlertsUpcoming,
                  &s.badgeAlertsEnding, &s.badgeAlertsOnlyMissing,
                  &s.badgeAlertsTwitch, &s.badgeAlertsSound,
                  &s.badgeAlertsColored, &s.badgeAlertsPaid})
            {
                setting->setValue(setting->getDefaultValue());
            }
            for (auto *sound :
                 {&s.badgeSoundAvailable, &s.badgeSoundUpcoming,
                  &s.badgeSoundEnding, &s.badgeSoundTwitch})
            {
                sound->setValue(sound->getDefaultValue());
            }
            for (auto *color : {&s.badgeColorAvailable, &s.badgeColorUpcoming,
                                &s.badgeColorEnding, &s.badgeColorTwitch})
            {
                color->setValue(color->getDefaultValue());
            }
        });
}

void BadgesPage::initSwitching(GeneralPageView &layout)
{
    layout.addTitle("Badge wechseln");
    layout.addDescription(
        "Wie im Chat auf twitch.tv wählen, welches Badge du trägst - mit dem "
        "Knopf „Badge wechseln“ in der Eingabezeile, einzuschalten unter "
        "Buttons. "
        "Twitch bietet das anderen Apps nicht an; es geht nur mit dem Login "
        "deines Browsers, so wie die Webseite selbst es macht. „Testen“ "
        "prüft, ob der Login taugt, und ändert dabei nichts.");
    layout.addDescription(
        "So kommst du an den Login: twitch.tv im Browser öffnen, angemeldet "
        "sein, mit Rechtsklick → Untersuchen die Entwicklertools öffnen, "
        "dort unter Speicher bzw. Anwendung → Cookies → twitch.tv den Wert "
        "von „auth-token“ kopieren.");
    layout.addDescription(
        "Der Wert ist wie ein Passwort: Füge ihn nur hier ein - nie in einen "
        "Chat, auf einer Webseite oder bei jemandem, der danach fragt. Er "
        "bleibt auf diesem Computer - im Schlüsselbund oder, wenn du es oben "
        "umstellst, verschlüsselt daneben - nie in den Einstellungen, einem "
        "Export, einem Backup oder dem Abgleich.");

    auto *field = new QLineEdit;
    field->setEchoMode(QLineEdit::Password);
    field->setPlaceholderText("auth-token hier einfügen");
    auto *save = new QPushButton("Speichern");
    auto *test = new QPushButton("Testen");
    auto *forget = new QPushButton("Login löschen");

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(field, 1);
    row->addWidget(save);
    row->addWidget(test);
    row->addWidget(forget);
    auto *rowWidget = new QWidget;
    rowWidget->setLayout(row);
    layout.addWidget(rowWidget, {"badge", "login", "browser", "auth-token"});

    auto *status = new QLabel;
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout.addWidget(status, {"badge"});

    if (!webbadges::canStore())
    {
        for (auto *widget :
             std::initializer_list<QWidget *>{field, save, test, forget})
        {
            widget->setEnabled(false);
        }
        status->setText("Geht nur mit dem Schlüsselbund des Systems - in der "
                        "portablen Version nicht.");
        return;
    }

    const auto showStored = [status] {
        webbadges::load(status, [status](const QString &token) {
            status->setText(token.isEmpty()
                                ? "Kein Browser-Login gespeichert."
                                : "Ein Browser-Login ist im Schlüsselbund.");
        });
    };
    showStored();

    QObject::connect(save, &QPushButton::clicked, status, [field, status] {
        const auto token = webbadges::normalize(field->text());
        field->clear();
        if (!webbadges::store(token))
        {
            status->setText("Das sieht nicht nach dem auth-token aus - "
                            "es sind 30 Buchstaben und Ziffern.");
            return;
        }
        status->setText("Gespeichert im Schlüsselbund. Jetzt Testen.");
    });
    QObject::connect(test, &QPushButton::clicked, status, [status] {
        status->setText("Frage Twitch …");
        webbadges::test(status, [status](const QString &report) {
            status->setText(report);
        });
    });
    QObject::connect(forget, &QPushButton::clicked, status, [status] {
        webbadges::erase();
        status->setText("Browser-Login gelöscht.");
    });
}

bool BadgesPage::filterElements(const QString &query)
{
    return this->view_->filterElements(query) || query.isEmpty();
}

}  // namespace chatterino
