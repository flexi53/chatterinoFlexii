// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ModAssistantPage.hpp"

#include "Application.hpp"
#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/WordAlertDetector.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "controllers/sound/ISoundController.hpp"
#include "common/Channel.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/SpellingVariants.hpp"
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
#include <QPlainTextEdit>
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


/// One made-up chat line for the preview under the word list
MessagePtr previewLine(const QString &text)
{
    MessageBuilder builder;
    builder.emplace<TimestampElement>(QTime::currentTime());
    builder.emplace<TextElement>("Jemand:", MessageElementFlag::Username,
                                 MessageColor(QColor(255, 127, 80)),
                                 FontStyle::ChatMediumBold);
    builder.appendOrEmplaceText(text, MessageColor::Text);
    builder->loginName = "jemand";
    builder->displayName = "Jemand";
    builder->messageText = text;
    builder->searchText = text;
    builder->flags.set(MessageFlag::DoNotLog);
    return builder.release();
}

/// The list of words, one row each: the word itself and the buttons for
/// what it alone offers. The buttons sit there greyed out; pressing them
/// picks the steps for that word. With none picked the word follows the
/// steps set below for all of them.
class WordRows : public QWidget
{
public:
    explicit WordRows(QWidget *parent = nullptr)
        : QWidget(parent)
        , rows_(new QVBoxLayout(this))
    {
        this->rows_->setContentsMargins(0, 0, 0, 0);
        this->rows_->setSpacing(2);
        this->reload();
    }

    /// Reads the words from the settings and builds their rows
    void reload()
    {
        this->loading_ = true;
        while (auto *item = this->rows_->takeAt(0))
        {
            delete item->widget();
            delete item;
        }
        for (const auto &word :
             WordAlertDetector::parseWords(
                 getSettings()->wordAlertWords.getValue()))
        {
            this->addRow(word.word, word.steps);
        }
        this->loading_ = false;
    }

    /// A row for a word yet to be typed
    void addEmptyRow()
    {
        this->addRow({}, {});
        this->rows_->itemAt(this->rows_->count() - 1)
            ->widget()
            ->findChild<QLineEdit *>()
            ->setFocus();
    }

private:
    void addRow(const QString &word, const std::vector<int> &steps)
    {
        auto *row = new QWidget(this);
        auto *lines = new QVBoxLayout(row);
        lines->setContentsMargins(0, 0, 0, 0);
        lines->setSpacing(2);

        auto *top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(3);
        lines->addLayout(top);

        auto *edit = new QLineEdit(word);
        edit->setPlaceholderText("Wort");
        edit->setFixedWidth(150);
        top->addWidget(edit);
        QObject::connect(edit, &QLineEdit::textChanged, this, [this] {
            this->save();
        });

        // The steps stay folded away - a row of nine buttons per word would
        // make a wall of them. What is picked stands on the button that
        // unfolds them.
        auto *steppers = new QWidget(row);
        steppers->hide();
        auto *stepRow = new QHBoxLayout(steppers);
        stepRow->setContentsMargins(0, 0, 0, 0);
        stepRow->setSpacing(3);

        auto *unfold = new QPushButton;
        unfold->setCheckable(true);
        unfold->setToolTip(
            "Was der Alarm für dieses Wort vorschlägt. Ohne Auswahl gelten "
            "die Stufen weiter unten.");
        top->addWidget(unfold);
        QObject::connect(unfold, &QPushButton::toggled, steppers,
                         [steppers](bool on) {
                             steppers->setVisible(on);
                         });

        const auto accent =
            ModAlertPopup::reasonColor(ModAlertPopup::Kind::Word);
        // What the unfolding button says: the steps picked, or that the
        // general ones hold
        const auto describe = [unfold, steppers] {
            QStringList picked;
            for (auto *button : steppers->findChildren<QPushButton *>())
            {
                if (button->isChecked())
                {
                    picked.append(button->text());
                }
            }
            unfold->setText(picked.isEmpty()
                                ? QStringLiteral("Stufen: Standard")
                                : QStringLiteral("Stufen: %1")
                                      .arg(picked.join(QStringLiteral(", "))));
        };

        for (const auto step : WordAlertDetector::palette())
        {
            auto *button = new QPushButton(stepLabel(step));
            button->setCheckable(true);
            button->setChecked(std::find(steps.begin(), steps.end(), step) !=
                               steps.end());
            button->setProperty("flexiiStep", step);
            button->setToolTip(QStringLiteral("Für dieses Wort %1 vorschlagen")
                                   .arg(stepLabel(step)));
            button->setStyleSheet(stepButtonStyle(accent));
            stepRow->addWidget(button);
            QObject::connect(button, &QPushButton::toggled, this,
                             [this, describe] {
                                 describe();
                                 this->save();
                             });
        }
        stepRow->addStretch(1);
        lines->addWidget(steppers);
        describe();

        top->addStretch(1);

        auto *remove = new QPushButton(QStringLiteral("✕"));
        remove->setToolTip("Dieses Wort von der Liste nehmen");
        remove->setFixedWidth(26);
        top->addWidget(remove);
        QObject::connect(remove, &QPushButton::clicked, this, [this, row] {
            row->deleteLater();
            this->rows_->removeWidget(row);
            this->save();
        });

        this->rows_->addWidget(row);
    }

