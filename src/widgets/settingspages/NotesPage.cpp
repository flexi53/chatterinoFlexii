// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/NotesPage.hpp"

#include "Application.hpp"
#include "controllers/people/WatchedPeople.hpp"
#include "controllers/saved/SavedMessages.hpp"
#include "controllers/userdata/UserDataController.hpp"
#include "controllers/userdata/UserNotes.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "singletons/Settings.hpp"
#include "widgets/dialogs/ColorPickerDialog.hpp"
#include "widgets/dialogs/EditUserNotesDialog.hpp"
#include "widgets/helper/color/ColorButton.hpp"
#include "widgets/settingspages/PageSections.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace chatterino {

using namespace pagesections;

namespace {

/// How many ids Twitch answers for in one request
constexpr int MOST_PER_REQUEST = 100;

}  // namespace

NotesPage::NotesPage()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    this->tabs_ = new QTabWidget;
    outer->addWidget(this->tabs_, 1);
    this->buildUsersTab(addPageTab(this->tabs_, "Zu Usern"));
    this->buildSavedTab(addPageTab(this->tabs_, "Gemerkte Nachrichten"));
    this->buildPeopleTab(addPageTab(this->tabs_, "Leute im Blick"));

    // Once the page is up, rather than while the dialog is still being built
    QTimer::singleShot(0, this, [this] {
        this->onShow();
    });
}

void NotesPage::buildUsersTab(QVBoxLayout *layout)
{
    auto &s = *getSettings();

    addText(layout,
            "Eine Notiz schreibst du in der Karte einer Person: klick im Chat "
            "auf den Namen und dann auf „Add notes“. Sie bleibt auf diesem "
            "Computer und geht niemanden sonst etwas an. Hier stehen alle "
            "beieinander.");

    addHeading(layout, "Im Chat");
    layout->addWidget(this->createCheckBox(
        QStringLiteral("Zeichen %1 vor dem Namen, wenn etwas notiert ist")
            .arg(usernotes::MARK),
        s.userNotesMark,
        "Fahr mit der Maus darüber, dann steht die Notiz da; ein Klick öffnet "
        "die Karte. Das Zeichen wird beim Ankommen in die Nachricht "
        "geschrieben - es steht also ab der nächsten Nachricht, nicht bei "
        "denen, die schon im Chat stehen."));

    addHeading(layout, "Notizen");
    this->search_ = new QLineEdit;
    this->search_->setPlaceholderText("Suchen - Name oder Text der Notiz");
    this->search_->setClearButtonEnabled(true);
    layout->addWidget(this->search_);
    QObject::connect(this->search_, &QLineEdit::textChanged, this, [this] {
        this->showNotes();
    });

    this->notes_ = new QListWidget;
    this->notes_->setAlternatingRowColors(true);
    this->notes_->setMinimumHeight(220);
    layout->addWidget(this->notes_, 1);

    this->status_ = addText(layout, QString(), true);

    {
        this->editButton_ = new QPushButton("Bearbeiten");
        this->deleteButton_ = new QPushButton("Löschen");
        this->editButton_->setEnabled(false);
        this->deleteButton_->setEnabled(false);

        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(this->editButton_);
        row->addWidget(this->deleteButton_);
        row->addStretch(1);
        auto *rowWidget = new QWidget;
        rowWidget->setLayout(row);
        layout->addWidget(rowWidget);

        const auto pickedId = [this]() -> QString {
            auto *item = this->notes_->currentItem();
            return item == nullptr ? QString()
                                   : item->data(Qt::UserRole).toString();
        };

        QObject::connect(this->notes_, &QListWidget::itemSelectionChanged, this,
                         [this] {
                             const bool picked =
                                 this->notes_->currentItem() != nullptr;
                             this->editButton_->setEnabled(picked);
                             this->deleteButton_->setEnabled(picked);
                         });
        QObject::connect(this->notes_, &QListWidget::itemDoubleClicked, this,
                         [this](QListWidgetItem *item) {
                             this->edit(item->data(Qt::UserRole).toString());
                         });
        QObject::connect(this->editButton_, &QPushButton::clicked, this,
                         [this, pickedId] {
                             const auto userId = pickedId();
                             if (!userId.isEmpty())
                             {
                                 this->edit(userId);
                             }
                         });
        QObject::connect(
            this->deleteButton_, &QPushButton::clicked, this, [this, pickedId] {
                const auto userId = pickedId();
                if (userId.isEmpty())
                {
                    return;
                }
                const auto name =
                    this->names_.value(userId, QStringLiteral("dieser Person"));
                auto *ask = new QMessageBox(
                    QMessageBox::Question, "Notiz löschen",
                    QStringLiteral("Die Notiz zu %1 löschen?").arg(name),
                    QMessageBox::Yes | QMessageBox::No, this);
                ask->setAttribute(Qt::WA_DeleteOnClose);
                QObject::connect(
                    ask, &QMessageBox::finished, this, [this, userId](int answer) {
                        if (answer == QMessageBox::Yes)
                        {
                            getApp()->getUserData()->setUserNotes(userId, {});
                            this->showNotes();
                        }
                    });
                ask->open();
            });
    }

    addText(layout,
            "Doppelklick öffnet dieselbe Notiz wie die Karte. Löschen "
            "entfernt nur die Notiz - eine eigene Farbe für die Person "
            "bleibt.",
            true);

    auto *standard = new QPushButton("Standard");
    standard->setToolTip("Das Zeichen im Chat wieder so wie am Anfang - "
                         "deine Notizen bleiben");
    QObject::connect(standard, &QPushButton::clicked, [&s] {
        s.userNotesMark.setValue(s.userNotesMark.getDefaultValue());
    });
    addButtonRow(layout, standard);

    // Someone noted from a card while this page is open
    this->managedConnections_.managedConnect(
        getApp()->getUserData()->userDataUpdated(), [this] {
            this->showNotes();
        });
}

