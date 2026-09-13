// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ModAssistantPage.hpp"

#include "Application.hpp"
#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/FormatTime.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"
#include "widgets/Window.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace chatterino {

ModAssistantPage::ModAssistantPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // Everything the assistant and its alerts can be told outgrows the
    // window, so the page scrolls instead of squeezing its rows together
    auto *assistantScroll = new QScrollArea;
    assistantScroll->setWidgetResizable(true);
    assistantScroll->setFrameShape(QFrame::NoFrame);
    assistantScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    assistantScroll->viewport()->setAutoFillBackground(false);
    auto *assistantContent = new QWidget;
    assistantContent->setAutoFillBackground(false);
    auto *assistantLayout = new QVBoxLayout(assistantContent);
    assistantScroll->setWidget(assistantContent);
    outer->addWidget(assistantScroll);

    LayoutCreator<QVBoxLayout> assistant(assistantLayout);
    {
        auto *intro = new QLabel(
            "Der Moderations-Assistent lernt aus Timeouts und Banns in "
            "Kanälen, in denen du Mod bist, und schlägt eine Aktion vor, wenn "
            "jemand etwas Ähnliches schreibt. Eingeschaltet wird er pro Kanal "
            "über den Schild-Knopf neben dem Emote-Knopf. Ein Vorschlag öffnet "
            "ein Fenster mit der Aktion, die Mods meistens gegeben haben – es "
            "passiert nichts, solange du nicht auf den Knopf drückst.");
        intro->setWordWrap(true);
        assistant.append(intro);

        auto *alertsIntro = new QLabel(
            "<br><b>Alarm-Fenster</b><br>Gilt für alle Fenster weiter unten: "
            "Vorschläge, wiederholte Nachrichten und Emote-Spam.");
        alertsIntro->setTextFormat(Qt::RichText);
        alertsIntro->setWordWrap(true);
        assistant.append(alertsIntro);

        assistant.append(this->createCheckBox(
            "Immer im Vordergrund, auch über anderen Programmen",
            getSettings()->modAlertAlwaysOnTop,
            "Das Fenster liegt über jedem Programm, auch wenn gerade der "
            "Browser oder ein Spiel vorne ist, und nimmt dir trotzdem nicht "
            "den Fokus. Gilt für Fenster, die ab jetzt aufgehen."));
        assistant.append(this->createCheckBox(
            "Ton abspielen, wenn ein Alarm-Fenster aufgeht",
            getSettings()->modAlertSound,
            "Spielt den Hinweiston, sobald ein neues Alarm-Fenster aufgeht - "
            "nicht, wenn ein offenes nur aktualisiert wird."));

        auto *alertsForm = new QFormLayout;
        auto *autoClose =
            this->createSpinBox(getSettings()->repeatAlertAutoClose, 0, 300);
        autoClose->setSuffix(" s");
        autoClose->setSpecialValueText("nie");
        autoClose->setToolTip(
            "Nach dieser Zeit schließt sich das Fenster von selbst. Solange die "
            "Maus darüber ist, bleibt es offen.");
        alertsForm->addRow("Fenster schließt sich von selbst nach", autoClose);

        const auto sizeHint = QStringLiteral(
            "Du kannst ein Alarm-Fenster auch einfach an der Ecke unten rechts "
            "größer ziehen - die Größe wird beim Schließen übernommen und gilt "
            "für alle weiteren Fenster. Bei \"automatisch\" wählt das Fenster "
            "seine Größe selbst.");
        auto *alertWidth =
            this->createSpinBox(getSettings()->modAlertWidth, 0, 3000);
        alertWidth->setSuffix(" px");
        alertWidth->setSpecialValueText("automatisch");
        alertWidth->setToolTip(sizeHint);
        alertsForm->addRow("Breite", alertWidth);
        auto *alertHeight =
            this->createSpinBox(getSettings()->modAlertHeight, 0, 3000);
        alertHeight->setSuffix(" px");
        alertHeight->setSpecialValueText("automatisch");
        alertHeight->setToolTip(sizeHint);
        alertsForm->addRow("Höhe", alertHeight);

        auto *resetPosition = new QPushButton("Zurücksetzen");
        resetPosition->setToolTip(
            "Die Fenster öffnen wieder dort, wo das System sie hinsetzt, bis "
            "du wieder eines verschiebst.");
        getSettings()->modAlertPositionSaved.connect(
            [resetPosition](const bool &saved, auto) {
                resetPosition->setEnabled(saved);
            },
            this->managedConnections_);
        QObject::connect(resetPosition, &QPushButton::clicked, [] {
            getSettings()->modAlertPositionSaved.setValue(false);
        });
        auto *positionRow = new QHBoxLayout;
        positionRow->addWidget(resetPosition);
        positionRow->addStretch(1);
        alertsForm->addRow("Gemerkte Position", positionRow);
        assistant->addLayout(alertsForm);

        auto *delayTests =
            new QCheckBox("Test-Fenster erst nach 5 Sekunden öffnen");
        delayTests->setToolTip(
            "Damit du nach dem Klick in ein anderes Programm wechseln und "
            "prüfen kannst, ob das Fenster dort vorne aufgeht.");
        assistant.append(delayTests);

        auto *sizeTip = new QLabel(
            "Tipp: Öffne einen Test-Alarm, schieb ihn dorthin, wo er hin soll, zieh "
            "ihn an der Ecke unten rechts auf die gewünschte Größe und schließ "
            "ihn - die nächsten Fenster öffnen dann genau dort und genauso groß.");
        sizeTip->setWordWrap(true);
        sizeTip->setEnabled(false);
        assistant.append(sizeTip);

        // Opens a test window, right away or after the delay above
        const auto openTest =
            [this, delayTests](std::function<void(ModAlertPopup *)> fill) {
                if (!delayTests->isChecked())
                {
                    auto *popup = new ModAlertPopup("test", "testuser", this);
                    fill(popup);
                    popup->present();
                    return;
                }

                // Held by the main window, which outlives these settings, so
                // closing them in the meantime does not call the test off. It
                // also sits on the main window like a real alert does.
                auto *mainWindow = &getApp()->getWindows()->getMainWindow();
                QTimer::singleShot(5000, mainWindow, [mainWindow, fill] {
                    auto *popup =
                        new ModAlertPopup("test", "testuser", mainWindow);
                    fill(popup);
                    popup->present();
                });
            };

        auto *suggestionsIntro = new QLabel(
            "<br><b>Vorschläge</b><br>Wann der Assistent sich meldet.");
        suggestionsIntro->setTextFormat(Qt::RichText);
        suggestionsIntro->setWordWrap(true);
        assistant.append(suggestionsIntro);

        auto *form = new QFormLayout;
        form->addRow("Vorschläge ab so vielen gesammelten Fällen",
                     this->createSpinBox(getSettings()->modAssistMinCases, 1,
                                         2000));
        form->addRow("Nur wenn so viele frühere Fälle ähnlich sind",
                     this->createSpinBox(getSettings()->modAssistMinSimilar, 1,
                                         50));
        form->addRow("Nötige Ähnlichkeit in Prozent",
                     this->createSpinBox(getSettings()->modAssistSimilarity,
                                         10, 100));
        assistant->addLayout(form);

        auto *testSuggestion = new QPushButton("Test-Vorschlag anzeigen");
        testSuggestion->setToolTip(
            "Öffnet ein Vorschlagsfenster mit ausgedachten Nachrichten, damit "
            "du siehst, wie es aussieht und sich verhält. Die Knöpfe tun "
            "nichts.");
        QObject::connect(testSuggestion, &QPushButton::clicked, this,
                         [openTest] {
                             openTest([](ModAlertPopup *popup) {
                                 popup->showTestSuggestion();
                             });
                         });
        auto *testSuggestionRow = new QHBoxLayout;
        testSuggestionRow->addWidget(testSuggestion);
        testSuggestionRow->addStretch(1);
        assistant->addLayout(testSuggestionRow);

        auto *repeatIntro = new QLabel(
            "<br><b>Wiederholte Nachrichten</b><br>Die Timeouts, die der Alarm "
            "für wiederholte Nachrichten der Reihe nach anbietet. Der erste "
            "gilt für dreimal dieselbe Nachricht hintereinander, der nächste "
            "jedes Mal, wenn der User nach einem abgesessenen Timeout "
            "weitermacht. Nach der letzten Stufe bleibt es bei der letzten.");
        repeatIntro->setTextFormat(Qt::RichText);
        repeatIntro->setWordWrap(true);
        assistant.append(repeatIntro);

        auto *steps = new QLineEdit(getSettings()->repeatAlertSteps.getValue());
        steps->setPlaceholderText("30s, 1m, 5m, 10m, 30m");
        auto *stepsPreview = new QLabel;
        stepsPreview->setTextFormat(Qt::RichText);
        stepsPreview->setWordWrap(true);

        const auto showSteps = [stepsPreview](const QString &text) {
            const auto parsed = RepeatSpamDetector::parseSteps(text);
            if (parsed.empty())
            {
                stepsPreview->setText(QStringLiteral(
                    "<span style=\"color:#e05050\">Keine gültige Liste – "
                    "schreib zum Beispiel 30s, 1m, 5m. Bis dahin gilt die "
                    "letzte gültige Liste.</span>"));
                return;
            }

            QStringList shown;
            for (const auto seconds : parsed)
            {
                shown.append(formatTime(seconds));
            }
            stepsPreview->setText(shown.join(QStringLiteral(" → ")));
        };
        showSteps(steps->text());

        QObject::connect(steps, &QLineEdit::textChanged, stepsPreview,
                         showSteps);
        // Only a list that reads is kept, so a half typed one never ends up
        // deciding a timeout
        QObject::connect(steps, &QLineEdit::editingFinished, steps, [steps] {
            if (!RepeatSpamDetector::parseSteps(steps->text()).empty())
            {
                getSettings()->repeatAlertSteps.setValue(
                    steps->text().trimmed());
            }
        });

        assistant.append(steps);
        assistant.append(stepsPreview);

        auto *repeatForm = new QFormLayout;
        auto *similarity =
            this->createSpinBox(getSettings()->repeatAlertSimilarity, 40, 100);
        similarity->setSuffix(" %");
        similarity->setToolTip(
            "Nachrichten ab 10 Zeichen zählen als gleich, wenn sie mindestens "
            "so ähnlich sind – so kommt man mit einem getauschten Wort nicht "
            "an der Regel vorbei. Kürzere müssen genau gleich sein. Bei 100 "
            "zählen nur genau gleiche.");
        repeatForm->addRow("Als gleiche Nachricht ab", similarity);
        assistant->addLayout(repeatForm);

        auto *testAlert = new QPushButton("Test-Alarm anzeigen");
        testAlert->setToolTip(
            "Öffnet den Alarm mit ausgedachten Nachrichten, damit du siehst, "
            "wie er aussieht und sich verhält. Nochmal klicken zeigt die "
            "Version nach einem Timeout. Die Knöpfe tun nichts.");
        QObject::connect(testAlert, &QPushButton::clicked, this, [openTest] {
            // Every other click shows the alert as it looks after a timeout
            static bool afterTimeout = false;
            const bool variant = afterTimeout;
            afterTimeout = !afterTimeout;

            openTest([variant](ModAlertPopup *popup) {
                popup->showTestCase(variant);
            });
        });
        auto *testRow = new QHBoxLayout;
        testRow->addWidget(testAlert);
        testRow->addStretch(1);
        assistant->addLayout(testRow);

        auto *emoteIntro = new QLabel(
            "<br><b>Emote-Spam</b><br>Ein Alarm für User, die den Chat mit "
            "Emotes fluten. Er zählt die Emotes ihrer Nachrichten über die hier "
            "eingestellte Zeit zusammen, und zwar aus allen Nachrichten, in "
            "denen Emotes überwiegen – viele kurze Schwälle zählen also genauso "
            "wie eine lange Emote-Wand. Die Stufen legen fest, was er jeweils "
            "anbietet: löschen oder eine Timeout-Dauer. Die nächste Stufe kommt "
            "erst, wenn wirklich eine Nachricht gelöscht oder der User "
            "getimeoutet wurde.");
        emoteIntro->setTextFormat(Qt::RichText);
        emoteIntro->setWordWrap(true);
        assistant.append(emoteIntro);

        auto *emoteForm = new QFormLayout;
        auto *minEmotes =
            this->createSpinBox(getSettings()->emoteAlertMinEmotes, 2, 200);
        minEmotes->setSuffix(" Emotes");
        emoteForm->addRow("Alarm ab", minEmotes);
        auto *emoteWindow =
            this->createSpinBox(getSettings()->emoteAlertWindowSeconds, 5, 600);
        emoteWindow->setSuffix(" s");
        emoteWindow->setToolTip(
            "Wie weit zurück die Emotes zusammengezählt werden. Eine einzelne "
            "Nachricht mit genug Emotes löst den Alarm auch allein aus.");
        emoteForm->addRow("Zusammengezählt über", emoteWindow);
        assistant->addLayout(emoteForm);

        auto *emoteSteps =
            new QLineEdit(getSettings()->emoteAlertSteps.getValue());
        emoteSteps->setPlaceholderText("löschen, löschen, 30s");
        auto *emotePreview = new QLabel;
        emotePreview->setTextFormat(Qt::RichText);
        emotePreview->setWordWrap(true);

        const auto showEmoteSteps = [emotePreview](const QString &text) {
            const auto parsed = EmoteSpamDetector::parseSteps(text);
            if (parsed.empty())
            {
                emotePreview->setText(QStringLiteral(
                    "<span style=\"color:#e05050\">Keine gültige Liste – "
                    "schreib zum Beispiel löschen, löschen, 30s. Bis dahin gilt "
                    "die letzte gültige Liste.</span>"));
                return;
            }

            QStringList shown;
            for (const auto step : parsed)
            {
                shown.append(step == EmoteSpamDetector::DELETE
                                 ? QStringLiteral("Löschen")
                                 : formatTime(step));
            }
            emotePreview->setText(shown.join(QStringLiteral(" → ")));
        };
        showEmoteSteps(emoteSteps->text());

        QObject::connect(emoteSteps, &QLineEdit::textChanged, emotePreview,
                         showEmoteSteps);
        QObject::connect(emoteSteps, &QLineEdit::editingFinished, emoteSteps,
                         [emoteSteps] {
                             if (!EmoteSpamDetector::parseSteps(
                                      emoteSteps->text())
                                      .empty())
                             {
                                 getSettings()->emoteAlertSteps.setValue(
                                     emoteSteps->text().trimmed());
                             }
                         });
        assistant.append(emoteSteps);
        assistant.append(emotePreview);

        auto *testEmote = new QPushButton("Test-Alarm für Emote-Spam anzeigen");
        testEmote->setToolTip(
            "Öffnet den Emote-Alarm mit ausgedachten Nachrichten. Jeder Klick "
            "geht eine Stufe weiter. Die Knöpfe tun nichts.");
        QObject::connect(testEmote, &QPushButton::clicked, this, [openTest] {
            static int step = 0;
            const int current = step;
            step = (step + 1) % 3;

            openTest([current](ModAlertPopup *popup) {
                popup->showTestEmoteSpam(current);
            });
        });
        auto *testEmoteRow = new QHBoxLayout;
        testEmoteRow->addWidget(testEmote);
        testEmoteRow->addStretch(1);
        assistant->addLayout(testEmoteRow);

        assistant->addStretch(1);
    }
}

bool ModAssistantPage::filterElements(const QString &query)
{
    static const QStringList keywords{
        "mod",       "assistent", "assistant", "moderation", "spam",
        "emote",     "alarm",     "alert",     "vorschlag",  "timeout",
        "wiederholt", "fenster",
    };

    if (query.isEmpty())
    {
        return true;
    }
    for (const auto &keyword : keywords)
    {
        if (keyword.contains(query, Qt::CaseInsensitive) ||
            query.contains(keyword, Qt::CaseInsensitive))
        {
            return true;
        }
    }
    return false;
}

}  // namespace chatterino
