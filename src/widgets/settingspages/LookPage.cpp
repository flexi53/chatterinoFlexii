// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/LookPage.hpp"

#include "messages/layouts/AlternateBackground.hpp"
#include "messages/layouts/MessageRole.hpp"
#include "singletons/Settings.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QComboBox>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <utility>

namespace chatterino {

namespace {

/// Which of the strengths on offer @a strength is, or is closest to
int strengthIndex(int strength)
{
    const auto &all = alternatebg::STRENGTHS;
    return int(std::min_element(all.begin(), all.end(),
                                [strength](int a, int b) {
                                    return std::abs(a - strength) <
                                           std::abs(b - strength);
                                }) -
               all.begin());
}

/// The room between messages on offer, in pixels
constexpr std::array<std::pair<const char *, int>, 4> SPACINGS{{
    {"Standard", 0},
    {"Etwas (2 px)", 2},
    {"Mehr (4 px)", 4},
    {"Viel (8 px)", 8},
}};

/// A button that puts a part of the page back to how it starts out
void addStandardButton(GeneralPageView &layout, const QString &tooltip,
                       std::function<void()> reset)
{
    auto *button = new QPushButton("Standard");
    button->setToolTip(tooltip);
    QObject::connect(button, &QPushButton::clicked, std::move(reset));
    layout.addWidget(button);
}

}  // namespace

LookPage::LookPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *tabs = new QTabWidget;
    outer->addWidget(tabs);

    const auto addTab = [this,
                         tabs](const QString &title) -> GeneralPageView & {
        auto *view = GeneralPageView::withoutNavigation(this);
        tabs->addTab(view, title);
        this->views_.push_back(view);
        return *view;
    };
    this->buildStyleTab(addTab("Stil"));
    this->buildTabsTab(addTab("Tabs"));
    this->buildChatTab(addTab("Chat"));
    this->buildColorsTab(addTab("Farben"));

    // Colour buttons grow into whatever room a short tab leaves them
    for (auto *view : this->views_)
    {
        for (auto *button : view->findChildren<ColorButton *>())
        {
            button->setFixedSize(50, 24);
            button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }
    }
}

bool LookPage::filterElements(const QString &query)
{
    bool any = query.isEmpty();
    for (auto *view : this->views_)
    {
        any = view->filterElements(query) || any;
    }
    return any;
}

void LookPage::buildStyleTab(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Aussehen");
    layout.addDescription(
        "Classic ist Chatterino, wie es immer aussah. Modern rundet die Tabs "
        "ab und gibt ihnen etwas Tiefe. Wirkt sofort.");
    SettingWidget::dropdown("Stil", s.uiStyle)->addTo(layout);

    layout.addTitle("Fokus-Ansicht");
    layout.addDescription(
        "Blendet in einem Fenster die Tabs, die Knöpfe daneben und die "
        "Split-Köpfe aus - übrig bleiben die Chats und die Tab-Gruppen mit "
        "„Always Show Group“, etwa deine wichtigsten Kanäle. Ein- und "
        "ausschalten geht mit dem Knopf mit den vier Ecken unten in der "
        "Eingabezeile, neben dem Emote-Knopf - oder per Rechtsklick auf die "
        "Tab-Leiste. Das gilt nur für das Fenster, in dem du ihn drückst; "
        "jedes Fenster merkt sich das für sich, und jeder Computer auch.");

    layout.addStretch();
}