void NotesPage::buildSavedTab(QVBoxLayout *layout)
{
    auto &s = *getSettings();

    addText(layout,
            "Rechtsklick auf eine Nachricht → „Merken“ legt sie im Tab "
            "„Gemerkt“ ab - mit Kanal, Namen und Uhrzeit. Der Tab geht dabei "
            "von selbst auf. Ein Klick auf „[vergessen]“ hinter einem "
            "Eintrag wirft ihn wieder raus, der Name öffnet die Karte, der "
            "Kanal springt dorthin.");

    addHeading(layout, "Wie du merkst");
    layout->addWidget(this->createCheckBox(
        "„Merken“ im Rechtsklick-Menü", s.savedMessagesMenu,
        "Ohne den Haken bleibt alles Gemerkte im Tab, nur der Eintrag im "
        "Menü ist weg."));

    addHeading(layout, "Farbe");
    layout->addWidget(this->createCheckBox(
        "Einträge farbig hinterlegen", s.savedMessagesColored,
        "Damit die gemerkten Nachrichten im Tab auffallen. Gilt ab dem "
        "nächsten Aufräumen des Tabs - also sobald du etwas merkst oder "
        "vergisst."));
    {
        auto *row = new QWidget;
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(20, 0, 0, 0);
        rowLayout->addWidget(new QLabel("Hintergrund:"));
        rowLayout->addStretch(1);
        auto *button = new ColorButton(QColor(s.savedMessagesColor.getValue()));
        button->setFixedSize(50, 24);
        rowLayout->addWidget(button);
        layout->addWidget(row);

        QObject::connect(button, &ColorButton::clicked, [this, button] {
            auto *dialog = new ColorPickerDialog(
                QColor(getSettings()->savedMessagesColor.getValue()), button);
            QObject::connect(dialog, &ColorPickerDialog::colorConfirmed, button,
                             [](const QColor &picked) {
                                 if (picked.isValid())
                                 {
                                     getSettings()->savedMessagesColor.setValue(
                                         picked.name(QColor::HexArgb));
                                 }
                             });
            dialog->show();
        });
        s.savedMessagesColor.connect(
            [button](const QString &value, auto) {
                button->setColor(QColor(value));
            },
            this->managedConnections_, false);
        s.savedMessagesColored.connect(
            [row](const bool colored, auto) {
                row->setEnabled(colored);
            },
            this->managedConnections_);
    }

    addHeading(layout, "Der Tab");
    {
        auto *open = new QPushButton("Tab öffnen");
        auto *clear = new QPushButton("Alle vergessen");
        clear->setToolTip("Wirft alles Gemerkte weg - das lässt sich nicht "
                          "zurückholen");
        QObject::connect(open, &QPushButton::clicked, [] {
            SavedMessages::openTab();
        });
        QObject::connect(clear, &QPushButton::clicked, this, [this] {
            auto *ask = new QMessageBox(
                QMessageBox::Question, "Alle vergessen",
                QStringLiteral("Alle %1 gemerkten Nachrichten wegwerfen?")
                    .arg(SavedMessages::instance().kept().size()),
                QMessageBox::Yes | QMessageBox::No, this);
            ask->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(ask, &QMessageBox::finished, this,
                             [this](int answer) {
                                 if (answer == QMessageBox::Yes)
                                 {
                                     SavedMessages::instance().forgetAll();
                                     this->showKept();
                                 }
                             });
            ask->open();
        });

        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(open);
        row->addWidget(clear);
        row->addStretch(1);
        auto *rowWidget = new QWidget;
        rowWidget->setLayout(row);
        layout->addWidget(rowWidget);
    }
    this->keptStatus_ = addText(layout, QString(), true);

    auto *standard = new QPushButton("Standard");
    standard->setToolTip("Menü-Eintrag und Farbe wieder so wie am Anfang - "
                         "was du gemerkt hast, bleibt");
    QObject::connect(standard, &QPushButton::clicked, [&s] {
        s.savedMessagesMenu.setValue(s.savedMessagesMenu.getDefaultValue());
        s.savedMessagesColored.setValue(
            s.savedMessagesColored.getDefaultValue());
        s.savedMessagesColor.setValue(s.savedMessagesColor.getDefaultValue());
    });
    addButtonRow(layout, standard);
    layout->addStretch(1);
}

