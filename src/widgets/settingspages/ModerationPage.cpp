// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/Window.hpp"
#include "singletons/WindowManager.hpp"
#include "controllers/moderation/EmoteSpamDetector.hpp"
#include "widgets/dialogs/ModAlertPopup.hpp"
#include "util/FormatTime.hpp"
#include "controllers/moderation/RepeatSpamDetector.hpp"
#include "widgets/settingspages/ModerationPage.hpp"

#include "Application.hpp"
#include "controllers/logging/ChannelLoggingModel.hpp"
#include "controllers/moderationactions/ModerationAction.hpp"
#include "controllers/moderationactions/ModerationActionModel.hpp"
#include "singletons/Logging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/Helpers.hpp"
#include "util/LayoutCreator.hpp"
#include "util/LoadPixmap.hpp"
#include "util/PostToThread.hpp"
#include "widgets/helper/EditableModelView.hpp"
#include "widgets/helper/IconDelegate.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <functional>
#include <QTimer>
#include <QCheckBox>
#include <QFrame>
#include <QScrollArea>
#include <QLineEdit>
#include <QFormLayout>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTableView>
#include <QtConcurrent/QtConcurrent>

namespace chatterino {

qint64 dirSize(QString &dirPath)
{
    QDirIterator it(dirPath, QDirIterator::Subdirectories);
    qint64 size = 0;

    while (it.hasNext())
    {
        size += it.fileInfo().size();
        it.next();
    }

    return size;
}

QString formatSize(qint64 size)
{
    QStringList units = {"Bytes", "KB", "MB", "GB", "TB", "PB"};
    int i;
    double outputSize = size;
    for (i = 0; i < units.size() - 1; i++)
    {
        if (outputSize < 1024)
        {
            break;
        }
        outputSize = outputSize / 1024;
    }
    return QString("%0 %1").arg(outputSize, 0, 'f', 2).arg(units[i]);
}

QString fetchLogDirectorySize()
{
    QString logsDirectoryPath = getSettings()->logPath.getValue().isEmpty()
                                    ? getApp()->getPaths().messageLogDirectory
                                    : getSettings()->logPath;

    auto logsSize = dirSize(logsDirectoryPath);

    return QString("Your logs currently take up %1 of space")
        .arg(formatSize(logsSize));
}

ModerationPage::ModerationPage()
{
    LayoutCreator<ModerationPage> layoutCreator(this);

    auto tabs = layoutCreator.emplace<QTabWidget>();
    this->tabWidget_ = tabs.getElement();

    auto logs = tabs.appendTab(new QVBoxLayout, "Logs");
    {
        QCheckBox *enableLogging = this->createCheckBox(
            "Enable logging", getSettings()->enableLogging);
        logs.append(enableLogging);

        auto logsPathLabel = logs.emplace<QLabel>();

        QString logExplanation =
            "<span style=\"color:#bbb\"> They are saved as plain "
            "text files per channel, containing the messages with "
            "timestamps.</span>";

        // Logs (copied from LoggingMananger)
        getSettings()->logPath.connect(
            [logsPathLabel, logExplanation](const QString &logPath,
                                            auto) mutable {
                QString pathOriginal =
                    logPath.isEmpty() ? getApp()->getPaths().messageLogDirectory
                                      : logPath;

                QString pathShortened =
                    "Logs are saved at <a href=\"file:///" + pathOriginal +
                    R"("><span style="color: white;">)" +
                    shortenString(pathOriginal, 50) + ".</span></a>";

                logsPathLabel->setText(pathShortened + logExplanation);
                logsPathLabel->setToolTip(pathOriginal);
                logsPathLabel->setWordWrap(true);
            });

        logsPathLabel->setTextFormat(Qt::RichText);
        logsPathLabel->setTextInteractionFlags(Qt::TextBrowserInteraction |
                                               Qt::LinksAccessibleByKeyboard);
        logsPathLabel->setOpenExternalLinks(true);

        auto buttons = logs.emplace<QHBoxLayout>().withoutMargin();

        // Select and Reset
        auto selectDir = buttons.emplace<QPushButton>("Select log directory ");
        auto resetDir = buttons.emplace<QPushButton>("Reset");

        getSettings()->logPath.connect(
            [element = resetDir.getElement()](const QString &path) {
                element->setEnabled(!path.isEmpty());
            });

        buttons->addStretch();

        // Show how big (size-wise) the logs are
        auto logsPathSizeLabel = logs.emplace<QLabel>();
        this->logsPathSizeLabel_ = logsPathSizeLabel.getElement();
        this->refreshLogDirectorySize();

        // Select event
        QObject::connect(selectDir.getElement(), &QPushButton::clicked, this,
                         [this]() mutable {
                             auto dirName =
                                 QFileDialog::getExistingDirectory(this);

                             getSettings()->logPath = dirName;

                             // Refresh: Show how big (size-wise) the logs are
                             this->refreshLogDirectorySize();
                         });

        buttons->addSpacing(16);

        // Reset custom logpath
        QObject::connect(resetDir.getElement(), &QPushButton::clicked, this,
                         [this]() mutable {
                             getSettings()->logPath = "";

                             // Refresh: Show how big (size-wise) the logs are
                             this->refreshLogDirectorySize();
                         });

        auto logsTimestampFormatLayout =
            logs.emplace<QHBoxLayout>().withoutMargin();
        auto logsTimestampFormatLabel =
            logsTimestampFormatLayout.emplace<QLabel>();
        logsTimestampFormatLabel->setText(
            QString("Log file timestamp format: "));

        QComboBox *logTimestampFormat = this->createComboBox(
            {"Disable", "h:mm", "hh:mm", "h:mm a", "hh:mm a", "h:mm:ss",
             "hh:mm:ss", "h:mm:ss a", "hh:mm:ss a", "h:mm:ss.zzz",
             "h:mm:ss.zzz a", "hh:mm:ss.zzz", "hh:mm:ss.zzz a"},
            getSettings()->logTimestampFormat);
        logTimestampFormat->setToolTip("a = am/pm, zzz = milliseconds");
        logsTimestampFormatLayout.append(logTimestampFormat);

        SettingWidget::checkbox("Use Twitch's timestamps",
                                getSettings()->tryUseTwitchTimestamps)
            ->setTooltip(
                "Try to use Twitch's timestamp (the time when the message was "
                "received by Twitch's chat server), rather than your "
                "computer's local timestamp.\nNote that using this setting can "
                "result in out-of-order timestamps in the log files, and that "
                "if Twitch's timestamp was unavailable for a message, it will "
                "fall back to your computer's local timestamp.")
            ->conditionallyEnabledBy(getSettings()->enableLogging)
            ->addToLayout(logs->layout());

        QCheckBox *onlyLogListedChannels =
            this->createCheckBox("Only log channels listed below",
                                 getSettings()->onlyLogListedChannels);

        onlyLogListedChannels->setEnabled(getSettings()->enableLogging);
        logs.append(onlyLogListedChannels);

        auto *separatelyStoreStreamLogs =
            this->createCheckBox("Store live stream logs as separate files",
                                 getSettings()->separatelyStoreStreamLogs);

        separatelyStoreStreamLogs->setEnabled(getSettings()->enableLogging);
        logs.append(separatelyStoreStreamLogs);

        // Select event
        QObject::connect(
            enableLogging, &QCheckBox::stateChanged, this,
            [enableLogging, onlyLogListedChannels,
             separatelyStoreStreamLogs]() mutable {
                onlyLogListedChannels->setEnabled(enableLogging->isChecked());
                separatelyStoreStreamLogs->setEnabled(
                    getSettings()->enableLogging);
            });

        EditableModelView *view =
            logs.emplace<EditableModelView>(
                    (new ChannelLoggingModel(nullptr))
                        ->initialized(&getSettings()->loggedChannels))
                .getElement();

        view->setTitles({"Twitch channels"});
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Fixed);
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Stretch);

        // We can safely ignore this signal connection since we own the view
        std::ignore = view->addButtonPressed.connect([] {
            getSettings()->loggedChannels.append(ChannelLog("channel"));
        });

    }  // logs end

    // Everything the assistant and its alerts can be told outgrows the
    // window, so the tab scrolls instead of squeezing its rows together
    auto assistantTab = tabs.appendTab(new QVBoxLayout, "Assistent");
    auto *assistantScroll = new QScrollArea;
    assistantScroll->setWidgetResizable(true);
    assistantScroll->setFrameShape(QFrame::NoFrame);
    assistantScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    assistantScroll->viewport()->setAutoFillBackground(false);
    auto *assistantContent = new QWidget;
    assistantContent->setAutoFillBackground(false);
    auto *assistantLayout = new QVBoxLayout(assistantContent);
    assistantScroll->setWidget(assistantContent);
    assistantTab->addWidget(assistantScroll);
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
        assistant->addLayout(alertsForm);

        auto *delayTests =
            new QCheckBox("Test-Fenster erst nach 5 Sekunden öffnen");
        delayTests->setToolTip(
            "Damit du nach dem Klick in ein anderes Programm wechseln und "
            "prüfen kannst, ob das Fenster dort vorne aufgeht.");
        assistant.append(delayTests);

        // Opens a test window, right away or after the delay above
        const auto openTest =
            [this, delayTests](std::function<void(ModAlertPopup *)> fill) {
                const bool delayed = delayTests->isChecked();
                const auto open = [this, delayed, fill] {
                    // A delayed one sits on the main window like a real alert,
                    // so it behaves the same with the settings out of sight
                    auto *parent =
                        delayed ? static_cast<QWidget *>(
                                      &getApp()->getWindows()->getMainWindow())
                                : static_cast<QWidget *>(this);
                    auto *popup = new ModAlertPopup("test", "testuser", parent);
                    fill(popup);
                    popup->show();
                    popup->raise();
                };

                if (delayed)
                {
                    QTimer::singleShot(5000, this, open);
                }
                else
                {
                    open();
                }
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

    auto modMode = tabs.appendTab(new QVBoxLayout, "Moderation buttons");
    {
        // clang-format off
        auto label = modMode.emplace<QLabel>(
            "Moderation mode is enabled by clicking <img width='18' height='18' src=':/buttons/moderationDisabledDarkMode18x18.png'> in a channel that you moderate.<br><br>"
            "Moderation buttons can be bound to chat commands such as \"/ban {user.name}\", \"/timeout {user.name} 1000\", \"/w someusername !report {user.name} was bad in channel {channel.name}\" or any other custom text commands.<br>"
            "For deleting messages use /delete {msg.id}.<br><br>"
            "More information can be found <a href='https://wiki.chatterino.com/Moderation/#moderation-mode'>here</a>.");
        label->setOpenExternalLinks(true);
        label->setWordWrap(true);
        label->setStyleSheet("color: #bbb");
        // clang-format on

        //        auto form = modMode.emplace<QFormLayout>();
        //        {
        //            form->addRow("Action on timed out messages
        //            (unimplemented):",
        //                         this->createComboBox({"Disable", "Hide"},
        //                         getSettings()->timeoutAction));
        //        }

        EditableModelView *view =
            modMode
                .emplace<EditableModelView>(
                    (new ModerationActionModel(nullptr))
                        ->initialized(&getSettings()->moderationActions))
                .getElement();

        view->setTitles({"Action", "Icon"});
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Fixed);
        view->getTableView()->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Stretch);
        view->getTableView()->setItemDelegateForColumn(
            ModerationActionModel::Column::Icon, new IconDelegate(view));
        QObject::connect(
            view->getTableView(), &QTableView::clicked,
            [this, view](const QModelIndex &clicked) {
                if (clicked.column() == ModerationActionModel::Column::Icon)
                {
                    auto fileUrl = QFileDialog::getOpenFileUrl(
                        this, "Open Image", QUrl(),
                        "Image Files (*.png *.jpg *.jpeg)");
                    view->getModel()->setData(clicked, fileUrl, Qt::UserRole);
                    view->getModel()->setData(clicked, fileUrl.fileName(),
                                              Qt::DisplayRole);
                    // Clear the icon if the user canceled the dialog
                    if (fileUrl.isEmpty())
                    {
                        view->getModel()->setData(clicked, QVariant(),
                                                  Qt::DecorationRole);
                    }
                    else
                    {
                        // QPointer will be cleared when view is destroyed
                        QPointer<EditableModelView> viewtemp = view;

                        loadPixmapFromUrl(
                            {fileUrl.toString()},
                            [clicked, view = viewtemp](const QPixmap &pixmap) {
                                postToThread([clicked, view, pixmap]() {
                                    if (view.isNull())
                                    {
                                        return;
                                    }

                                    view->getModel()->setData(
                                        clicked, pixmap, Qt::DecorationRole);
                                });
                            });
                    }
                }
            });

        // We can safely ignore this signal connection since we own the view
        std::ignore = view->addButtonPressed.connect([] {
            getSettings()->moderationActions.append(
                ModerationAction("/timeout {user.name} 300"));
        });
    }

    this->addModerationButtonSettings(tabs.getElement());

    // ---- misc
    this->itemsChangedTimer_.setSingleShot(true);
}

