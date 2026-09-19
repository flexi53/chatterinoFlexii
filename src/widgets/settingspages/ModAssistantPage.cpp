// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ModAssistantPage.hpp"

#include "Application.hpp"
#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "controllers/sound/ISoundController.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/FormatTime.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/settingspages/PageSections.hpp"
#include "widgets/Window.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace chatterino {

using namespace pagesections;

namespace {

/// A line to type the steps into, with how they read underneath. Only a list
/// that reads is saved, so a half typed one never ends up deciding a timeout.
void addStepsEditor(QVBoxLayout *layout, QStringSetting &setting,
                    const QString &example,
                    std::function<std::vector<int>(const QString &)> parse,
                    std::function<QString(int)> label)
{
    auto *steps = new QLineEdit(setting.getValue());
    steps->setPlaceholderText(example);
    auto *preview = new QLabel;
    preview->setTextFormat(Qt::RichText);
    preview->setWordWrap(true);

    const auto show = [preview, parse, label, example](const QString &text) {
        const auto parsed = parse(text);
        if (parsed.empty())
        {
            preview->setText(
                QStringLiteral("<span style=\"color:#e05050\">Keine gültige "
                               "Liste - schreib zum Beispiel %1. Bis dahin gilt "
                               "die letzte gültige Liste.</span>")
                    .arg(example.toHtmlEscaped()));
            return;
        }

        QStringList shown;
        for (const auto step : parsed)
        {
            shown.append(label(step));
        }
        preview->setText(shown.join(QStringLiteral(" → ")));
    };
    show(steps->text());

    QObject::connect(steps, &QLineEdit::textChanged, preview, show);
    QObject::connect(steps, &QLineEdit::editingFinished, steps,
                     [steps, parse, &setting] {
                         if (!parse(steps->text()).empty())
                         {
                             setting.setValue(steps->text().trimmed());
                         }
                     });

    layout->addWidget(steps);
    layout->addWidget(preview);
}

}  // namespace