void NotesPage::buildPeopleTab(QVBoxLayout *layout)
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
    QObject::connect(standard, &QPushButton::clicked, [&s] {
        s.watchedPeopleEnabled.setValue(
            s.watchedPeopleEnabled.getDefaultValue());
        s.watchedPeopleMenu.setValue(s.watchedPeopleMenu.getDefaultValue());
    });
    addButtonRow(layout, standard);
    layout->addStretch(1);
}

void NotesPage::showPeople()
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

void NotesPage::showKept()
{
    if (this->keptStatus_ == nullptr)
    {
        return;
    }
    const auto count = SavedMessages::instance().kept().size();
    this->keptStatus_->setText(
        count == 0 ? QStringLiteral("Nichts gemerkt.")
        : count == 1
            ? QStringLiteral("1 Nachricht gemerkt.")
            : QStringLiteral("%1 Nachrichten gemerkt.").arg(count));
}

void NotesPage::showNotes()
{
    auto entries = usernotes::all();
    std::sort(entries.begin(), entries.end(),
              [this](const auto &lhs, const auto &rhs) {
                  return this->names_.value(lhs.userId, lhs.userId).compare(
                             this->names_.value(rhs.userId, rhs.userId),
                             Qt::CaseInsensitive) < 0;
              });

    const auto query = this->search_->text().trimmed();
    const auto picked = this->notes_->currentItem() == nullptr
                            ? QString()
                            : this->notes_->currentItem()
                                  ->data(Qt::UserRole)
                                  .toString();

    this->notes_->clear();
    int shown = 0;
    for (const auto &entry : entries)
    {
        const auto name = this->names_.value(entry.userId);
        const auto line = usernotes::oneLine(entry.note);
        if (!query.isEmpty() &&
            !name.contains(query, Qt::CaseInsensitive) &&
            !entry.note.contains(query, Qt::CaseInsensitive))
        {
            continue;
        }

        auto *item = new QListWidgetItem(
            QStringLiteral("%1  ·  %2")
                .arg(name.isEmpty() ? QStringLiteral("Name wird geladen")
                                    : name,
                     line));
        item->setData(Qt::UserRole, entry.userId);
        item->setToolTip(entry.note);
        this->notes_->addItem(item);
        if (entry.userId == picked)
        {
            this->notes_->setCurrentItem(item);
        }
        shown++;
    }

    const auto all = static_cast<int>(entries.size());
    if (all == 0)
    {
        this->status_->setText("Noch nichts notiert.");
    }
    else if (shown == all)
    {
        this->status_->setText(all == 1 ? "1 Notiz"
                                        : QStringLiteral("%1 Notizen").arg(all));
    }
    else
    {
        this->status_->setText(
            QStringLiteral("%1 von %2 Notizen").arg(shown).arg(all));
    }

    this->fetchNames();
}

void NotesPage::fetchNames()
{
    if (this->asking_)
    {
        return;
    }

    QStringList unknown;
    for (const auto &entry : usernotes::all())
    {
        if (!this->names_.contains(entry.userId))
        {
            unknown.append(entry.userId);
        }
        if (unknown.size() >= MOST_PER_REQUEST)
        {
            break;
        }
    }
    if (unknown.isEmpty())
    {
        return;
    }

    this->asking_ = true;
    getHelix()->fetchUsers(
        unknown, {},
        [this, unknown](const std::vector<HelixUser> &users) {
            this->asking_ = false;
            for (const auto &user : users)
            {
                this->names_[user.id] = user.displayName.isEmpty()
                                            ? user.login
                                            : user.displayName;
            }
            // Ids Twitch knows nothing about - a deleted account, say - keep
            // their number, so they are not asked for again and again
            for (const auto &id : unknown)
            {
                if (!this->names_.contains(id))
                {
                    this->names_[id] = QStringLiteral("Unbekannt (%1)").arg(id);
                }
            }
            this->showNotes();
        },
        [this] {
            this->asking_ = false;
        });
}

void NotesPage::edit(const QString &userId)
{
    if (userId.isEmpty())
    {
        return;
    }

    auto *dialog = new EditUserNotesDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    const auto user = getApp()->getUserData()->getUser(userId);
    dialog->setNotes(user ? user->notes : QString());
    dialog->updateWindowTitle(this->names_.value(userId, userId));
    std::ignore = dialog->onOk.connect([this, userId](const QString &written) {
        getApp()->getUserData()->setUserNotes(userId, written);
        this->showNotes();
    });
    dialog->show();
}

void NotesPage::onShow()
{
    this->showNotes();
    this->showKept();
    this->showPeople();
}

bool NotesPage::filterElements(const QString &query)
{
    return matchesPageText(this, query) || matchesKeywords(query, {
                                      "notiz",
                                      "notizen",
                                      "note",
                                      "user",
                                      "merken",
                                      "gemerkt",
                                      "nachricht",
                                      "zeichen",
                                  });
}

}  // namespace chatterino
