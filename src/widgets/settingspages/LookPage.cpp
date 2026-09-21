// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/LookPage.hpp"

#include "Application.hpp"
#include "messages/layouts/AlternateBackground.hpp"
#include "messages/layouts/MessageRole.hpp"
#include "common/Channel.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/FuzzyConvert.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/helper/FontSettingWidget.hpp"
#include "widgets/NotebookEnums.hpp"
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
#include <type_traits>
#include <utility>

namespace chatterino {

namespace {

/// The zoom levels Chatterino offers, in its own order
const QStringList ZOOM_LEVELS = {
    "0.5x", "0.6x", "0.7x", "0.8x",  "0.9x",  "Default", "1.2x", "1.4x",
    "1.6x", "1.8x", "2x",   "2.33x", "2.66x", "3x",      "3.5x", "4x",
};

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

/// One made-up chat line for the preview: @a badges are the Twitch badges
/// its writer carries, which is what the role stripes go by
MessagePtr previewLine(const QString &name, const QString &text,
                       const QColor &color,
                       std::initializer_list<const char *> badges = {},
                       MessageFlag flag = MessageFlag::None)
{
    MessageBuilder builder;
    builder.emplace<TimestampElement>(QTime::currentTime());
    builder.emplace<TextElement>(name + ":", MessageElementFlag::Username,
                                 MessageColor(color), FontStyle::ChatMediumBold);
    builder.appendOrEmplaceText(text, MessageColor::Text);
    for (const auto *badge : badges)
    {
        builder->twitchBadges.emplace_back(badge, "1");
    }
    if (flag != MessageFlag::None)
    {
        builder->flags.set(flag);
    }
    builder->loginName = name.toLower();
    builder->displayName = name;
    builder->messageText = text;
    builder->searchText = name + ": " + text;
    builder->flags.set(MessageFlag::DoNotLog);
    return builder.release();
}

}  // namespace

LookPage::LookPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // A made-up chat above the switches: everything set here shows in it at
    // once, rather than after closing the window. Folded away for whoever
    // would rather have the room.
    auto *preview = new ChannelView(this, ChannelView::Context::UserCard, 12);
    preview->setFixedHeight(152);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto shown = std::make_shared<Channel>("vorschau", Channel::Type::None);
    preview->setChannel(shown);

    auto *fold = new QPushButton;
    fold->setFlat(true);
    fold->setCursor(Qt::PointingHandCursor);
    fold->setStyleSheet(
        QStringLiteral("QPushButton { border: none; background: transparent; "
                       "color: #9a9a9a; padding: 3px 6px; text-align: left; }"
                       "QPushButton:hover { color: #d0d0d0; }"));
    const auto refreshFold = [fold, preview] {
        const bool open = getSettings()->lookPreviewOpen;
        preview->setVisible(open);
        fold->setText(open ? QStringLiteral("▾  Beispiel-Chat")
                           : QStringLiteral("▸  Beispiel-Chat"));
        fold->setToolTip(open ? QStringLiteral("Den Beispiel-Chat einklappen")
                              : QStringLiteral("Zeigt an einem erfundenen "
                                               "Chat, was die Schalter hier "
                                               "bewirken"));
    };
    QObject::connect(fold, &QPushButton::clicked, this, [] {
        getSettings()->lookPreviewOpen.setValue(
            !getSettings()->lookPreviewOpen);
    });
    // Whoever changes it - the button here, a view being switched to -
    // the preview follows
    getSettings()->lookPreviewOpen.connect(
        [refreshFold](const bool, auto) {
            refreshFold();
        },
        this->managedConnections_, false);

    outer->addWidget(fold);
    outer->addWidget(preview);
    refreshFold();