void LookPage::buildTabsTab(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Tab-Leiste");
    layout.addDescription(
        "Der Bereich um die Tabs, hinter ihnen. Wirkt mit beiden Stilen.");
    SettingWidget::colorButton("Hintergrund", s.tabBarBackgroundColor)
        ->addTo(layout);
    SettingWidget::checkbox("Verlauf", s.tabBarGradient)
        ->setTooltip("Die Tab-Leiste mit einem Verlauf von oben nach unten "
                     "füllen statt mit einer Farbe.")
        ->addTo(layout);
    SettingWidget::colorButton("Verlauf oben", s.tabBarGradientTopColor)
        ->conditionallyEnabledBy(s.tabBarGradient)
        ->addTo(layout);
    SettingWidget::colorButton("Verlauf unten", s.tabBarGradientBottomColor)
        ->conditionallyEnabledBy(s.tabBarGradient)
        ->addTo(layout);

    layout.addTitle("Tabs");
    layout.addDescription(
        "Die Tabs selbst. Der ausgewählte Tab und Tabs mit neuen Nachrichten "
        "oder Highlights behalten ihre eigenen Farben, damit sie auffallen.");
    SettingWidget::colorButton("Hintergrund", s.tabBackgroundColor)
        ->addTo(layout);
    SettingWidget::colorButton("Ausgewählter Tab", s.tabSelectedBackgroundColor)
        ->addTo(layout);
    SettingWidget::checkbox("Verlauf", s.tabGradient)
        ->setTooltip("Die Tabs mit einem Verlauf von oben nach unten füllen. "
                     "Der ausgewählte Tab und Gruppenköpfe bleiben flach.")
        ->addTo(layout);
    SettingWidget::colorButton("Verlauf oben", s.tabGradientTopColor)
        ->conditionallyEnabledBy(s.tabGradient)
        ->addTo(layout);
    SettingWidget::colorButton("Verlauf unten", s.tabGradientBottomColor)
        ->conditionallyEnabledBy(s.tabGradient)
        ->addTo(layout);

    // Back to the theme for the tab bar and the tabs. The gradient colors are
    // kept, so switching a gradient on again brings back what was set up.
    addStandardButton(layout,
                      "Tab-Leiste und Tabs wieder in den Farben des "
                      "Themes",
                      [&s] {
                          s.tabBarBackgroundColor.setValue("");
                          s.tabBarGradient.setValue(false);
                          s.tabBackgroundColor.setValue("");
                          s.tabSelectedBackgroundColor.setValue("");
                          s.tabGradient.setValue(false);
                      });

    layout.addTitle("Profilbilder");
    layout.addDescription(
        "Das Profilbild des Kanals vor dem Namen jedes Tabs - bei mehreren "
        "Splits das des ersten. Tabs ohne Twitch-Kanal bleiben, wie sie sind.");
    SettingWidget::checkbox("Profilbilder in den Tabs", s.tabProfilePictures)
        ->addKeywords({"avatar", "bild"})
        ->addTo(layout);
    SettingWidget::checkbox("Live-Ring ums Profilbild statt Punkt",
                            s.tabLiveRing)
        ->setTooltip("Ist ein Kanal live, bekommt sein Bild im Tab einen "
                     "roten Ring - statt des kleinen Punkts in der Ecke.")
        ->conditionallyEnabledBy(s.tabProfilePictures)
        ->addKeywords({"live", "ring"})
        ->addTo(layout);
    addStandardButton(layout, "Tabs ohne Profilbild, Live mit Punkt", [&s] {
        s.tabProfilePictures.setValue(false);
        s.tabLiveRing.setValue(false);
    });

    layout.addTitle("Split-Kopf");
    layout.addDescription(
        "Die Leiste über jedem Chat, mit dem Namen des Kanals.");
    SettingWidget::checkbox("Profilbild und Kategorie zeigen",
                            s.splitHeaderPictures)
        ->setTooltip("Links im Split-Kopf das Bild des Kanals und, solange "
                     "er live ist, das Cover dessen, was er streamt.")
        ->addKeywords({"avatar", "spiel", "game", "kategorie", "cover"})
        ->addTo(layout);
    SettingWidget::checkbox("Aktivitäts-Kurve zeigen", s.splitHeaderActivity)
        ->setTooltip("Eine kleine Kurve rechts im Split-Kopf: wie viel in den "
                     "letzten zehn Minuten im Chat los war. Zählt ab dem "
                     "Einschalten.")
        ->addKeywords({"aktivität", "activity", "kurve", "graph"})
        ->addTo(layout);
    addStandardButton(layout, "Nur der Name, wie bisher", [&s] {
        s.splitHeaderPictures.setValue(false);
        s.splitHeaderActivity.setValue(false);
    });

    layout.addStretch();
}

