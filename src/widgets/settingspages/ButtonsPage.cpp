// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ButtonsPage.hpp"

#include "singletons/Settings.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
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

ButtonsPage::ButtonsPage()
    : view_(GeneralPageView::withoutNavigation(this))
{
    auto *outer = new QVBoxLayout;
    auto *row = new QHBoxLayout;
    row->addWidget(this->view_);
    auto *frame = new QFrame;
    frame->setLayout(row);
    outer->addWidget(frame);
    this->setLayout(outer);

    this->initLayout(*this->view_);
}

void ButtonsPage::initLayout(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("Unten in der Eingabezeile");
    layout.addDescription(
        "Die kleine Reihe rechts neben dem Eingabefeld. Was du hier "
        "ausschaltest, ist nicht weg - es steht weiter im Menü des Splits "
        "oder in den Einstellungen.");

    SettingWidget::checkbox("Emotes", s.showEmoteButton)
        ->setTooltip("Öffnet die Liste der Emotes dieses Kanals.")
        ->addKeywords({"emote", "knopf", "button"})
        ->addTo(layout);
    SettingWidget::checkbox("Senden", s.showSendButton)
        ->setTooltip("Schickt die getippte Nachricht ab - dasselbe wie die "
                     "Eingabetaste. Derselbe Schalter wie General -> Show "
                     "send message button.")
        ->addTo(layout);
    SettingWidget::checkbox("Chat leeren", s.showClearChatButton)
        ->setTooltip("Leert diesen Chat hier bei dir, wie „Clear messages“. "
                     "Für alle anderen bleibt alles, wie es ist.")
        ->addKeywords({"clear", "mülleimer", "leeren"})
        ->addTo(layout);
    SettingWidget::checkbox("Fokus-Ansicht", s.showFocusButton)
        ->setTooltip("Blendet Tabs, Knöpfe und Split-Köpfe aus und wieder "
                     "ein - nur in diesem Fenster.")
        ->addKeywords({"fokus", "focus"})
        ->addTo(layout);

    layout.addDescription(
        "Die nächsten beiden erscheinen ohnehin nur in Kanälen, in denen du "
        "Mod oder Streamer bist - sonst könnten sie nichts ausrichten.");
    SettingWidget::checkbox("Mod-Assistent (Schild)", s.showModAssistButton)
        ->setTooltip("Öffnet das Fenster, in dem du für diesen Kanal "
                     "einstellst, worauf der Mod-Assistent achtet.")
        ->addKeywords({"schild", "shield", "mod"})
        ->addTo(layout);
    SettingWidget::checkbox("Alarme stumm (Glocke)", s.showAlertMuteButton)
        ->setTooltip("Schaltet die Alarm-Fenster stumm, ohne etwas an den "
                     "Einstellungen zu ändern. Ist die Glocke aus und die "
                     "Alarme sind gerade stumm, bleiben sie es - schalte sie "
                     "vorher wieder an.")
        ->addKeywords({"glocke", "stumm", "alarm"})
        ->addTo(layout);

    addStandardButton(layout, "Alle Knöpfe wieder so, wie sie am Anfang sind",
                      [&s] {
                          s.showEmoteButton.setValue(
                              s.showEmoteButton.getDefaultValue());
                          s.showSendButton.setValue(
                              s.showSendButton.getDefaultValue());
                          s.showClearChatButton.setValue(
                              s.showClearChatButton.getDefaultValue());
                          s.showFocusButton.setValue(
                              s.showFocusButton.getDefaultValue());
                          s.showModAssistButton.setValue(
                              s.showModAssistButton.getDefaultValue());
                          s.showAlertMuteButton.setValue(
                              s.showAlertMuteButton.getDefaultValue());
                      });

    layout.addTitle("Oben in der Tab-Leiste");
    layout.addDescription(
        "Die Knöpfe links neben den Tabs und das Kreuz am Tab selbst. Das "
        "sind Chatterinos eigene Schalter, hier gleich zur Hand.");
    SettingWidget::inverseCheckbox("Einstellungen (Zahnrad)",
                                   s.hidePreferencesButton)
        ->setTooltip("Ohne ihn kommst du mit ⌘P in die Einstellungen.")
        ->addTo(layout);
    SettingWidget::inverseCheckbox("Eigenes Konto", s.hideUserButton)
        ->setTooltip("Ohne ihn wechselst du das Konto über die "
                     "Einstellungen -> Konten.")
        ->addTo(layout);
    SettingWidget::checkbox("Kreuz zum Schließen am Tab", s.showTabCloseButton)
        ->setTooltip("Ohne es schließt du einen Tab über sein Rechtsklick-"
                     "Menü.")
        ->addTo(layout);

    addStandardButton(layout, "Wieder so, wie Chatterino es zeigt", [&s] {
        s.hidePreferencesButton.setValue(
            s.hidePreferencesButton.getDefaultValue());
        s.hideUserButton.setValue(s.hideUserButton.getDefaultValue());
        s.showTabCloseButton.setValue(
            s.showTabCloseButton.getDefaultValue());
    });

    layout.addStretch();
}

bool ButtonsPage::filterElements(const QString &query)
{
    if (this->view_ != nullptr)
    {
        return this->view_->filterElements(query) || query.isEmpty();
    }
    return false;
}

int ButtonsPage::changedSettings()
{
    return this->view_ != nullptr ? this->view_->countChanged() : -1;
}

void ButtonsPage::showOnlyChanged(bool only)
{
    if (this->view_ != nullptr)
    {
        this->view_->showOnlyChanged(only);
    }
}

}  // namespace chatterino
