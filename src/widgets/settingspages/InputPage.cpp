// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/InputPage.hpp"

#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "widgets/settingspages/SettingWidget.hpp"
#include "widgets/splits/InputButtons.hpp"

#include <QAbstractItemModel>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
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

/// The buttons of the input bar: which are shown, and in which order. Ticked
/// here they appear, and the two arrows move the picked one about.
void addButtonList(GeneralPageView &layout, QWidget *parent)
{
    auto &s = *getSettings();

    const auto settingFor = [&s](const QString &key) -> BoolSetting * {
        if (key == inputbuttons::MOD_ASSIST)
        {
            return &s.showModAssistButton;
        }
        if (key == inputbuttons::ALERT_MUTE)
        {
            return &s.showAlertMuteButton;
        }
        if (key == inputbuttons::CLEAR)
        {
            return &s.showClearChatButton;
        }
        if (key == inputbuttons::FOCUS)
        {
            return &s.showFocusButton;
        }
        if (key == inputbuttons::FOLLOW)
        {
            return &s.showFollowBrowserButton;
        }
        if (key == inputbuttons::BADGE)
        {
            return &s.showBadgeButton;
        }
        if (key == inputbuttons::CLIP)
        {
            return &s.showClipButton;
        }
        if (key == inputbuttons::EMOTE)
        {
            return &s.showEmoteButton;
        }
        return nullptr;
    };

    const auto tooltipFor = [](const QString &key) -> QString {
        if (key == inputbuttons::MOD_ASSIST)
        {
            return "Öffnet das Fenster, in dem du für diesen Kanal "
                   "einstellst, worauf der Mod-Assistent achtet. Erscheint "
                   "nur, wo du Mod oder Streamer bist.";
        }
        if (key == inputbuttons::ALERT_MUTE)
        {
            return "Schaltet die Alarm-Fenster stumm, ohne etwas an den "
                   "Einstellungen zu ändern. Erscheint nur, wo du Mod oder "
                   "Streamer bist.";
        }
        if (key == inputbuttons::CLEAR)
        {
            return "Leert diesen Chat hier bei dir, wie „Clear messages“. "
                   "Für alle anderen bleibt alles, wie es ist.";
        }
        if (key == inputbuttons::FOCUS)
        {
            return "Blendet Tabs und Knöpfe aus und wieder ein - nur in "
                   "diesem Fenster.";
        }
        if (key == inputbuttons::FOLLOW)
        {
            return "Schaltet „Aussehen → Tabs → Dem Browser folgen“ an und "
                   "aus. Gilt für das ganze Programm, wie die "
                   "Fokus-Ansicht; geschlossene Kette heißt: der Tab folgt.";
        }
        if (key == inputbuttons::BADGE)
        {
            return "Zeigt, welches Badge du im Kanal trägst, und lässt dich "
                   "wie auf twitch.tv ein anderes wählen. Braucht den "
                   "Browser-Login unter Einstellungen → Badges.";
        }
        if (key == inputbuttons::CLIP)
        {
            return "Schneidet die letzte halbe Minute des Streams mit - "
                   "dasselbe wie Alt+X. Geht nur, solange der Kanal live "
                   "ist.";
        }
        if (key == inputbuttons::EMOTE)
        {
            return "Öffnet die Liste der Emotes dieses Kanals.";
        }
        return {};
    };

    auto *list = new QListWidget;
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    // Room for all of them, so nothing has to be scrolled to
    list->setFixedHeight(list->fontMetrics().lineSpacing() * 9 + 16);

    auto *up = new QPushButton("Hoch");
    auto *down = new QPushButton("Runter");
    up->setToolTip("Den ausgewählten Knopf eine Stelle nach links");
    down->setToolTip("Den ausgewählten Knopf eine Stelle nach rechts");

    const auto fill = [list, settingFor, tooltipFor](const QString &keep) {
        const QSignalBlocker blocker(list);
        list->clear();
        for (const auto &key : inputbuttons::order())
        {
            auto *item = new QListWidgetItem(inputbuttons::nameOf(key));
            item->setData(Qt::UserRole, key);
            item->setToolTip(tooltipFor(key));
            const auto *setting = settingFor(key);
            item->setCheckState(setting != nullptr && setting->getValue()
                                    ? Qt::Checked
                                    : Qt::Unchecked);
            list->addItem(item);
            if (key == keep)
            {
                list->setCurrentItem(item);
            }
        }
    };
    fill({});

    QObject::connect(
        list, &QListWidget::itemChanged, parent,
        [settingFor](QListWidgetItem *item) {
            auto *setting = settingFor(item->data(Qt::UserRole).toString());
            if (setting != nullptr)
            {
                setting->setValue(item->checkState() == Qt::Checked);
            }
        });

    const auto moveBy = [list, fill](bool up) {
        auto *item = list->currentItem();
        if (item == nullptr)
        {
            return;
        }
        const auto key = item->data(Qt::UserRole).toString();
        if (inputbuttons::move(key, up))
        {
            fill(key);
        }
    };
    QObject::connect(up, &QPushButton::clicked, parent, [moveBy] {
        moveBy(true);
    });
    QObject::connect(down, &QPushButton::clicked, parent, [moveBy] {
        moveBy(false);
    });

    // Ticked or moved from somewhere else - another window, the Standard
    // button - the list says what holds
    getSettings()->inputButtonOrder.connect(
        [list, fill](const QString &, auto) {
            auto *item = list->currentItem();
            fill(item == nullptr ? QString()
                                 : item->data(Qt::UserRole).toString());
        },
        false);

    layout.addWidget(list, {"knopf", "button", "reihenfolge", "sortieren"});

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(up);
    row->addWidget(down);
    row->addStretch(1);
    auto *rowWidget = new QWidget;
    rowWidget->setLayout(row);
    layout.addWidget(rowWidget, {"reihenfolge", "hoch", "runter"});
}

}  // namespace