ModAssistantPage::ModAssistantPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // One tab for what all alert windows share, then one per kind of alert
    // holding everything about it - detection, steps, colour and its test
    auto *tabs = new QTabWidget;
    outer->addWidget(tabs);

    auto *general = addPageTab(tabs, "Allgemein");
    auto *suggestions = addPageTab(tabs, "Vorschläge");
    auto *repeats = addPageTab(tabs, "Wiederholte Nachrichten");
    auto *emotes = addPageTab(tabs, "Emote-Spam");

    // ----- Allgemein -----
    addText(general,
            "Der Mod-Assistent meldet sich mit einem Alarm-Fenster, wenn in "
            "einem Kanal, in dem du Mod bist, etwas passiert: jemand schreibt "
            "dieselbe Nachricht immer wieder, flutet den Chat mit Emotes, oder "
            "schreibt etwas, wofür andere Mods schon Timeouts gegeben haben. "
            "Eingeschaltet wird das pro Kanal über den Schild-Knopf neben dem "
            "Emote-Knopf. Es passiert nie etwas, solange du nicht selbst auf "
            "den Knopf im Fenster drückst.");
    addText(general,
            "Was hier steht, gilt für alle Alarm-Fenster. Alles zu einer "
            "bestimmten Art von Alarm findest du in ihrem eigenen Reiter.",
            true);

    addHeading(general, "Verhalten");
    general->addWidget(this->createCheckBox(
        "Immer im Vordergrund, auch über anderen Programmen",
        getSettings()->modAlertAlwaysOnTop,
        "Das Fenster liegt über jedem Programm, auch wenn gerade der Browser "
        "oder ein Spiel vorne ist, und nimmt dir trotzdem nicht den Fokus. "
        "Gilt für Fenster, die ab jetzt aufgehen."));
    general->addWidget(this->createCheckBox(
        "Ton abspielen, wenn ein Alarm-Fenster aufgeht",
        getSettings()->modAlertSound,
        "Spielt einen Ton, sobald ein neues Alarm-Fenster aufgeht - nicht, "
        "wenn ein offenes nur aktualisiert wird. Welcher Ton, stellst du im "
        "Reiter der jeweiligen Alarm-Art ein."));
    addText(general,
            "Jede Alarm-Art hat ihren eigenen Ton, eingestellt in ihrem Reiter. "
            "Den Ton für Live-Benachrichtigungen und Highlights stellst du dort "
            "ein: Einstellungen → Live Notifications bzw. Highlights.",
            true);
    {
        auto *form = new QFormLayout;
        auto *autoClose =
            this->createSpinBox(getSettings()->repeatAlertAutoClose, 0, 300);
        autoClose->setSuffix(" s");
        autoClose->setSpecialValueText("nie");
        autoClose->setToolTip(
            "Nach dieser Zeit schließt sich das Fenster von selbst, als hättest "
            "du Ignorieren gedrückt. Solange die Maus darüber ist, bleibt es "
            "offen.");
        form->addRow("Fenster schließt sich von selbst nach", autoClose);
        general->addLayout(form);
    }

    addHeading(general, "Größe und Position");
    {
        const auto sizeHint = QStringLiteral(
            "Du kannst ein Alarm-Fenster auch einfach an der Ecke unten rechts "
            "größer ziehen - die Größe wird beim Schließen übernommen und gilt "
            "für alle weiteren Fenster. Bei \"automatisch\" wählt das Fenster "
            "seine Größe selbst.");

        auto *form = new QFormLayout;
        auto *alertWidth =
            this->createSpinBox(getSettings()->modAlertWidth, 0, 3000);
        alertWidth->setSuffix(" px");
        alertWidth->setSpecialValueText("automatisch");
        alertWidth->setToolTip(sizeHint);
        form->addRow("Breite", alertWidth);
        auto *alertHeight =
            this->createSpinBox(getSettings()->modAlertHeight, 0, 3000);
        alertHeight->setSuffix(" px");
        alertHeight->setSpecialValueText("automatisch");
        alertHeight->setToolTip(sizeHint);
        form->addRow("Höhe", alertHeight);

        auto *resetPosition = new QPushButton("Zurücksetzen");
        resetPosition->setToolTip(
            "Die Fenster öffnen wieder dort, wo das System sie hinsetzt, bis du "
            "wieder eines verschiebst.");
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
        form->addRow("Gemerkte Position", positionRow);
        general->addLayout(form);

        addText(general,
                "Tipp: Öffne einen Test-Alarm, schieb ihn dorthin, wo er hin "
                "soll, zieh ihn an der Ecke unten rechts auf die gewünschte "
                "Größe und schließ ihn - die nächsten Fenster öffnen dann genau "
                "dort und genauso groß.",
                true);
    }

    addHeading(general, "Testen");
    auto *delayTests = new QCheckBox("Test-Fenster erst nach 5 Sekunden öffnen");
    delayTests->setToolTip(
        "Gilt für die Test-Knöpfe in allen Reitern. Damit kannst du nach dem "
        "Klick in ein anderes Programm wechseln und prüfen, ob das Fenster dort "
        "vorne aufgeht.");
    general->addWidget(delayTests);
    general->addStretch(1);

    // Opens a test window, right away or after the delay above
    const auto openTest = [this, delayTests](
                              std::function<void(ModAlertPopup *)> fill) {
        if (!delayTests->isChecked())
        {
            auto *popup = new ModAlertPopup("test", "testuser", this);
            fill(popup);
            popup->present();
            return;
        }

        // Held by the main window, which outlives these settings, so closing
        // them in the meantime does not call the test off. It also sits on the
        // main window like a real alert does.
        auto *mainWindow = &getApp()->getWindows()->getMainWindow();
        QTimer::singleShot(5000, mainWindow, [mainWindow, fill] {
            auto *popup = new ModAlertPopup("test", "testuser", mainWindow);
            fill(popup);
            popup->present();
        });
    };

    // The colour one kind of alert sets its reason off in. Whatever is picked
    // is lit up, and the chip beside the button shows it as the window will.
    const auto addColor = [this](QVBoxLayout *layout, QStringSetting &setting,
                                 ModAlertPopup::Kind kind) {
        addHeading(layout, "Aussehen");

        auto *button = new ColorButton(ModAlertPopup::reasonColor(kind));
        auto *preview = new QLabel(QStringLiteral("REASON"));
        auto *reset = new QPushButton("Standardfarbe");
        setting.connect(
            [kind, button, preview](const QString &, auto) {
                const auto color = ModAlertPopup::reasonColor(kind);
                button->setColor(color);
                preview->setStyleSheet(ModAlertPopup::reasonTagStyle(color));
            },
            this->managedConnections_);

        QObject::connect(button, &ColorButton::clicked, this,
                         [this, &setting, kind] {
                             auto *dialog = new ColorPickerDialog(
                                 ModAlertPopup::reasonColor(kind), this);
                             QObject::connect(
                                 dialog, &ColorPickerDialog::colorConfirmed,
                                 this, [&setting, kind](QColor picked) {
                                     if (picked.isValid())
                                     {
                                         setting.setValue(
                                             ModAlertPopup::vividColor(
                                                 picked,
                                                 ModAlertPopup::
                                                     defaultReasonColor(kind))
                                                 .name());
                                     }
                                 });
                             dialog->show();
                         });
        QObject::connect(reset, &QPushButton::clicked, this,
                         [&setting, kind] {
                             setting.setValue(
                                 ModAlertPopup::defaultReasonColor(kind).name());
                         });

        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        row->addWidget(button);
        row->addWidget(preview);
        row->addWidget(reset);
        row->addStretch(1);
        auto *form = new QFormLayout;
        form->addRow("Farbe für den Reason", row);
        layout->addLayout(form);

        addText(layout,
                "Jede Farbe wird automatisch kräftig und hell gemacht, damit "
                "der Reason immer leuchtet. Grau, Schwarz und Weiß leuchten "
                "nicht - bei ihnen bleibt es bei der Standardfarbe. Der "
                "ablaufende Balken im Fenster nimmt dieselbe Farbe.",
                true);
    };

    // The sound one kind of alert plays - apart from the ping everything else
    // plays, so an alert is told from a live notification by ear
    const auto addSound = [this](QVBoxLayout *layout, QStringSetting &setting) {
        addHeading(layout, "Ton");

        auto *choice = new QComboBox;
        auto *listen = new QPushButton("Anhören");
        const auto fill = [choice, &setting] {
            const QSignalBlocker blocker(choice);
            choice->clear();
            choice->addItem("Standard-Ping (wie Highlights und Live)", QString());
            for (const auto &[value, name] : ModAlertPopup::builtInSounds())
            {
                choice->addItem(name, value);
            }
            const auto current = setting.getValue();
            if (!current.isEmpty() &&
                !current.startsWith(QStringLiteral("builtin:")))
            {
                choice->addItem(QStringLiteral("Eigene: %1")
                                    .arg(QFileInfo(current).fileName()),
                                current);
            }
            choice->addItem("Eigene Datei …", QStringLiteral("__choose__"));
            const auto index = choice->findData(current);
            choice->setCurrentIndex(index >= 0 ? index : 0);
        };
        fill();
        setting.connect(
            [fill](const auto &, auto) {
                fill();
            },
            this->managedConnections_, false);

        QObject::connect(
            choice, &QComboBox::activated, this,
            [this, choice, &setting, fill](int index) {
                const auto value = choice->itemData(index).toString();
                if (value != QStringLiteral("__choose__"))
                {
                    setting.setValue(value);
                    return;
                }
                const auto file = QFileDialog::getOpenFileName(
                    this, "Ton auswählen", QString(),
                    "Töne (*.wav *.mp3 *.ogg *.flac)");
                if (file.isEmpty())
                {
                    fill();
                    return;
                }
                setting.setValue(file);
            });
        QObject::connect(listen, &QPushButton::clicked, this, [&setting] {
            getApp()->getSound()->play(
                ModAlertPopup::soundUrl(setting.getValue()));
        });

        auto *row = new QHBoxLayout;
        row->addWidget(choice);
        row->addWidget(listen);
        row->addStretch(1);
        auto *form = new QFormLayout;
        form->addRow("Ton", row);
        layout->addLayout(form);
        addText(layout,
                "Spielt nur, wenn unter „Allgemein“ der Ton eingeschaltet ist.",
                true);
    };

    // ----- Vorschläge -----
    addText(suggestions,
            "Der Assistent lernt aus Timeouts und Banns, die Mods in deinen "
            "Kanälen geben, und schlägt eine Aktion vor, wenn jemand etwas "
            "Ähnliches schreibt. Im Fenster steht als Reason, was er an der "
            "Nachricht erkannt hat, und darunter der ähnlichste frühere Fall.");
    addText(suggestions,
            "Ob der Assistent in einem Kanal nur lernt oder auch vorschlägt, "
            "stellst du über den Schild-Knopf in diesem Kanal ein.",
            true);

    addHeading(suggestions, "Wann ein Vorschlag kommt");
    {
        auto *form = new QFormLayout;
        form->addRow("Erst ab so vielen gesammelten Fällen",
                     this->createSpinBox(getSettings()->modAssistMinCases, 1,
                                         2000));
        form->addRow("Nur wenn so viele frühere Fälle ähnlich sind",
                     this->createSpinBox(getSettings()->modAssistMinSimilar, 1,
                                         50));
        auto *similarity =
            this->createSpinBox(getSettings()->modAssistSimilarity, 10, 100);
        similarity->setSuffix(" %");
        form->addRow("Nötige Ähnlichkeit", similarity);
        suggestions->addLayout(form);
    }

    addColor(suggestions, getSettings()->modAlertColorSuggestion,
             ModAlertPopup::Kind::Suggestion);
    addSound(suggestions, getSettings()->modAlertSoundSuggestion);

    addHeading(suggestions, "Testen");
    {
        auto *test = new QPushButton("Test-Vorschlag anzeigen");
        test->setToolTip(
            "Öffnet ein Vorschlagsfenster mit ausgedachten Nachrichten, damit "
            "du siehst, wie es aussieht und sich verhält. Die Knöpfe tun "
            "nichts.");
        QObject::connect(test, &QPushButton::clicked, this, [openTest] {
            openTest([](ModAlertPopup *popup) {
                popup->showTestSuggestion();
            });
        });
        addButtonRow(suggestions, test);
    }
    suggestions->addStretch(1);

    // ----- Wiederholte Nachrichten -----
    addText(repeats,
            "Ein Alarm, wenn jemand dreimal hintereinander dieselbe Nachricht "
            "schreibt. Er bietet einen Timeout an, und macht der User nach dem "
            "Timeout weiter, beim nächsten Mal die nächste Stufe.");
    addText(repeats,
            "Eingeschaltet wird der Alarm pro Kanal über den Schild-Knopf in "
            "diesem Kanal.",
            true);

    addHeading(repeats, "Erkennung");
    {
        auto *form = new QFormLayout;
        auto *similarity =
            this->createSpinBox(getSettings()->repeatAlertSimilarity, 40, 100);
        similarity->setSuffix(" %");
        similarity->setToolTip(
            "Nachrichten ab 10 Zeichen zählen als gleich, wenn sie mindestens so "
            "ähnlich sind - so kommt man mit einem getauschten Wort nicht an "
            "der Regel vorbei. Kürzere müssen genau gleich sein. Bei 100 zählen "
            "nur genau gleiche.");
        form->addRow("Als gleiche Nachricht ab", similarity);
        repeats->addLayout(form);
    }

    addHeading(repeats, "Timeout-Stufen");
    {
        addText(repeats,
                "Der Reihe nach, durch Kommas getrennt. Nach der letzten Stufe "
                "bleibt es bei der letzten.",
                true);

        addStepsEditor(repeats, getSettings()->repeatAlertSteps,
                       QStringLiteral("30s, 1m, 5m, 10m, 30m"),
                       &RepeatSpamDetector::parseSteps, [](int seconds) {
                           return formatTime(seconds);
                       });
    }

    addColor(repeats, getSettings()->modAlertColorRepeat,
             ModAlertPopup::Kind::RepeatedMessage);
    addSound(repeats, getSettings()->modAlertSoundRepeat);

    addHeading(repeats, "Testen");
    {
        auto *test = new QPushButton("Test-Alarm anzeigen");
        test->setToolTip(
            "Öffnet den Alarm mit ausgedachten Nachrichten, damit du siehst, "
            "wie er aussieht und sich verhält. Nochmal klicken zeigt die "
            "Version nach einem Timeout. Die Knöpfe tun nichts.");
        QObject::connect(test, &QPushButton::clicked, this, [openTest] {
            // Every other click shows the alert as it looks after a timeout
            static bool afterTimeout = false;
            const bool variant = afterTimeout;
            afterTimeout = !afterTimeout;

            openTest([variant](ModAlertPopup *popup) {
                popup->showTestCase(variant);
            });
        });
        addButtonRow(repeats, test);
    }
    repeats->addStretch(1);

    // ----- Emote-Spam -----
    addText(emotes,
            "Ein Alarm für User, die den Chat mit Emotes fluten. Er zählt die "
            "Emotes aus allen Nachrichten zusammen, in denen Emotes überwiegen "
            "- viele kurze Schwälle zählen also genauso wie eine lange "
            "Emote-Wand.");
    addText(emotes,
            "Eingeschaltet wird der Alarm pro Kanal über den Schild-Knopf in "
            "diesem Kanal.",
            true);

    addHeading(emotes, "Erkennung");
    {
        auto *form = new QFormLayout;
        auto *minEmotes =
            this->createSpinBox(getSettings()->emoteAlertMinEmotes, 2, 200);
        minEmotes->setSuffix(" Emotes");
        form->addRow("Alarm ab", minEmotes);
        auto *window =
            this->createSpinBox(getSettings()->emoteAlertWindowSeconds, 5, 600);
        window->setSuffix(" s");
        window->setToolTip(
            "Wie weit zurück die Emotes zusammengezählt werden. Eine einzelne "
            "Nachricht mit genug Emotes löst den Alarm auch allein aus.");
        form->addRow("Zusammengezählt über", window);
        emotes->addLayout(form);
    }

    addHeading(emotes, "Stufen");
    {
        addText(emotes,
                "Was der Alarm jeweils anbietet: löschen oder eine "
                "Timeout-Dauer, durch Kommas getrennt. Die nächste Stufe kommt "
                "erst, wenn wirklich eine Nachricht gelöscht oder der User "
                "getimeoutet wurde.",
                true);

        addStepsEditor(emotes, getSettings()->emoteAlertSteps,
                       QStringLiteral("löschen, löschen, 30s"),
                       &EmoteSpamDetector::parseSteps, [](int step) {
                           return step == EmoteSpamDetector::DELETE
                                      ? QStringLiteral("Löschen")
                                      : formatTime(step);
                       });
    }

    addColor(emotes, getSettings()->modAlertColorEmote,
             ModAlertPopup::Kind::EmoteSpam);
    addSound(emotes, getSettings()->modAlertSoundEmote);

    addHeading(emotes, "Testen");
    {
        auto *test = new QPushButton("Test-Alarm für Emote-Spam anzeigen");
        test->setToolTip(
            "Öffnet den Emote-Alarm mit ausgedachten Nachrichten. Jeder Klick "
            "geht eine Stufe weiter. Die Knöpfe tun nichts.");
        QObject::connect(test, &QPushButton::clicked, this, [openTest] {
            static int step = 0;
            const int current = step;
            step = (step + 1) % 3;

            openTest([current](ModAlertPopup *popup) {
                popup->showTestEmoteSpam(current);
            });
        });
        addButtonRow(emotes, test);
    }
    emotes->addStretch(1);
}

bool ModAssistantPage::filterElements(const QString &query)
{
    static const QStringList keywords{
        "mod",        "assistent", "assistant", "moderation", "spam",
        "emote",      "alarm",     "alert",     "vorschlag",  "timeout",
        "wiederholt", "fenster",   "reason",    "farbe",      "position",
    };

    return matchesPageText(this, query) ||
           matchesKeywords(query, keywords);
}

}  // namespace chatterino