void LookPage::buildChatTab(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Lesbarkeit");
    layout.addDescription(
        "Jede zweite Nachricht bekommt einen etwas anderen Hintergrund, damit "
        "man die Nachrichten auch im schnellen Chat auseinanderhält. Wie "
        "deutlich und in welcher Farbe, stellst du hier ein; es wirkt sofort.");

    SettingWidget::checkbox("Jede zweite Nachricht absetzen",
                            s.alternateMessages)
        ->setTooltip("Derselbe Schalter wie General -> Messages -> Alternate "
                     "background color.")
        ->addKeywords({"alternate", "background", "Hintergrund"})
        ->addTo(layout);

    auto *strength = layout.addDropdown<int>(
        "Stärke", {"Wie im Theme", "Dezent", "Mittel", "Deutlich", "Stark"},
        s.alternateMessageStrength,
        [](int value) {
            return strengthIndex(value);
        },
        [](const DropdownArgs &args) {
            return alternatebg::STRENGTHS.at(std::clamp(
                args.index, 0, int(alternatebg::STRENGTHS.size()) - 1));
        },
        false,
        "Wie weit sich jede zweite Nachricht abhebt. „Wie im Theme“ ist der "
        "leichte Grauton, den das Theme mitbringt.");

    SettingWidget::colorButton("Farbton", s.alternateMessageTint)
        ->setTooltip("Leer ist neutral: heller im dunklen Theme, dunkler im "
                     "hellen. Mit einer Farbe - etwa einem leichten Violett - "
                     "wird jede zweite Nachricht darin eingefärbt, so stark "
                     "wie oben gewählt.")
        ->conditionallyEnabledBy(s.alternateMessages)
        ->addTo(layout);

    SettingWidget::checkbox("Nur wechseln, wenn jemand anderes schreibt",
                            s.alternateMessagesBySender)
        ->setTooltip(
            "Schreibt jemand mehrere Nachrichten hintereinander, behalten sie "
            "denselben Hintergrund und lesen sich wie ein Block. Der "
            "Hintergrund wechselt erst, wenn jemand anderes schreibt.")
        ->conditionallyEnabledBy(s.alternateMessages)
        ->addTo(layout);

    s.alternateMessages.connect(
        [strength](const bool &on, auto) {
            strength->setEnabled(on);
        },
        this->managedConnections_);

    addStandardButton(layout,
                      "Der Grauton des Themes, ohne Farbe, bei jeder "
                      "Nachricht wechselnd",
                      [&s] {
                          s.alternateMessageStrength.setValue(0);
                          s.alternateMessageTint.setValue("");
                          s.alternateMessagesBySender.setValue(false);
                      });

    layout.addTitle("Nachrichten");
    layout.addDropdown<int>(
        "Abstand zwischen Nachrichten",
        [] {
            QStringList names;
            for (const auto &[name, pixels] : SPACINGS)
            {
                names.append(name);
            }
            return names;
        }(),
        s.messageSpacing,
        [](int value) {
            for (size_t i = 0; i < SPACINGS.size(); i++)
            {
                if (SPACINGS.at(i).second == value)
                {
                    return int(i);
                }
            }
            return 0;
        },
        [](const DropdownArgs &args) {
            return SPACINGS
                .at(std::clamp(args.index, 0, int(SPACINGS.size()) - 1))
                .second;
        },
        false, "Etwas mehr Luft über und unter jeder Nachricht.");
    SettingWidget::checkbox("Nachricht unter der Maus hervorheben",
                            s.hoverHighlight)
        ->setTooltip("Hellt die Nachricht unter dem Mauszeiger leicht auf - "
                     "so verrutschst du in langen Zeilen nicht und siehst, "
                     "welche Nachricht du gleich anklickst.")
        ->addTo(layout);
    SettingWidget::colorButton("Farbe der Hervorhebung", s.hoverHighlightColor)
        ->setTooltip("Leer ist ein leichter Schimmer, der zum Theme passt.")
        ->conditionallyEnabledBy(s.hoverHighlight)
        ->addTo(layout);
    SettingWidget::checkbox("Neue Nachrichten sanft einblenden",
                            s.fadeInMessages)
        ->setTooltip("Neue Nachrichten erscheinen nicht schlagartig, sondern "
                     "blenden weich ein und gleiten an ihren Platz.")
        ->addTo(layout);
    SettingWidget::checkbox("Chat weich nachrutschen lassen",
                            s.enableSmoothScrollingNewMessages)
        ->setTooltip("Kommt eine Nachricht dazu, rutscht der Chat weich nach "
                     "oben, statt um eine Zeile zu springen - zusammen mit "
                     "dem Einblenden am ruhigsten. Derselbe Schalter wie "
                     "General -> Smooth scrolling on new messages.")
        ->addKeywords({"smooth", "scroll"})
        ->addTo(layout);
    addStandardButton(layout,
                      "Kein zusätzlicher Abstand, keine Hervorhebung, "
                      "kein Einblenden oder Nachrutschen",
                      [&s] {
                          s.messageSpacing.setValue(0);
                          s.hoverHighlight.setValue(false);
                          s.hoverHighlightColor.setValue("");
                          s.fadeInMessages.setValue(false);
                          s.enableSmoothScrollingNewMessages.setValue(false);
                      });

    layout.addTitle("Ereignisse und Erwähnungen");
    SettingWidget::checkbox("Ereignisse mit Symbol und Farbe markieren",
                            s.eventSymbols)
        ->setTooltip("Subs ⭐, Gifts 🎁, Raids 🚀, Ankündigungen 📣, "
                     "Timeouts ⏱️, Banns 🔨, Bits 💎, eingelöste Punkte 🎟️ "
                     "und Watch-Streaks 🔥 bekommen vorne ein Symbol und links "
                     "einen Streifen in ihrer Farbe.")
        ->addKeywords({"sub", "raid", "bann", "ban", "timeout", "symbol"})
        ->addTo(layout);
    SettingWidget::checkbox("Erwähnungen kurz aufleuchten lassen",
                            s.pulseMentions)
        ->setTooltip("Erwähnt dich jemand, leuchtet die Nachricht einmal "
                     "sanft auf, wenn sie reinkommt.")
        ->addKeywords({"mention", "erwähnung", "leuchten"})
        ->addTo(layout);
    addStandardButton(layout, "Keine Symbole, kein Aufleuchten", [&s] {
        s.eventSymbols.setValue(false);
        s.pulseMentions.setValue(false);
    });

    layout.addTitle("Rollen-Streifen");
    layout.addDescription(
        "Ein schmaler farbiger Streifen links an jeder Nachricht zeigt, wer "
        "schreibt - ohne auf die Badges zu schauen. Hast du unter Highlights "
        "-> Badges eine Farbe für die Rolle, nimmt der Streifen die von selbst "
        "und das Feld hier ist ausgegraut. Eine Rolle ohne Farbe (oder ganz "
        "durchsichtig) bekommt keinen Streifen.");
    SettingWidget::checkbox("Rollen-Streifen anzeigen", s.roleStripes)
        ->addKeywords({"mod", "vip", "sub", "streamer", "rolle", "badge"})
        ->addTo(layout);
    this->addRoleColor(layout, "Streamer", ChatRole::Broadcaster,
                       s.roleStripeBroadcaster);
    this->addRoleColor(layout, "Mods", ChatRole::Moderator,
                       s.roleStripeModerator);
    this->addRoleColor(layout, "VIPs", ChatRole::Vip, s.roleStripeVip);
    this->addRoleColor(layout, "Subs", ChatRole::Subscriber,
                       s.roleStripeSubscriber);
    addStandardButton(layout, "Streifen aus, Farben wie zu Beginn", [&s] {
        s.roleStripes.setValue(false);
        for (auto *setting : {&s.roleStripeBroadcaster, &s.roleStripeModerator,
                              &s.roleStripeVip, &s.roleStripeSubscriber})
        {
            setting->setValue(setting->getDefaultValue());
        }
    });

    layout.addStretch();
}