InputPage::InputPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    this->view_ = GeneralPageView::withoutNavigation(this);
    outer->addWidget(this->view_);

    this->initLayout(*this->view_);
}

void InputPage::initLayout(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Unten in der Eingabezeile");
    layout.addDescription(
        "Die kleine Reihe rechts neben dem Eingabefeld. Was du hier "
        "ausschaltest, ist nicht weg - es steht weiter im Menü des Splits "
        "oder in den Einstellungen.");

    addButtonList(layout, this);

    SettingWidget::checkbox("Senden", s.showSendButton)
        ->setTooltip("Schickt die getippte Nachricht ab - dasselbe wie die "
                     "Eingabetaste. Derselbe Schalter wie General -> Show "
                     "send message button. Steht im Eingabefeld selbst, "
                     "nicht in der Reihe oben.")
        ->addTo(layout);
    SettingWidget::checkbox("Link gleich kopieren", s.clipCopyLink)
        ->setTooltip("Sobald der Clip da ist, liegt sein Link in der "
                     "Zwischenablage - du musst ihn nicht erst anklicken. "
                     "Gilt für jeden Clip, auch über Alt+X.")
        ->addKeywords({"clip", "kopieren", "link", "zwischenablage"})
        ->addTo(layout);
    SettingWidget::checkbox("Zum Bearbeiten im Browser öffnen",
                            s.clipOpenEditor)
        ->setTooltip("Öffnet den Clip gleich auf twitch.tv, wo du ihn "
                     "zuschneiden und benennen kannst. Gilt für jeden Clip, "
                     "auch über Alt+X.")
        ->addKeywords({"clip", "browser", "bearbeiten"})
        ->addTo(layout);

    layout.addDescription(
        "„Mod-Assistent“ und „Alarme stumm“ erscheinen ohnehin nur in "
        "Kanälen, in denen du Mod oder Streamer bist - sonst könnten sie "
        "nichts ausrichten. „Badge wechseln“ nur dort, wo du eingeloggt "
        "schreiben kannst, „Clip erstellen“ nur in Twitch-Kanälen.");

    addStandardButton(
        layout, "Alle Knöpfe wieder so, wie sie am Anfang sind", [&s] {
            s.showEmoteButton.setValue(s.showEmoteButton.getDefaultValue());
            s.showSendButton.setValue(s.showSendButton.getDefaultValue());
            s.showClearChatButton.setValue(
                s.showClearChatButton.getDefaultValue());
            s.showFocusButton.setValue(s.showFocusButton.getDefaultValue());
            s.showModAssistButton.setValue(
                s.showModAssistButton.getDefaultValue());
            s.showAlertMuteButton.setValue(
                s.showAlertMuteButton.getDefaultValue());
            s.showBadgeButton.setValue(s.showBadgeButton.getDefaultValue());
            s.showClipButton.setValue(s.showClipButton.getDefaultValue());
            s.showFollowBrowserButton.setValue(
                s.showFollowBrowserButton.getDefaultValue());
            s.inputButtonOrder.setValue(s.inputButtonOrder.getDefaultValue());
            s.clipCopyLink.setValue(s.clipCopyLink.getDefaultValue());
            s.clipOpenEditor.setValue(s.clipOpenEditor.getDefaultValue());
        });

    layout.addTitle("Befehle beim Tippen");
    SettingWidget::checkbox("Befehle vorschlagen", s.commandSuggestions)
        ->setTooltip("Tippst du einen Schrägstrich, stehen die passenden "
                     "Befehle über der Eingabezeile - dieselbe Liste, die "
                     "die Tabulatortaste durchgeht.")
        ->addKeywords({"befehl", "vorschlag", "command", "slash"})
        ->addTo(layout);
    SettingWidget::checkbox("Emotes ohne Doppelpunkt vorschlagen",
                            s.emoteSuggestionsWithoutColon)
        ->setTooltip("Schon ab dem dritten Buchstaben eines Wortes stehen "
                     "die passenden Emotes über der Eingabezeile - ohne "
                     "dass ein „:“ davor muss. Passt nichts, bleibt das "
                     "Fenster weg, und solange du nichts auswählst, bleibt "
                     "dein Wort so stehen, wie du es getippt hast.")
        ->addKeywords({"emote", "vorschlag", "doppelpunkt", "autocomplete"})
        ->addTo(layout);
    SettingWidget::checkbox("Eingabetaste sendet immer", s.enterAlwaysSends)
        ->setTooltip("Steht ein Vorschlag über der Eingabezeile, schickt "
                     "die Eingabetaste trotzdem die Nachricht ab - den "
                     "Vorschlag holt die Tabulatortaste. Für "
                     "Tastenmakros, die ein Wort tippen und gleich Enter "
                     "hinterherschicken, ist das der Unterschied zwischen "
                     "gesendet und nur vervollständigt. Aus heißt: "
                     "wie in Chatterino, dann übernimmt die "
                     "Eingabetaste den Vorschlag.")
        ->addKeywords({"enter", "eingabetaste", "senden", "tab", "vorschlag"})
        ->addTo(layout);
    SettingWidget::checkbox("Nur zeigen, was hier geht",
                            s.hideUnavailableCommands)
        ->setTooltip("Befehle, für die man Moderator sein muss, bleiben in "
                     "Kanälen weg, in denen du keiner bist - bei vielen "
                     "Kanälen ist sonst die halbe Liste ohne Nutzen.")
        ->addKeywords({"befehl", "mod", "moderator", "ausblenden"})
        ->addTo(layout);

    addStandardButton(
        layout, "Vorschläge und Eingabetaste wie am Anfang", [&s] {
            s.commandSuggestions.setValue(
                s.commandSuggestions.getDefaultValue());
            s.emoteSuggestionsWithoutColon.setValue(
                s.emoteSuggestionsWithoutColon.getDefaultValue());
            s.enterAlwaysSends.setValue(s.enterAlwaysSends.getDefaultValue());
            s.hideUnavailableCommands.setValue(
                s.hideUnavailableCommands.getDefaultValue());
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

    layout.addStretch();
}

bool InputPage::filterElements(const QString &query)
{
    return this->view_ != nullptr && this->view_->filterElements(query);
}

}  // namespace chatterino