void ModerationPage::addModerationButtonSettings(QTabWidget *tabs)
{
    auto timeoutLayout =
        LayoutCreator{tabs}.appendTab(new QVBoxLayout, "User Timeout Buttons");
    auto texts = timeoutLayout.emplace<QVBoxLayout>().withoutMargin();
    {
        auto infoLabel = texts.emplace<QLabel>();
        infoLabel->setText(
            "Customize the timeout buttons in the user popup (accessible "
            "through clicking a username).\nUse seconds (s), "
            "minutes (m), hours (h), days (d) or weeks (w).");

        infoLabel->setAlignment(Qt::AlignCenter);

        auto maxLabel = texts.emplace<QLabel>();
        maxLabel->setText("(maximum timeout duration = 2 w)");
        maxLabel->setAlignment(Qt::AlignCenter);
    }
    texts->setContentsMargins(0, 0, 0, 15);
    texts->setSizeConstraint(QLayout::SetMaximumSize);

    const auto valueChanged = [=, this] {
        const auto index = QObject::sender()->objectName().toInt();

        auto *const line = this->durationInputs_[index];
        const auto duration = line->text().toInt();
        const auto unit = this->unitInputs_[index]->currentText();

        // safety mechanism for setting days and weeks
        if (unit == "d" && duration > 14)
        {
            line->setText("14");
            return;
        }
        else if (unit == "w" && duration > 2)
        {
            line->setText("2");
            return;
        }

        auto timeouts = getSettings()->timeoutButtons.getValue();
        timeouts[index] = TimeoutButton{unit, duration};
        getSettings()->timeoutButtons.setValue(timeouts);
    };

    // build one line for each customizable button
    auto i = 0;
    for (const auto &tButton : getSettings()->timeoutButtons.getValue())
    {
        const auto buttonNumber = QString::number(i);
        auto timeout = timeoutLayout.emplace<QHBoxLayout>().withoutMargin();

        auto buttonLabel = timeout.emplace<QLabel>();
        buttonLabel->setText(QString("Button %1: ").arg(++i));

        auto *lineEditDurationInput = new QLineEdit();
        lineEditDurationInput->setObjectName(buttonNumber);
        lineEditDurationInput->setValidator(new QIntValidator(1, 99, this));
        lineEditDurationInput->setText(QString::number(tButton.second));
        lineEditDurationInput->setAlignment(Qt::AlignRight);
        lineEditDurationInput->setMaximumWidth(30);
        timeout.append(lineEditDurationInput);

        auto *timeoutDurationUnit = new QComboBox();
        timeoutDurationUnit->setObjectName(buttonNumber);
        timeoutDurationUnit->addItems({"s", "m", "h", "d", "w"});
        timeoutDurationUnit->setCurrentText(tButton.first);
        timeout.append(timeoutDurationUnit);

        QObject::connect(lineEditDurationInput, &QLineEdit::textChanged, this,
                         valueChanged);

        QObject::connect(timeoutDurationUnit, &QComboBox::currentTextChanged,
                         this, valueChanged);

        timeout->addStretch();

        this->durationInputs_.push_back(lineEditDurationInput);
        this->unitInputs_.push_back(timeoutDurationUnit);

        timeout->setContentsMargins(40, 0, 0, 0);
        timeout->setSizeConstraint(QLayout::SetMaximumSize);
    }
    timeoutLayout->addStretch();
}

void ModerationPage::selectModerationActions()
{
    this->tabWidget_->setCurrentIndex(1);
}

void ModerationPage::refreshLogDirectorySize()
{
    if (this->logsPathSizeLabel_ == nullptr)
    {
        return;
    }

    this->logsPathSizeLabel_->setText(QtConcurrent::run([] {
                                          return fetchLogDirectorySize();
                                      }).result());
}

void ModerationPage::onShow()
{
    this->refreshLogDirectorySize();
}

}  // namespace chatterino