    shown->addMessage(previewLine("Zarbex", "moin zusammen",
                                  QColor(0x5b, 0xc8, 0xff), {"broadcaster"}),
                      MessageContext::Original);
    shown->addMessage(previewLine("Mira", "erste Nachricht des Tages",
                                  QColor(0xff, 0x7f, 0x50)),
                      MessageContext::Original);
    shown->addMessage(previewLine("Mira", "und gleich noch eine",
                                  QColor(0xff, 0x7f, 0x50)),
                      MessageContext::Original);
    shown->addMessage(previewLine("Tom", "@fx_flexii schau mal her",
                                  QColor(0x8a, 0xe2, 0x34), {"moderator"}),
                      MessageContext::Original);
    shown->addMessage(previewLine("Kai", "hat 3 Monate abonniert!",
                                  QColor(0xd8, 0x7c, 0xff), {"subscriber"},
                                  MessageFlag::Subscription),
                      MessageContext::Original);

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

    layout.addTitle("Grundeinstellungen");
    layout.addDescription(
        "Farbschema, Schrift und Vergrößerung - Chatterinos eigene Schalter, "
        "hier gleich zur Hand. Sie stehen auch unter Allgemein -> Oberfläche "
        "und meinen dort dasselbe.");
    {
        auto *themes = getApp()->getThemes();
        auto available = themes->availableThemes();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        available.emplace_back("System", "System");
#endif
        SettingWidget::dropdown("Theme", themes->themeName, available)
            ->addTo(layout);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        SettingWidget::dropdown("Dark system theme",
                                themes->darkSystemThemeName,
                                themes->availableThemes())
            ->conditionallyEnabledBy(themes->themeName, "System")
            ->addTo(layout);
        SettingWidget::dropdown("Light system theme",
                                themes->lightSystemThemeName,
                                themes->availableThemes())
            ->conditionallyEnabledBy(themes->themeName, "System")
            ->addTo(layout);
#endif
    }

    layout.addWidget(new FontSettingWidget(s.chatFontFamily, s.chatFontSize,
                                           s.chatFontWeight),
                     {"font", "weight", "size", "Schrift", "Schriftgröße"});

    layout.addDropdown<float>(
        "Zoom", ZOOM_LEVELS, s.uiScale,
        [](auto val) {
            if (val == 1)
            {
                return QString("Default");
            }
            return QString::number(val) + "x";
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 1.F);
        });

    layout.addTitle("Aussehen");
    layout.addDescription(
        "Classic ist Chatterino, wie es immer aussah. Modern rundet ab und "
        "beruhigt: die Tabs mit etwas Tiefe, das Eingabefeld, den Griff der "
        "Bildlaufleiste, dazu ein durchgehend gezeichneter Symbolsatz in "
        "dieser Leiste hier und Alarm-Fenster, die sanft aufgehen statt "
        "aufzuspringen. Wirkt sofort, und zurück geht es jederzeit.");
    SettingWidget::dropdown("Stil", s.uiStyle)->addTo(layout);

    layout.addTitle("Fokus-Ansicht");
    layout.addDescription(
        "Blendet in einem Fenster die Tabs und die Knöpfe daneben aus - "
        "übrig bleiben die Chats mit ihrer Titelleiste und die Tab-Gruppen "
        "mit „Always Show Group“, etwa deine wichtigsten Kanäle. Ein- und "
        "ausschalten geht mit dem Knopf mit den vier Ecken unten in der "
        "Eingabezeile, neben dem Emote-Knopf - oder per Rechtsklick auf die "
        "Tab-Leiste. Das gilt nur für das Fenster, in dem du ihn drückst; "
        "jedes Fenster merkt sich das für sich, und jeder Computer auch.");

    layout.addStretch();
}