    /// How a step button looks: quiet while it is not picked, in the
    /// alert's colour once it is
    static QString stepButtonStyle(const QColor &accent)
    {
        const auto light = getTheme()->isLightTheme();
        const auto quiet = light ? QStringLiteral("#5a5a5a")
                                 : QStringLiteral("#8c8c8c");
        const auto line = light ? QStringLiteral("#b4b4b4")
                                : QStringLiteral("#4a4a4a");
        const auto picked = light ? QStringLiteral("#101010")
                                  : QStringLiteral("#ffffff");
        return QStringLiteral(
                   "QPushButton { padding: 1px 6px; border: 1px solid %1; "
                   "border-radius: 4px; color: %2; background: transparent; }"
                   "QPushButton:checked { color: %3; border: 1px solid %4; "
                   "background: rgba(%5, %6, %7, 55); }")
            .arg(line, quiet, picked, accent.name())
            .arg(accent.red())
            .arg(accent.green())
            .arg(accent.blue());
    }

    /// What the buttons say, in the order the palette has them
    static QString stepLabel(int step)
    {
        if (step == WordAlertDetector::DELETE)
        {
            return QStringLiteral("Löschen");
        }
        if (step == 0)
        {
            return QStringLiteral("Bann");
        }
        return formatTime(step);
    }

    void save()
    {
        if (this->loading_)
        {
            return;
        }

        std::vector<WordAlertDetector::Watched> words;
        for (int i = 0; i < this->rows_->count(); i++)
        {
            auto *row = this->rows_->itemAt(i)->widget();
            if (row == nullptr)
            {
                continue;
            }
            auto *edit = row->findChild<QLineEdit *>();
            if (edit == nullptr || edit->text().trimmed().isEmpty())
            {
                continue;
            }

            std::vector<int> steps;
            for (auto *button : row->findChildren<QPushButton *>())
            {
                // The button that unfolds the others is checkable too, but
                // carries no step
                const auto step = button->property("flexiiStep");
                if (step.isValid() && button->isChecked())
                {
                    steps.push_back(step.toInt());
                }
            }
            // In the order the buttons stand in, however they were pressed
            std::sort(steps.begin(), steps.end(), [](int a, int b) {
                const auto &palette = WordAlertDetector::palette();
                return std::find(palette.begin(), palette.end(), a) <
                       std::find(palette.begin(), palette.end(), b);
            });

            words.push_back({
                .word = edit->text().trimmed(),
                .pattern = {},
                .steps = steps,
            });
        }

        getSettings()->wordAlertWords.setValue(
            WordAlertDetector::writeWords(words));
    }