void LookPage::addRoleColor(GeneralPageView &layout, const QString &name,
                            ChatRole role, QStringSetting &setting)
{
    auto *label = new QLabel(name + ":");
    auto *note = new QLabel;
    note->setStyleSheet("color: #8a8a8a;");
    auto *button = new ColorButton(QColor(setting.getValue()));
    button->setFixedSize(50, 24);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *row = new QHBoxLayout;
    row->addWidget(label);
    row->addSpacing(6);
    row->addWidget(note);
    row->addStretch(1);
    row->addWidget(button);
    layout.addLayout(row);

    // A badge highlight for the role wins - its colour shows, greyed out
    const auto refresh = [label, note, button, role, &setting] {
        const auto on = getSettings()->roleStripes.getValue();
        const auto fromBadge = badgeStripeColor(
            role, *getSettings()->highlightedBadges.readOnly());
        label->setEnabled(on);
        note->setEnabled(on);
        // Greyed out rather than just disabled, which it does not show
        if (fromBadge || !on)
        {
            auto *faded = new QGraphicsOpacityEffect;
            faded->setOpacity(0.4);
            button->setGraphicsEffect(faded);
        }
        else
        {
            button->setGraphicsEffect(nullptr);
        }
        if (fromBadge)
        {
            button->setColor(*fromBadge);
            button->setEnabled(false);
            button->setToolTip("Die Farbe kommt aus Highlights -> Badges. "
                               "Ändern oder löschen kannst du sie dort.");
            note->setText("aus Highlights -> Badges");
        }
        else
        {
            button->setColor(QColor(setting.getValue()));
            button->setEnabled(on);
            button->setToolTip({});
            note->clear();
        }
    };

    QObject::connect(button, &ColorButton::clicked, [button, &setting] {
        auto *dialog = new ColorPickerDialog(QColor(setting), button);
        QObject::connect(
            dialog, &ColorPickerDialog::colorConfirmed, button,
            [&setting](const QColor &selected) {
                if (selected.isValid())
                {
                    setting.setValue(selected.name(QColor::HexArgb));
                }
            });
        dialog->show();
    });
    setting.connect(
        [refresh](const auto &, const auto &) {
            refresh();
        },
        this->managedConnections_, false);
    getSettings()->roleStripes.connect(
        [refresh](const auto &, const auto &) {
            refresh();
        },
        this->managedConnections_, false);
    this->managedConnections_.managedConnect(
        getSettings()->highlightedBadges.delayedItemsChanged, refresh);
    refresh();
}

