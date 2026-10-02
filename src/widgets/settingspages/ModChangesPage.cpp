// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ModChangesPage.hpp"

#include "controllers/moderation/ModChanges.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "singletons/Settings.hpp"
#include "widgets/settingspages/PageSections.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace chatterino {

using namespace pagesections;

ModChangesPage::ModChangesPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(6);

    auto *layout = outer;
    auto &s = *getSettings();

    addText(layout,
            "Ein eigener Tab, der meldet, wer in den Kanälen, die du unter "
            "„Allgemein“ ausgewählt hast, neu Mod geworden ist - und wer es "
            "nicht mehr ist. Er vergleicht die Mod-Listen von whosthemod.xyz "
            "mit denen vom letzten Mal; solange er an ist, jede Viertelstunde "
            "statt alle sechs Stunden. Was er gemeldet hat, bleibt auch nach "
            "einem Neustart im Tab stehen.");
    addText(layout,
            "Wie schnell eine Änderung auftaucht, hängt davon ab, wann "
            "whosthemod.xyz sie selbst bemerkt. Ein Kanal, der gerade erst "
            "dazukommt, meldet beim ersten Mal nichts - dann ist seine Liste "
            "der Stand, mit dem verglichen wird.",
            true);

    layout->addWidget(this->createCheckBox(
        "Mod-Änderungen melden", s.modChangesEnabled,
        "Öffnet den Tab „Mod-Änderungen“ und schaut ab jetzt jede "
        "Viertelstunde nach."));
    // Ticked, the tab is there at once
    s.modChangesEnabled.connect(
        [](const bool enabled, auto) {
            if (enabled)
            {
                ModChanges::openTab();
            }
        },
        this->managedConnections_, false);

    {
        auto *row = new QWidget;
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *open = new QPushButton("Tab öffnen");
        auto *now = new QPushButton("Jetzt nachsehen");
        now->setToolTip("Holt die Mod-Listen gleich, statt auf die nächste "
                        "Viertelstunde zu warten");
        QObject::connect(open, &QPushButton::clicked, [] {
            ModChanges::openTab();
        });
        QObject::connect(now, &QPushButton::clicked, [] {
            ModChanges::openTab();
            ModChanges::instance().checkNow();
        });
        rowLayout->addWidget(open);
        rowLayout->addWidget(now);
        rowLayout->addStretch(1);
        layout->addWidget(row);
    }

    addHeading(layout, "Was gemeldet wird");
    layout->addWidget(this->createCheckBox(
        "Bots weglassen", s.modChangesHideBots,
        "Wie unter „Bots ausschließen“: die Namen dort und, wenn angehakt, "
        "alle, die auf „bot“ enden."));
    layout->addWidget(this->createCheckBox(
        "Ton bei einer Meldung", s.modChangesSound,
        "Kommen mehrere zusammen, spielt der Ton für „neu Mod“, sonst der "
        "für „nicht mehr Mod“."));
    {
        const auto soundRow = [this, layout, &s](const QString &name,
                                                 QStringSetting &setting) {
            auto *row = new QWidget;
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(20, 0, 0, 0);
            auto *label = new QLabel(name + ":");
            label->setMinimumWidth(130);
            rowLayout->addWidget(label);
            rowLayout->addWidget(
                soundChooser(row, setting, this->managedConnections_));
            layout->addWidget(row);

            s.modChangesSound.connect(
                [row](const bool on, auto) {
                    row->setEnabled(on);
                },
                this->managedConnections_);
        };
        soundRow("Neu Mod", s.modChangesSoundAdded);
        soundRow("Nicht mehr Mod", s.modChangesSoundRemoved);
    }

    addHeading(layout, "Farben");
    layout->addWidget(this->createCheckBox(
        "Meldungen farbig hinterlegen", s.modChangesColored,
        "Neu Mod und nicht mehr Mod je auf ihrer Farbe. Gilt für Meldungen "
        "ab jetzt."));
    const auto colorRow = [this, layout, &s](const QString &name,
                                             QStringSetting &setting) {
        auto *row = new QWidget;
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(20, 0, 0, 0);
        rowLayout->addWidget(new QLabel(name + ":"));
        rowLayout->addStretch(1);
        auto *button = new ColorButton(QColor(setting.getValue()));
        button->setFixedSize(50, 24);
        rowLayout->addWidget(button);
        layout->addWidget(row);

        QObject::connect(button, &ColorButton::clicked, [button, &setting] {
            auto *dialog = new ColorPickerDialog(QColor(setting), button);
            QObject::connect(dialog, &ColorPickerDialog::colorConfirmed, button,
                             [&setting](const QColor &picked) {
                                 if (picked.isValid())
                                 {
                                     setting.setValue(
                                         picked.name(QColor::HexArgb));
                                 }
                             });
            dialog->show();
        });
        setting.connect(
            [button](const QString &value, auto) {
                button->setColor(QColor(value));
            },
            this->managedConnections_, false);
        s.modChangesColored.connect(
            [row](const bool colored, auto) {
                row->setEnabled(colored);
            },
            this->managedConnections_);
    };
    colorRow("Neu Mod", s.modChangesColorAdded);
    colorRow("Nicht mehr Mod", s.modChangesColorRemoved);

    auto *standard = new QPushButton("Standard");
    standard->setToolTip("Was gemeldet wird und die Farben wieder so wie am "
                         "Anfang");
    QObject::connect(standard, &QPushButton::clicked, [&s] {
        for (auto *setting :
             {&s.modChangesHideBots, &s.modChangesSound, &s.modChangesColored})
        {
            setting->setValue(setting->getDefaultValue());
        }
        for (auto *sound : {&s.modChangesSoundAdded, &s.modChangesSoundRemoved})
        {
            sound->setValue(sound->getDefaultValue());
        }
        s.modChangesColorAdded.setValue(s.modChangesColorAdded.getDefaultValue());
        s.modChangesColorRemoved.setValue(
            s.modChangesColorRemoved.getDefaultValue());
    });
    addButtonRow(layout, standard);
    layout->addStretch(1);

    layout->addStretch();
}

bool ModChangesPage::filterElements(const QString & /*query*/)
{
    // Nothing on this page is searched through yet
    return true;
}

}  // namespace chatterino