    QVBoxLayout *rows_;
    /// While the rows are being built, nothing is written back
    bool loading_ = false;
};

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
    auto *words = addPageTab(tabs, "Wörter");
    auto *shared = addPageTab(tabs, "Shared Chat");

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
        "Knopf „Verwarnen“ im Alarm-Fenster", getSettings()->modAlertWarnButton,
        "Die sanfte Stufe vor dem Timeout: verwarnt wie auf twitch.tv, mit "
        "einem Grund, den der User bestätigen muss, bevor er weiterschreiben "
        "kann. Das Fenster schlägt einen passenden Grund vor; die Liste der "
        "Gründe steht unter Buttons. Eine Verwarnung zählt als erledigt, beim "
        "nächsten Mal wird die nächste Stufe vorgeschlagen."));
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
        auto *preview = new QLabel(QStringLiteral("GRUND"));
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
        form->addRow("Farbe für den Grund-Kasten", row);
        layout->addLayout(form);

        addText(layout,
                "Jede Farbe wird automatisch kräftig und hell gemacht, damit "
                "der Grund-Kasten immer leuchtet. Grau, Schwarz und Weiß leuchten "
                "nicht - bei ihnen bleibt es bei der Standardfarbe. Der "
                "ablaufende Balken im Fenster nimmt dieselbe Farbe.",
                true);
    };

    // The sound one kind of alert plays - apart from the ping everything else
    // plays, so an alert is told from a live notification by ear
    const auto addSound = [this](QVBoxLayout *layout, QStringSetting &setting) {
        addHeading(layout, "Ton");
        auto *form = new QFormLayout;
        form->addRow("Ton",
                     soundChooser(this, setting, this->managedConnections_));
        layout->addLayout(form);
        addText(layout,
                "Spielt nur, wenn unter „Allgemein“ der Ton eingeschaltet ist.",
                true);
    };

    // ----- Vorschläge -----
    addText(suggestions,
            "Der Assistent lernt aus Timeouts und Banns, die Mods in deinen "
            "Kanälen geben, und schlägt eine Aktion vor, wenn jemand etwas "
            "Ähnliches schreibt. Im Fenster steht als Grund, was er an der "
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
                "Der Reihe nach, durch Kommas getrennt: Dauern wie 30s, 10m, "
                "1d - oder bann für einen dauerhaften Bann. Nach der letzten "
                "Stufe bleibt es bei der letzten.",
                true);

        addStepsEditor(repeats, getSettings()->repeatAlertSteps,
                       QStringLiteral("30s, 1m, 5m, 10m, 30m"),
                       &RepeatSpamDetector::parseSteps, [](int seconds) {
                           return seconds == 0 ? QStringLiteral("Bann")
                                               : formatTime(seconds);
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
            "Emote-Wand. Dazu kommen zwei Regeln, die für sich allein "
            "auslösen: eine einzelne Nachricht mit sehr vielen Emotes, und "
            "mehrere Nachrichten hintereinander, in denen nichts als Emotes "
            "stehen.");
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

        auto *single =
            this->createSpinBox(getSettings()->emoteAlertSingleMessage, 0, 200);
        single->setSuffix(" Emotes");
        single->setSpecialValueText("aus");
        single->setToolTip(
            "Eine einzelne Nachricht mit so vielen Emotes löst den Alarm "
            "sofort aus, auch wenn über das Zeitfenster noch nicht genug "
            "zusammengekommen ist. Auf \"aus\" zählt nur das Zeitfenster.");
        form->addRow("Eine Nachricht ab", single);

        auto *streak =
            this->createSpinBox(getSettings()->emoteAlertStreak, 0, 50);
        streak->setSuffix(" Nachrichten");
        streak->setSpecialValueText("aus");
        streak->setToolTip(
            "So viele Nachrichten desselben Users hintereinander, in denen "
            "nichts als Emotes stehen, lösen den Alarm aus - auch wenn es "
            "jedes Mal nur ein oder zwei Emotes sind. Eine Nachricht mit "
            "einem Wort darin beendet die Reihe.");
        form->addRow("Nur Emotes hintereinander", streak);

        emotes->addLayout(form);
    }

    addHeading(emotes, "Stufen");
    {
        addText(emotes,
                "Was der Alarm jeweils anbietet: löschen, eine "
                "Timeout-Dauer wie 30s, 10m, 1d - oder bann für einen "
                "dauerhaften Bann. Durch Kommas getrennt. Die nächste Stufe "
                "kommt erst, wenn wirklich eine Nachricht gelöscht oder der "
                "User getimeoutet wurde.",
                true);

        auto *form = new QFormLayout;
        auto *deleteCount = this->createSpinBox(
            getSettings()->emoteAlertDeleteCount, 0,
            EmoteSpamDetector::MOST_DELETED);
        deleteCount->setSuffix(" Nachrichten");
        deleteCount->setSpecialValueText("alle gezählten");
        deleteCount->setToolTip(
            "Wie viele Nachrichten der Löschen-Knopf wegnimmt: die neuesten "
            "so vielen. Auf \"alle gezählten\" nimmt er alle, die im "
            "Zeitfenster zusammengezählt wurden - höchstens 30.");
        form->addRow("Löschen nimmt", deleteCount);
        emotes->addLayout(form);

        addStepsEditor(emotes, getSettings()->emoteAlertSteps,
                       QStringLiteral("löschen, löschen, 30s"),
                       &EmoteSpamDetector::parseSteps, [](int step) {
                           if (step == EmoteSpamDetector::DELETE)
                           {
                               return QStringLiteral("Löschen");
                           }
                           return step == 0 ? QStringLiteral("Bann")
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

    // ----- Wörter -----
    addText(words,
            "Ein Alarm für Wörter, die du im Chat nicht sehen willst. Schreib "
            "sie hier untereinander - aus jedem Wort wird von selbst ein "
            "Muster gebaut, das es auch abgewandelt findet: sybau, syb4u, "
            "s.y.b.a.u, mit kyrillischen Buchstaben.");
    addText(words,
            "Eingeschaltet wird der Alarm pro Kanal über den Schild-Knopf in "
            "diesem Kanal.",
            true);

    addHeading(words, "Wörter");
    {
        auto *rows = new WordRows;
        words->addWidget(rows);

        auto *add = new QPushButton("Wort hinzufügen");
        QObject::connect(add, &QPushButton::clicked, this, [rows] {
            rows->addEmptyRow();
        });
        addButtonRow(words, add);

        addText(words,
                "Die Knöpfe hinter einem Wort sagen, was der Alarm für "
                "dieses Wort vorschlägt - in der Reihenfolge, in der sie "
                "stehen. Ist keiner gedrückt, gelten die Stufen weiter "
                "unten, die für alle Wörter zählen.",
                true);

        auto *preview = new QLabel;
        preview->setWordWrap(true);
        preview->setEnabled(false);
        words->addWidget(preview);

        // The same as it would look in chat, rather than as a list of
        // spellings
        auto *lines = new ChannelView(this, ChannelView::Context::UserCard, 8);
        lines->setMinimumHeight(86);
        lines->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto shown = std::make_shared<Channel>("vorschau", Channel::Type::None);
        lines->setChannel(shown);
        words->addWidget(lines);

        // What the words find, so it is clear before it goes live
        const auto show = [preview, shown] {
            const auto list = WordAlertDetector::parseWords(
                getSettings()->wordAlertWords.getValue(),
                getSettings()->wordAlertVariants,
                getSettings()->wordAlertWholeWord);
            shown->clearMessages();
            if (list.empty())
            {
                preview->setText(
                    "Noch keine Wörter - solange passiert hier nichts.");
                return;
            }

            const spelling::Options options{
                .leet = getSettings()->wordAlertVariants,
                .lookalikes = getSettings()->wordAlertVariants,
                .stretched = getSettings()->wordAlertVariants,
                .separated = getSettings()->wordAlertVariants,
                .wholeWord = getSettings()->wordAlertWholeWord,
            };
            const auto examples = spelling::examples(list.front().word,
                                                     options);
            preview->setText(
                QStringLiteral("%1 Wörter. So würde „%2\" im Chat aussehen - "
                               "jede dieser Zeilen löst den Alarm aus:")
                    .arg(list.size())
                    .arg(list.front().word));

            // A few of the spellings, as they would stand in chat
            int drawn = 0;
            for (const auto &example : examples)
            {
                if (drawn++ >= 3)
                {
                    break;
                }
                shown->addMessage(
                    previewLine(example.note.isEmpty()
                                    ? example.text
                                    : QStringLiteral("%1   (%2)")
                                          .arg(example.text, example.note)),
                    MessageContext::Original);
            }
        };
        show();
        getSettings()->wordAlertWords.connect(
            [show](const QString &, auto) {
                show();
            },
            this->managedConnections_, false);

        auto *variants = this->createCheckBox(
            "Auch abgewandelte Schreibweisen finden",
            getSettings()->wordAlertVariants,
            "Findet das Wort auch mit Ziffern statt Buchstaben, mit Punkten "
            "dazwischen, gestreckt oder mit Buchstaben, die genauso aussehen. "
            "Aus heißt: nur genau so geschrieben.");
        QObject::connect(variants, &QCheckBox::toggled, this, [show] {
            show();
        });
        words->addWidget(variants);

        auto *wholeWord = this->createCheckBox(
            "Nur als ganzes Wort", getSettings()->wordAlertWholeWord,
            "An heißt: „ass\" schlägt nicht bei „Klasse\" an. Aus findet das "
            "Wort auch mitten in einem längeren.");
        QObject::connect(wholeWord, &QCheckBox::toggled, this, [show] {
            show();
        });
        words->addWidget(wholeWord);
    }

    addHeading(words, "Stufen");
    {
        addText(words,
                "Was der Alarm jeweils anbietet: löschen, eine "
                "Timeout-Dauer wie 5m, 1h, 1d - oder bann für einen "
                "dauerhaften Bann. Durch Kommas getrennt. Die nächste Stufe "
                "kommt erst, wenn wirklich gelöscht oder getimeoutet wurde.",
                true);

        auto *form = new QFormLayout;
        auto *deleteCount = this->createSpinBox(
            getSettings()->wordAlertDeleteCount, 0,
            WordAlertDetector::MOST_DELETED);
        deleteCount->setSuffix(" Nachrichten");
        deleteCount->setSpecialValueText("alle gefundenen");
        deleteCount->setToolTip(
            "Wie viele Nachrichten der Löschen-Knopf wegnimmt: die neuesten "
            "so vielen, in denen ein Wort von der Liste stand.");
        form->addRow("Löschen nimmt", deleteCount);
        words->addLayout(form);

        addStepsEditor(words, getSettings()->wordAlertSteps,
                       QStringLiteral("5m, 10m, 30m, 1h, 1d"),
                       &WordAlertDetector::parseSteps, [](int step) {
                           if (step == WordAlertDetector::DELETE)
                           {
                               return QStringLiteral("Löschen");
                           }
                           return step == 0 ? QStringLiteral("Bann")
                                            : formatTime(step);
                       });
    }

    addColor(words, getSettings()->modAlertColorWord,
             ModAlertPopup::Kind::Word);
    addSound(words, getSettings()->modAlertSoundWord);

    addHeading(words, "Testen");
    {
        auto *test = new QPushButton("Test-Alarm für ein Wort anzeigen");
        test->setToolTip(
            "Öffnet den Wort-Alarm mit ausgedachten Nachrichten, mit dem "
            "ersten Wort deiner Liste. Jeder Klick geht eine Stufe weiter. "
            "Die Knöpfe tun nichts.");
        QObject::connect(test, &QPushButton::clicked, this, [openTest] {
            static int step = 0;
            const int current = step;
            step = (step + 1) % 3;

            openTest([current](ModAlertPopup *popup) {
                popup->showTestWordAlert(current);
            });
        });
        addButtonRow(words, test);
    }
    words->addStretch(1);

    // ----- Shared Chat -----
    addText(shared,
            "Bei Shared Chat zeigt Twitch die Chats mehrerer Kanäle als "
            "einen, moderiert wird aber weiter jeder für sich: Ein Timeout, "
            "den du in einem Kanal gibst, gilt nur dort. In den anderen "
            "Kanälen der Runde schreibt die Person weiter.");
    addText(shared,
            "Mit dem Haken gibt ChattiFlexii denselben Timeout, Bann oder "
            "das Aufheben gleich noch einmal - in jedem weiteren Kanal der "
            "Runde, den du hier offen hast und in dem du Mod oder Streamer "
            "bist. Was dabei herauskommt, steht danach im Chat.");

    addHeading(shared, "Verhalten");
    shared->addWidget(this->createCheckBox(
        "Timeouts und Banns auf die ganze Shared-Chat-Runde anwenden",
        getSettings()->sharedChatCarryOver,
        "Gilt für jeden Weg: den Knopf im Alarm-Fenster, die Usercard und "
        "/timeout, /ban und /untimeout von Hand. Kanäle, die du hier nicht "
        "offen hast, bleiben außen vor - dorthin würde sonst etwas gehen, "
        "das du nicht siehst."));
    addText(shared,
            "Twitch selbst bietet das nicht an: Jede Strafe geht einzeln an "
            "einen Kanal. ChattiFlexii schickt sie also nacheinander an jeden "
            "Kanal der Runde - was ein anderer Mod schon gemacht hat, siehst "
            "du ohnehin im Chat, dort steht dann der Kanal dabei.",
            true);
    shared->addStretch(1);
}

bool ModAssistantPage::filterElements(const QString &query)
{
    static const QStringList keywords{
        "mod",        "assistent", "assistant", "moderation", "spam",
        "emote",      "alarm",     "alert",     "vorschlag",  "timeout",
        "wiederholt", "fenster",   "reason",    "farbe",      "position",
        "shared",     "chat",      "runde",
    };

    return matchesPageText(this, query) ||
           matchesKeywords(query, keywords);
}

}  // namespace chatterino