void LookPage::buildTabsTab(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Anordnung");
    layout.addDescription(
        "Wo die Tab-Leiste sitzt und welche Tabs sie zeigt - Chatterinos "
        "eigene Schalter, dieselben wie unter Allgemein -> Oberfläche.");
    layout.addDropdown<std::underlying_type_t<NotebookTabLocation>>(
        "Anordnung der Tabs", {"Oben", "Links", "Rechts", "Unten"},
        s.tabDirection,
        [](auto val) {
            switch (val)
            {
                case NotebookTabLocation::Left:
                    return "Links";
                case NotebookTabLocation::Right:
                    return "Rechts";
                case NotebookTabLocation::Bottom:
                    return "Unten";
                case NotebookTabLocation::Top:
                default:
                    return "Oben";
            }
        },
        [](auto args) {
            if (args.value == "Links")
            {
                return NotebookTabLocation::Left;
            }
            if (args.value == "Rechts")
            {
                return NotebookTabLocation::Right;
            }
            if (args.value == "Unten")
            {
                return NotebookTabLocation::Bottom;
            }
            return NotebookTabLocation::Top;
        },
        false);

    layout.addDropdown<std::underlying_type_t<NotebookTabVisibility>>(
        "Sichtbarkeit der Tabs", {"Alle Tabs", "Nur Live-Kanäle"},
        s.tabVisibility,
        [](auto val) {
            switch (val)
            {
                case NotebookTabVisibility::LiveOnly:
                    return "Nur Live-Kanäle";
                case NotebookTabVisibility::AllTabs:
                default:
                    return "Alle Tabs";
            }
        },
        [](auto args) {
            if (args.value == "Nur Live-Kanäle")
            {
                return NotebookTabVisibility::LiveOnly;
            }
            return NotebookTabVisibility::AllTabs;
        },
        false, "Welche Tabs in der Leiste zu sehen sind");

    SettingWidget::dropdown("Tab style", s.tabStyle)->addTo(layout);

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
        ->setTooltip("Eine kleine Kurve rechts im Split-Kopf: wie viel im "
                     "Chat los war. Ist der Kanal live, reicht sie über den "
                     "ganzen Stream, sonst über die letzte Viertelstunde. "
                     "Darunter eine Zeitachse, deren Striche je nach Länge "
                     "eine Minute bis mehrere Stunden auseinander liegen; "
                     "wo der Kanal die Kategorie gewechselt hat, steht ein "
                     "senkrechter Strich, und der Tooltip nennt Uhrzeit und "
                     "Kategorie. Was war, bevor du den Kanal geöffnet hast, "
                     "weiß niemand - der Anfang bleibt dann leer. Im "
                     "schmalen Split wird die Kurve kürzer, damit der Titel "
                     "bleibt.")
        ->addKeywords({"aktivität", "activity", "kurve", "graph"})
        ->addTo(layout);
    addStandardButton(layout, "Nur der Name, wie bisher", [&s] {
        s.splitHeaderPictures.setValue(false);
        s.splitHeaderActivity.setValue(false);
    });

    layout.addTitle("Aktiver Tab");
    layout.addDescription(
        "Der Tab, in dem du gerade bist, bekommt einen farbigen Rahmen - "
        "in derselben Farbe wie der aktive Split, damit beides zusammen "
        "gehört.");
    SettingWidget::checkbox("Rahmen um den Tab, in dem du bist",
                            s.activeTabBorder)
        ->addKeywords({"tab", "rahmen", "rand", "aktiv"})
        ->addTo(layout);
    SettingWidget::colorButton("Farbe des Rahmens", s.activeTabBorderColor)
        ->setTooltip("Leer heißt: dieselbe Farbe wie der Rand um den "
                     "aktiven Split.")
        ->conditionallyEnabledBy(s.activeTabBorder)
        ->addTo(layout);
    addStandardButton(layout, "Kein Rahmen um den aktiven Tab", [&s] {
        s.activeTabBorder.setValue(false);
        s.activeTabBorderColor.setValue("");
    });

    layout.addTitle("Zustände am Tab");
    SettingWidget::checkbox("Stumme Kanäle am Tab zeigen", s.tabMarkMuted)
        ->setTooltip("Ist in einem Kanal die Glocke aus, steht am Tab eine "
                     "kleine rote durchgestrichene Glocke - auf dem Profilbild "
                     "oder, ohne Bild, in der Ecke. So vergisst du keinen "
                     "Kanal, den du stumm geschaltet hast.")
        ->addKeywords({"glocke", "stumm", "alarm"})
        ->addTo(layout);
    SettingWidget::checkbox(
        "Offline-Kanäle in immer gezeigten Gruppen ausgrauen",
        s.tabDimOfflinePinned)
        ->setTooltip("In einer Tab-Gruppe mit „Always Show Group“ tritt ein "
                     "Kanal, der gerade nicht live ist, etwas zurück. Der Tab "
                     "bleibt, wo er ist; der, auf dem du gerade bist, bleibt "
                     "hell.")
        ->addKeywords({"offline", "grau", "gruppe"})
        ->addTo(layout);
    addStandardButton(layout, "Keine Zeichen am Tab, nichts ausgegraut",
                      [&s] {
                          s.tabMarkMuted.setValue(false);
                          s.tabDimOfflinePinned.setValue(false);
                      });

    layout.addTitle("Aktiver Split");
    layout.addDescription(
        "Hat ein Tab mehrere Chats nebeneinander, bekommt der, in den du "
        "gerade tippst, einen farbigen Rand - so schreibst du nie in den "
        "falschen.");
    SettingWidget::checkbox("Rand um den Split, in den du tippst",
                            s.activeSplitBorder)
        ->addKeywords({"rand", "border", "fokus", "aktiv"})
        ->addTo(layout);
    SettingWidget::colorButton("Farbe des Rands", s.activeSplitBorderColor)
        ->conditionallyEnabledBy(s.activeSplitBorder)
        ->addTo(layout);
    addStandardButton(layout, "Kein Rand, Farbe wie zu Beginn", [&s] {
        s.activeSplitBorder.setValue(false);
        s.activeSplitBorderColor.setValue(
            s.activeSplitBorderColor.getDefaultValue());
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

    layout.addTitle("Eingabefeld");
    SettingWidget::checkbox("Wartebalken unter dem Eingabefeld", s.slowModeBar)
        ->setTooltip("Hat ein Kanal Slow-Modus oder hast du einen Timeout, "
                     "läuft unter dem Eingabefeld ein Balken ab, bis du "
                     "wieder schreiben darfst - so wie im Twitch-Chat. Für "
                     "Mods und VIPs gilt der Slow-Modus nicht, dann bleibt "
                     "der Balken weg.")
        ->addKeywords({"slowmode", "slow mode", "timeout", "balken", "warten"})
        ->addTo(layout);
    SettingWidget::checkbox("Restzeit als Zahl rechts im Eingabefeld",
                            s.showSendWaitTimer)
        ->setTooltip("Derselbe Schalter wie General -> Chat -> Show countdown "
                     "on slow mode or when timed out.")
        ->addTo(layout);
    addStandardButton(layout, "Kein Balken, Restzeit wie bei Chatterino", [&s] {
        s.slowModeBar.setValue(false);
        s.showSendWaitTimer.setValue(s.showSendWaitTimer.getDefaultValue());
    });

    layout.addTitle("Profilbilder im Chat");
    SettingWidget::checkbox("Profilbild vor jedem Namen", s.chatAvatars)
        ->setTooltip("Ein kleines rundes Bild des Chatters vor seinem Namen. "
                     "Neue Nachrichten bekommen es ab dem Einschalten; "
                     "ausschalten blendet es überall sofort aus. Ein Klick "
                     "aufs Bild öffnet die User-Card.")
        ->addKeywords({"avatar", "profilbild", "bild"})
        ->addTo(layout);
    addStandardButton(layout, "Keine Profilbilder im Chat", [&s] {
        s.chatAvatars.setValue(false);
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
