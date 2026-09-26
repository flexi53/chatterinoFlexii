// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/AdvancedPage.hpp"

#include "controllers/people/WatchedPeople.hpp"
#include "singletons/Settings.hpp"
#include "widgets/settingspages/ModAssistantPage.hpp"
#include "widgets/settingspages/PageSections.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace chatterino {

using namespace pagesections;

AdvancedPage::AdvancedPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // The two halves as they were, each as its own tab
    this->tabs_ = new QTabWidget;
    outer->addWidget(this->tabs_);

    this->assistant_ = new ModAssistantPage;
    this->tabs_->addTab(this->assistant_, "Mod-Assistent");
    this->buildPeopleTab(addPageTab(this->tabs_, "User"));
}

bool AdvancedPage::filterElements(const QString &query)
{
    // Whichever half holds what is looked for brings the page up
    return matchesPageText(this, query) ||
           this->assistant_->filterElements(query) ||
           matchesKeywords(query, {
                                      "erweitert",
                                      "user",
                                      "leute",
                                      "im blick",
                                      "mod",
                                      "assistent",
                                      "alarm",
                                  });
}

void AdvancedPage::onShow()
{
    this->showPeople();
}

void AdvancedPage::buildPeopleTab(QVBoxLayout *layout)
{
    auto &s = *getSettings();

    addText(layout,
            "Ein eigener Tab, in dem steht, was ausgewählte Leute schreiben - "
            "aus jedem Kanal, den du gerade offen hast, mit dem Kanal neben "
            "jeder Zeile. So siehst du, wo sie sich herumtreiben, ohne jeden "
            "Tab im Auge zu behalten.");
    addText(layout,
            "Gesammelt wird nur, was hereinkommt, solange ChattiFlexii läuft "
            "und der Kanal offen ist - nichts aus der Vergangenheit, und "
            "nach einem Neustart fängt der Tab wieder leer an.",
            true);
    addText(layout,
            "Dahinter steckt Chatterinos eigene Technik: Jeder Name hier "
            "bekommt unter Highlights → Users einen stillen Eintrag (ohne "
            "Farbe, ohne Ton), damit seine Nachrichten in den "
            "Erwähnungen-Kanal wandern, und der Tab ist ein "
            "Erwähnungen-Tab mit dem Filter unten darauf. Schaltest du das "
            "hier ab, verschwinden diese Einträge wieder.",
            true);

    addHeading(layout, "Einschalten");
    layout->addWidget(this->createCheckBox(
        "Leute im Blick behalten", s.watchedPeopleEnabled,
        "Ohne den Haken bleibt der Tab leer und es wird nichts gesammelt."));
    layout->addWidget(this->createCheckBox(
        "Eintrag im Rechtsklick-Menü einer Nachricht", s.watchedPeopleMenu,
        "Damit nimmst du jemanden direkt aus dem Chat in die Liste auf - oder "
        "wieder heraus."));
    {
        auto *open = new QPushButton("Tab öffnen");
        QObject::connect(open, &QPushButton::clicked, [] {
            WatchedPeople::openTab();
        });
        addButtonRow(layout, open);
    }

    addHeading(layout, "Wer");
    addText(layout, "Ein Name pro Zeile, Kommas gehen auch.", true);
    {
        this->peopleList_ =
            new QPlainTextEdit(WatchedPeople::write(WatchedPeople::people()));
        this->peopleList_->setPlaceholderText("sonkertd\nzarbex\n…");
        this->peopleList_->setFixedHeight(
            this->peopleList_->fontMetrics().lineSpacing() * 8 + 16);

        // Saved once typing pauses, not on every key
        auto *save = new QTimer(this->peopleList_);
        save->setSingleShot(true);
        save->setInterval(500);
        QObject::connect(save, &QTimer::timeout, this->peopleList_, [this] {
            getSettings()->watchedPeople.setValue(
                this->peopleList_->toPlainText().trimmed());
            this->showPeople();
        });
        QObject::connect(this->peopleList_, &QPlainTextEdit::textChanged, save,
                         qOverload<>(&QTimer::start));
        layout->addWidget(this->peopleList_);
    }
    this->peopleStatus_ = addText(layout, QString(), true);

    addHeading(layout, "Oder ein Filter");
    addText(layout,
            "Wer mehr will als Namen, schreibt hier einen Filter in "
            "derselben Sprache wie unter Einstellungen → Filters. Steht hier "
            "etwas, gilt nur der Filter und die Liste oben bleibt "
            "unberührt.",
            true);
    {
        this->peopleFilter_ = new QLineEdit(
            getSettings()->watchedPeopleFilter.getValue());
        this->peopleFilter_->setPlaceholderText(
            R"((author.name == "fx_flexii") || (author.name == "ardaslegacy"))");
        this->peopleFilter_->setClearButtonEnabled(true);
        layout->addWidget(this->peopleFilter_);

        this->filterStatus_ = addText(layout, QString(), true);

        const auto pruefe = [this] {
            const auto written = this->peopleFilter_->text().trimmed();
            if (written.isEmpty())
            {
                this->filterStatus_->setText(
                    "Kein Filter - es gilt die Liste oben.");
                return;
            }
            const auto problem = WatchedPeople::problemWith(written);
            this->filterStatus_->setText(
                problem.isEmpty()
                    ? QStringLiteral("Filter sitzt.")
                    : QStringLiteral(
                          "<span style=\"color:#ffaa00\">%1</span>")
                          .arg(problem.toHtmlEscaped()));
        };

        // Gespeichert wird erst, wenn er hält - sonst stünde beim Tippen
        // ständig ein halber Ausdruck in den Einstellungen
        auto *save = new QTimer(this->peopleFilter_);
        save->setSingleShot(true);
        save->setInterval(500);
        QObject::connect(save, &QTimer::timeout, this->peopleFilter_,
                         [this, pruefe] {
                             const auto written =
                                 this->peopleFilter_->text().trimmed();
                             if (written.isEmpty() ||
                                 WatchedPeople::problemWith(written).isEmpty())
                             {
                                 getSettings()->watchedPeopleFilter.setValue(
                                     written);
                             }
                             pruefe();
                         });
        QObject::connect(this->peopleFilter_, &QLineEdit::textChanged, save,
                         qOverload<>(&QTimer::start));

        auto *fromList = new QPushButton("Aus der Liste erzeugen");
        fromList->setToolTip("Schreibt die Namen von oben als Filter hin - "
                             "als Anfang zum Weiterbauen");
        QObject::connect(fromList, &QPushButton::clicked, this,
                         [this, pruefe] {
                             const auto people = WatchedPeople::read(
                                 this->peopleList_->toPlainText());
                             if (people.isEmpty())
                             {
                                 return;
                             }
                             this->peopleFilter_->setText(
                                 WatchedPeople::expressionFor(people));
                             pruefe();
                         });
        addButtonRow(layout, fromList);
        pruefe();
    }

    // Someone added from the chat while this page is open
    s.watchedPeople.connect(
        [this](const QString &written, auto) {
            if (this->peopleList_ == nullptr ||
                this->peopleList_->hasFocus())
            {
                return;
            }
            const QSignalBlocker blocker(this->peopleList_);
            this->peopleList_->setPlainText(
                WatchedPeople::write(WatchedPeople::read(written)));
            this->showPeople();
        },
        this->managedConnections_, false);

    auto *standard = new QPushButton("Standard");
    standard->setToolTip("Wieder aus, Liste bleibt");
    QObject::connect(standard, &QPushButton::clicked, [this, &s] {
        s.watchedPeopleEnabled.setValue(
            s.watchedPeopleEnabled.getDefaultValue());
        s.watchedPeopleMenu.setValue(s.watchedPeopleMenu.getDefaultValue());
        s.watchedPeopleFilter.setValue(
            s.watchedPeopleFilter.getDefaultValue());
        if (this->peopleFilter_ != nullptr)
        {
            this->peopleFilter_->clear();
        }
    });
    addButtonRow(layout, standard);
    layout->addStretch(1);
}


void AdvancedPage::showPeople()
{
    if (this->peopleStatus_ == nullptr)
    {
        return;
    }
    const auto count = WatchedPeople::people().size();
    this->peopleStatus_->setText(
        count == 0   ? QStringLiteral("Noch niemand ausgewählt.")
        : count == 1 ? QStringLiteral("1 Person im Blick.")
                     : QStringLiteral("%1 Leute im Blick.").arg(count));
}


}  // namespace chatterino
