// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ModAssistantPage.hpp"

#include "Application.hpp"
#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "controllers/moderation/ModHighlights.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/FormatTime.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/Window.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace chatterino {

namespace {

/// One tab of the page. Each scrolls on its own, so a long one never squeezes
/// its rows together.
QVBoxLayout *addPageTab(QTabWidget *tabs, const QString &title)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto *content = new QWidget;
    content->setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);
    scroll->setWidget(content);

    tabs->addTab(scroll, title);
    return layout;
}

void addHeading(QVBoxLayout *layout, const QString &text)
{
    if (layout->count() > 0)
    {
        layout->addSpacing(12);
    }
    auto *heading = new QLabel(text);
    auto font = heading->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.1);
    heading->setFont(font);
    layout->addWidget(heading);
}

QLabel *addText(QVBoxLayout *layout, const QString &text, bool dimmed = false)
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    if (dimmed)
    {
        label->setEnabled(false);
    }
    layout->addWidget(label);
    return label;
}

void addButtonRow(QVBoxLayout *layout, QWidget *widget)
{
    auto *row = new QHBoxLayout;
    row->addWidget(widget);
    row->addStretch(1);
    layout->addLayout(row);
}

/// Channel pictures for the mod highlights list, by login, cut round
QHash<QString, QPixmap> &channelPictures()
{
    static QHash<QString, QPixmap> pictures;
    return pictures;
}

/// Pictures already on their way, so each is asked for once
QSet<QString> &picturesAsked()
{
    static QSet<QString> asked;
    return asked;
}

QPixmap roundPicture(const QPixmap &source, int side)
{
    QPixmap round(side, side);
    round.fill(Qt::transparent);
    QPainter painter(&round);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(source.scaled(side, side, Qt::IgnoreAspectRatio,
                                          Qt::SmoothTransformation)));
    painter.drawEllipse(QRectF(0, 0, side, side));
    return round;
}