void LookPage::buildColorsTab(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Eigenes Farbschema");
    layout.addDescription(
        "Deine eigenen Farben über dem Theme (Dark, Light ...). Eine Farbe, "
        "die du nicht setzt, bleibt wie im Theme. Ausschalten bringt das Theme "
        "zurück - deine Farben bleiben für später gespeichert.");
    SettingWidget::checkbox("Eigene Farben verwenden", s.customColors)
        ->addKeywords({"theme", "farbe", "color"})
        ->addTo(layout);

    const std::array<std::pair<const char *, QStringSetting *>, 7> colors{{
        {"Chat-Hintergrund", &s.customColorBackground},
        {"Text", &s.customColorText},
        {"Systemtext", &s.customColorSystemText},
        {"Links", &s.customColorLink},
        {"Akzentfarbe", &s.customColorAccent},
        {"Split-Kopf", &s.customColorHeader},
        {"Eingabefeld", &s.customColorInput},
    }};
    for (const auto &[name, setting] : colors)
    {
        SettingWidget::colorButton(name, *setting)
            ->conditionallyEnabledBy(s.customColors)
            ->addTo(layout);
    }

    addStandardButton(layout,
                      "Das Theme, wie es kommt - eigene Farben aus und "
                      "gelöscht",
                      [&s, colors] {
                          s.customColors.setValue(false);
                          for (const auto &[name, setting] : colors)
                          {
                              setting->setValue("");
                          }
                      });

    layout.addStretch();
}

}  // namespace chatterino