/// A login as Twitch spells it, from whatever was typed - "#Zarbex" gives
/// "zarbex"
QString typedLogin(const QString &text)
{
    QString login;
    for (const auto c : text.trimmed().toLower())
    {
        if ((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') || c == u'_')
        {
            login.append(c);
        }
    }
    return login;
}

QString channelText(const ModHighlightChannel &channel)
{
    QStringList parts{channel.displayName.isEmpty() ? channel.login
                                                    : channel.displayName};
    if (channel.modCount >= 0)
    {
        parts.append(channel.modCount == 1
                         ? QStringLiteral("1 Mod")
                         : QStringLiteral("%1 Mods").arg(channel.modCount));
    }
    if (channel.live)
    {
        parts.append(QStringLiteral("LIVE"));
    }
    return parts.join(QStringLiteral("  ·  "));
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
    auto *modHighlights = addPageTab(tabs, "Mod-Highlights");
    const int modHighlightsTab = tabs->count() - 1;

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
        "Spielt den Hinweiston, sobald ein neues Alarm-Fenster aufgeht - "
        "nicht, wenn ein offenes nur aktualisiert wird."));
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

        auto *steps = new QLineEdit(getSettings()->repeatAlertSteps.getValue());
        steps->setPlaceholderText("30s, 1m, 5m, 10m, 30m");
        auto *preview = new QLabel;
        preview->setTextFormat(Qt::RichText);
        preview->setWordWrap(true);

        const auto showSteps = [preview](const QString &text) {
            const auto parsed = RepeatSpamDetector::parseSteps(text);
            if (parsed.empty())
            {
                preview->setText(QStringLiteral(
                    "<span style=\"color:#e05050\">Keine gültige Liste - "
                    "schreib zum Beispiel 30s, 1m, 5m. Bis dahin gilt die "
                    "letzte gültige Liste.</span>"));
                return;
            }

            QStringList shown;
            for (const auto seconds : parsed)
            {
                shown.append(formatTime(seconds));
            }
            preview->setText(shown.join(QStringLiteral(" → ")));
        };
        showSteps(steps->text());

        QObject::connect(steps, &QLineEdit::textChanged, preview, showSteps);
        // Only a list that reads is kept, so a half typed one never ends up
        // deciding a timeout
        QObject::connect(steps, &QLineEdit::editingFinished, steps, [steps] {
            if (!RepeatSpamDetector::parseSteps(steps->text()).empty())
            {
                getSettings()->repeatAlertSteps.setValue(
                    steps->text().trimmed());
            }
        });

        repeats->addWidget(steps);
        repeats->addWidget(preview);
    }

    addColor(repeats, getSettings()->modAlertColorRepeat,
             ModAlertPopup::Kind::RepeatedMessage);

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

        auto *steps = new QLineEdit(getSettings()->emoteAlertSteps.getValue());
        steps->setPlaceholderText("löschen, löschen, 30s");
        auto *preview = new QLabel;
        preview->setTextFormat(Qt::RichText);
        preview->setWordWrap(true);

        const auto showSteps = [preview](const QString &text) {
            const auto parsed = EmoteSpamDetector::parseSteps(text);
            if (parsed.empty())
            {
                preview->setText(QStringLiteral(
                    "<span style=\"color:#e05050\">Keine gültige Liste - "
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
            preview->setText(shown.join(QStringLiteral(" → ")));
        };
        showSteps(steps->text());

        QObject::connect(steps, &QLineEdit::textChanged, preview, showSteps);
        QObject::connect(steps, &QLineEdit::editingFinished, steps, [steps] {
            if (!EmoteSpamDetector::parseSteps(steps->text()).empty())
            {
                getSettings()->emoteAlertSteps.setValue(steps->text().trimmed());
            }
        });
        emotes->addWidget(steps);
        emotes->addWidget(preview);
    }

    addColor(emotes, getSettings()->modAlertColorEmote,
             ModAlertPopup::Kind::EmoteSpam);

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

    // ----- Mod-Highlights -----
    this->buildModHighlights(modHighlights);
    // The channel list is asked for the first time the tab is opened, not on
    // every visit to the settings
    QObject::connect(tabs, &QTabWidget::currentChanged, this,
                     [this, modHighlightsTab](int index) {
                         if (index == modHighlightsTab &&
                             !this->modChannelsAsked_)
                         {
                             this->modChannelsAsked_ = true;
                             this->searchModChannels(false);
                         }
                     });
}

void ModAssistantPage::onShow()
{
    // The plugin may have been switched on or off in the meantime
    this->showModHighlightsState();
}

void ModAssistantPage::buildModHighlights(QVBoxLayout *layout)
{
    ModHighlights::instance().start();

    addText(layout,
            "Markiert Nachrichten von Mods der Kanäle, die du hier auswählst, "
            "mit den Profilbildern dieser Kanäle - wer bei mehreren davon Mod "
            "ist, bekommt alle ihre Bilder. Die Mod-Listen kommen von "
            "whosthemod.xyz und werden alle sechs Stunden aktualisiert.");
    this->modPluginNote_ =
        addText(layout,
                "<span style=\"color:#ffaa00\">Braucht das Plugin WhoseTheMod. "
                "Schalte unter Einstellungen → Plugins die Plugins ein und "
                "aktiviere WhoseTheMod, dann ist dieser Reiter frei.</span>");

    this->modHighlightsBody_ = new QWidget;
    auto *body = new QVBoxLayout(this->modHighlightsBody_);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(6);
    layout->addWidget(this->modHighlightsBody_, 1);

    body->addWidget(this->createCheckBox(
        "Mods der ausgewählten Kanäle markieren",
        getSettings()->modHighlightsEnabled));

    addHeading(body, "Aussehen");
    body->addWidget(this->createCheckBox(
        "Nachricht einfärben", getSettings()->modHighlightsColorEnabled,
        "Ohne Farbe bekommt die Nachricht nur die Profilbilder am Ende der "
        "Zeile."));
    {
        auto *button =
            new ColorButton(QColor(getSettings()->modHighlightsColor.getValue()));
        getSettings()->modHighlightsColor.connect(
            [button](const QString &value, auto) {
                button->setColor(QColor(value));
            },
            this->managedConnections_);
        getSettings()->modHighlightsColorEnabled.connect(
            [button](const bool &on, auto) {
                button->setEnabled(on);
            },
            this->managedConnections_);
        QObject::connect(button, &ColorButton::clicked, this, [this] {
            auto *dialog = new ColorPickerDialog(
                QColor(getSettings()->modHighlightsColor.getValue()), this);
            QObject::connect(dialog, &ColorPickerDialog::colorConfirmed, this,
                             [](QColor picked) {
                                 if (picked.isValid())
                                 {
                                     getSettings()->modHighlightsColor.setValue(
                                         picked.name(QColor::HexArgb));
                                 }
                             });
            dialog->show();
        });

        auto *row = new QHBoxLayout;
        row->addWidget(button);
        row->addStretch(1);
        auto *form = new QFormLayout;
        form->addRow("Farbe", row);
        body->addLayout(form);
    }
    addText(body,
            "Hat jemand schon ein eigenes User- oder Badge-Highlight, behält "
            "er dessen Farbe und Caption, und die Profilbilder kommen dahinter.",
            true);

    addHeading(body, "Kanäle");
    this->modSearch_ = new QLineEdit;
    this->modSearch_->setPlaceholderText("Kanal suchen …");
    this->modSearch_->setClearButtonEnabled(true);
    body->addWidget(this->modSearch_);
    this->modListNote_ = addText(body, QString(), true);
    this->modListNote_->hide();

    this->modChannels_ = new QListWidget;
    this->modChannels_->setIconSize(QSize(28, 28));
    this->modChannels_->setMinimumHeight(240);
    this->modChannels_->setSpacing(2);
    body->addWidget(this->modChannels_, 1);

    this->modMore_ = new QPushButton("Mehr laden");
    this->modMore_->hide();
    addButtonRow(body, this->modMore_);

    auto *statusRow = new QHBoxLayout;
    this->modStatus_ = new QLabel;
    this->modStatus_->setWordWrap(true);
    statusRow->addWidget(this->modStatus_, 1);
    auto *refresh = new QPushButton("Jetzt aktualisieren");
    refresh->setToolTip(
        "Holt die Mod-Listen der ausgewählten Kanäle sofort neu von "
        "whosthemod.xyz.");
    statusRow->addWidget(refresh);
    body->addLayout(statusRow);

    this->modSearchDelay_ = new QTimer(this);
    this->modSearchDelay_->setSingleShot(true);
    this->modSearchDelay_->setInterval(350);
    QObject::connect(this->modSearchDelay_, &QTimer::timeout, this, [this] {
        this->searchModChannels(false);
    });
    QObject::connect(this->modSearch_, &QLineEdit::textChanged, this, [this] {
        this->modSearchDelay_->start();
    });
    QObject::connect(this->modSearch_, &QLineEdit::returnPressed, this,
                     [this] {
                         this->addTypedModChannel();
                     });
    QObject::connect(this->modMore_, &QPushButton::clicked, this, [this] {
        this->searchModChannels(true);
    });
    QObject::connect(refresh, &QPushButton::clicked, this, [this] {
        this->modStatus_->setText("Wird aktualisiert …");
        ModHighlights::instance().refresh();
    });

    // Ticking a channel picks it, unticking lets it go
    QObject::connect(
        this->modChannels_, &QListWidget::itemChanged, this,
        [this](QListWidgetItem *item) {
            if (this->fillingModChannels_)
            {
                return;
            }
            const auto login = item->data(Qt::UserRole).toString();
            auto channels = getSettings()->modHighlightChannels.getValue();
            const auto it = std::find(channels.begin(), channels.end(), login);
            const bool ticked = item->checkState() == Qt::Checked;
            if (ticked && it == channels.end())
            {
                channels.push_back(login);
            }
            else if (!ticked && it != channels.end())
            {
                channels.erase(it);
            }
            else
            {
                return;
            }
            getSettings()->modHighlightChannels.setValue(channels);
        });

    this->managedConnections_.managedConnect(
        ModHighlights::instance().updated, [this] {
            this->showModHighlightsState();
            if (!this->siteListAvailable_)
            {
                this->showChosenModChannels();
            }
        });

    this->showModHighlightsState();
}

void ModAssistantPage::showModHighlightsState()
{
    if (this->modHighlightsBody_ == nullptr)
    {
        return;
    }

    const bool available = ModHighlights::pluginAvailable();
    this->modPluginNote_->setVisible(!available);
    this->modHighlightsBody_->setEnabled(available);

    const auto &highlights = ModHighlights::instance();
    const auto channels = getSettings()->modHighlightChannels.getValue();
    if (channels.empty())
    {
        this->modStatus_->setText("Noch kein Kanal ausgewählt.");
        return;
    }

    const auto updated = highlights.lastUpdated();
    this->modStatus_->setText(
        QStringLiteral("%1 %2 ausgewählt  ·  %3 Mods markiert  ·  Stand %4")
            .arg(channels.size())
            .arg(channels.size() == 1 ? QStringLiteral("Kanal")
                                      : QStringLiteral("Kanäle"))
            .arg(highlights.markedCount())
            .arg(updated.isValid()
                     ? QLocale(QLocale::German)
                           .toString(updated.toLocalTime(),
                                     QStringLiteral("dd.MM. HH:mm"))
                     : QStringLiteral("noch nicht geladen")));
}

void ModAssistantPage::searchModChannels(bool more)
{
    const auto query = this->modSearch_->text().trimmed();
    ModHighlights::instance().searchChannels(
        query, more ? this->modCursor_ : QString(), this,
        [this, more, query](auto channels, const QString &next) {
            // An answer to a search typed over since is of no use
            if (query != this->modSearch_->text().trimmed())
            {
                return;
            }

            if (!channels)
            {
                this->siteListAvailable_ = false;
                this->modCursor_.clear();
                this->modMore_->hide();
                this->showChosenModChannels();
                return;
            }

            this->siteListAvailable_ = true;
            this->modCursor_ = next;
            this->modMore_->setVisible(!next.isEmpty());
            if (!more)
            {
                this->modListNote_->setText(
                    "Kein Kanal gefunden. Enter prüft den Namen direkt.");
                this->modListNote_->setVisible(channels->empty() &&
                                               !query.isEmpty());
            }
            this->fillModChannels(*channels, more, query.isEmpty());
        });
}

void ModAssistantPage::showChosenModChannels()
{
    const auto query = typedLogin(this->modSearch_->text());
    std::vector<ModHighlightChannel> chosen;
    for (const auto &login : getSettings()->modHighlightChannels.getValue())
    {
        if (query.isEmpty() || login.contains(query))
        {
            ModHighlightChannel channel;
            channel.login = login;
            channel.modCount = ModHighlights::instance().modCount(login);
            chosen.push_back(channel);
        }
    }

    this->modListNote_->setText(
        "Die Liste aller Kanäle von whosthemod.xyz ist noch nicht online. Bis "
        "dahin: Kanalnamen eintippen und Enter drücken - er wird direkt "
        "geprüft und hinzugefügt.");
    this->modListNote_->show();
    this->fillModChannels(chosen, false, false);
}

void ModAssistantPage::fillModChannels(
    const std::vector<ModHighlightChannel> &channels, bool append,
    bool chosenFirst)
{
    const auto chosen = getSettings()->modHighlightChannels.getValue();
    const auto isChosen = [&chosen](const QString &login) {
        return std::find(chosen.begin(), chosen.end(), login) != chosen.end();
    };

    // The picked channels lead an unfiltered list, whether or not they are on
    // its first page
    std::vector<ModHighlightChannel> ordered;
    if (chosenFirst)
    {
        for (const auto &login : chosen)
        {
            const auto found =
                std::find_if(channels.begin(), channels.end(),
                             [&login](const ModHighlightChannel &channel) {
                                 return channel.login == login;
                             });
            if (found != channels.end())
            {
                ordered.push_back(*found);
            }
            else
            {
                ModHighlightChannel channel;
                channel.login = login;
                channel.modCount = ModHighlights::instance().modCount(login);
                ordered.push_back(channel);
            }
        }
    }
    for (const auto &channel : channels)
    {
        if (!chosenFirst || !isChosen(channel.login))
        {
            ordered.push_back(channel);
        }
    }

    this->fillingModChannels_ = true;
    if (!append)
    {
        this->modChannels_->clear();
    }
    QSet<QString> shown;
    for (int i = 0; i < this->modChannels_->count(); i++)
    {
        shown.insert(this->modChannels_->item(i)->data(Qt::UserRole).toString());
    }

    for (const auto &channel : ordered)
    {
        if (shown.contains(channel.login))
        {
            continue;
        }
        shown.insert(channel.login);

        auto *item = new QListWidgetItem(channelText(channel), this->modChannels_);
        item->setData(Qt::UserRole, channel.login);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(isChosen(channel.login) ? Qt::Checked
                                                     : Qt::Unchecked);
        this->showModChannelPicture(channel.login, channel.profileImageUrl);
    }
    this->fillingModChannels_ = false;
}

void ModAssistantPage::addTypedModChannel()
{
    const auto login = typedLogin(this->modSearch_->text());
    if (login.isEmpty())
    {
        return;
    }

    // A channel the list already shows is ticked right there
    for (int i = 0; i < this->modChannels_->count(); i++)
    {
        auto *item = this->modChannels_->item(i);
        if (item->data(Qt::UserRole).toString() == login)
        {
            item->setCheckState(Qt::Checked);
            return;
        }
    }

    this->modListNote_->setText(QStringLiteral("Prüfe #%1 …").arg(login));
    this->modListNote_->show();
    ModHighlights::instance().checkChannel(
        login, this, [this, login](int count) {
            if (count < 0)
            {
                this->modListNote_->setText(
                    QStringLiteral("#%1 konnte nicht geprüft werden - versuch "
                                   "es gleich nochmal.")
                        .arg(login));
                return;
            }
            if (count == 0)
            {
                this->modListNote_->setText(
                    QStringLiteral("#%1 hat bei whosthemod keine bekannten "
                                   "Mods.")
                        .arg(login));
                return;
            }

            auto channels = getSettings()->modHighlightChannels.getValue();
            if (std::find(channels.begin(), channels.end(), login) ==
                channels.end())
            {
                channels.push_back(login);
                getSettings()->modHighlightChannels.setValue(channels);
            }
            this->modSearch_->clear();
            this->modListNote_->setText(
                QStringLiteral("#%1 hinzugefügt  ·  %2 Mods")
                    .arg(login)
                    .arg(count));
            this->modListNote_->show();
            if (!this->siteListAvailable_)
            {
                this->showChosenModChannels();
            }
        });
}

void ModAssistantPage::showModChannelPicture(const QString &login,
                                             const QString &url)
{
    const auto applyPicture = [this](const QString &forLogin) {
        const auto picture = channelPictures().value(forLogin);
        if (picture.isNull() || this->modChannels_ == nullptr)
        {
            return;
        }
        for (int i = 0; i < this->modChannels_->count(); i++)
        {
            auto *item = this->modChannels_->item(i);
            if (item->data(Qt::UserRole).toString() == forLogin)
            {
                item->setIcon(QIcon(picture));
            }
        }
    };

    if (channelPictures().contains(login))
    {
        applyPicture(login);
        return;
    }
    const auto key = login + u'\n' + url;
    if (picturesAsked().contains(key))
    {
        return;
    }
    picturesAsked().insert(key);

    if (url.isEmpty())
    {
        // Not in the site's answer - Twitch has it
        QPointer<ModAssistantPage> self(this);
        getHelix()->fetchUsers(
            {}, {login},
            [self, login](const std::vector<HelixUser> &users) {
                if (!self || users.empty() ||
                    users.front().profileImageUrl.isEmpty())
                {
                    return;
                }
                auto picture = users.front().profileImageUrl;
                self->showModChannelPicture(
                    login, picture.replace(QStringLiteral("300x300"),
                                           QStringLiteral("70x70")));
            },
            [] {});
        return;
    }

    static auto *manager = new QNetworkAccessManager;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "Chatterino");
    auto *reply = manager->get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply,
                     &QObject::deleteLater);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [reply, login, applyPicture] {
                         QPixmap pixmap;
                         if (reply->error() != QNetworkReply::NoError ||
                             !pixmap.loadFromData(reply->readAll()))
                         {
                             return;
                         }
                         channelPictures().insert(login,
                                                  roundPicture(pixmap, 56));
                         applyPicture(login);
                     });
}

bool ModAssistantPage::filterElements(const QString &query)
{
    static const QStringList keywords{
        "mod",        "assistent", "assistant", "moderation", "spam",
        "emote",      "alarm",     "alert",     "vorschlag",  "timeout",
        "wiederholt", "fenster",   "reason",    "farbe",      "position",
        "whosthemod", "highlight", "kanal",
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
