// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/WhatsNew.hpp"

#include "Application.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Window.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include <array>

namespace {

struct Release {
    /// What is remembered as seen. It never changes once a release is out -
    /// the first ones were keyed by their English date, and changing a key
    /// would show everyone everything again.
    const char *id;
    /// Dated rather than numbered - the fork does not carry a version of its
    /// own, and a date says more about when something landed anyway.
    const char *date;
    std::initializer_list<const char *> changes;
};

/// Newest first. Add to the top when something lands that is worth telling
/// the user about; everything above what they last saw is shown at once.
const std::array<Release, 2> RELEASES{{
    {"13 September 2026",
     "13. September 2026",
     {
         "Ein Mod-Assistent hinter dem Schild-Knopf neben dem Emote-Knopf in "
         "Kanälen, in denen du Mod bist. Er lernt aus den Timeouts und Banns "
         "dort, und auf „Vorschlagen“ gestellt öffnet eine Nachricht, die "
         "früheren Fällen ähnelt, ein Fenster mit der Aktion, die Mods "
         "meistens gegeben haben, und dem Grund dafür - es passiert nichts, "
         "solange du nicht auf den Knopf drückst.",
         "Farben für die Tab-Leiste und die Tabs auf der Look-Seite - "
         "Hintergrund, ausgewählter Tab und Farbverlauf, in beiden Looks.",
         "Benenne im Farbwähler die Farben, die du immer wieder nimmst - dann "
         "bekommt mit einem Klick auf „Mods“ jeder Mod dieselbe Farbe.",
         "Ein Alarm für User, die dieselbe - oder fast dieselbe - Nachricht "
         "dreimal hintereinander schicken, mit ihren Nachrichten, den Timeouts "
         "dazwischen und einem Timeout-Knopf, der jedes Mal eine Stufe höher "
         "geht (anfangs 30s, 1m, 5m, 10m, 30m, einstellbar). Pro Kanal im "
         "Mod-Assistenten einzuschalten.",
         "Ein Alarm für Emote-Spam - die Emotes werden über kurze Zeit "
         "zusammengezählt, sodass auch kurze Schwälle zählen: anfangs löschen, "
         "löschen, dann 30 Sekunden Timeout, genauso einstellbar.",
         "Etwas mehr Abstand zwischen den Tab-Reihen.",
     }},
    {"8 September 2026",
     "8. September 2026",
     {
         "Tab-Gruppen: Tabs unter einer benannten Überschrift sammeln, "
         "einklappen, eine ganze Gruppe auf einmal einfärben, per Ziehen "
         "verschieben und so festhalten, dass sie auch bei „nur Live-Tabs“ "
         "sichtbar bleibt.",
         "Eine Caption-Spalte auf allen Highlight-Seiten. Was du dort "
         "einträgst, steht neben passenden Nachrichten am Ende der letzten "
         "Zeile.",
         "Das Label für erste Nachrichten ist jetzt eine Caption wie jede "
         "andere - es steht in der Zeile First Messages und ist anfangs "
         "FIRST.",
         "Eine Look-Seite in den Einstellungen, mit dem klassischen Look und "
         "einem modernen, der alles etwas abrundet.",
         "Die Usercard zeigt sieben Tage Nachrichten eines Users statt etwa "
         "einer Stunde.",
         "Chatterino-Homies-Badges, und 7TV-Badges, die beim Betreten eines "
         "Kanals laden statt erst, wenn der User schreibt.",
         "Die gemeinsame Zuschauerzahl eines Stream Together im Split-Header.",
         "Chat-Logs älter als vierzehn Tage werden beim Start aufgeräumt.",
         "crossbanned und WhoseTheMod sind dabei. Einschalten unter "
         "Einstellungen → Plugins.",
     }},
}};

QString buildBody(const QStringList &ids)
{
    QString html;

    for (const auto &release : RELEASES)
    {
        if (!ids.contains(QString::fromUtf8(release.id)))
        {
            continue;
        }

        html += QStringLiteral("<h3 style='margin-bottom:4px'>%1</h3><ul>")
                    .arg(QString::fromUtf8(release.date).toHtmlEscaped());

        for (const auto *change : release.changes)
        {
            html += QStringLiteral("<li style='margin-bottom:6px'>%1</li>")
                        .arg(QString::fromUtf8(change).toHtmlEscaped());
        }

        html += QStringLiteral("</ul>");
    }

    return html;
}

}  // namespace

namespace chatterino {

void showWhatsNew()
{
    const auto newest = QString::fromUtf8(RELEASES.front().id);
    const auto lastSeen = getSettings()->lastSeenChanges.getValue();

    if (lastSeen == newest)
    {
        return;
    }

    // Everything down to what they last saw. An unknown value - a fresh
    // profile, or a downgrade - means they have seen none of it.
    QStringList unseen;
    for (const auto &release : RELEASES)
    {
        const auto id = QString::fromUtf8(release.id);
        if (id == lastSeen)
        {
            break;
        }

        unseen.append(id);
    }

    if (unseen.isEmpty())
    {
        return;
    }

    auto *parent = static_cast<QWidget *>(
        &getApp()->getWindows()->getMainWindow());

    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("Neu in ChattiFlexii"));
    dialog->setMinimumSize(480, 440);

    auto *layout = new QVBoxLayout(dialog);

    auto *body = new QLabel(buildBody(unseen), dialog);
    body->setTextFormat(Qt::RichText);
    body->setWordWrap(true);
    body->setAlignment(Qt::AlignTop);
    body->setContentsMargins(4, 0, 12, 0);

    auto *scroll = new QScrollArea(dialog);
    scroll->setWidget(body);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    layout->addWidget(scroll, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Alles klar"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog,
                     &QDialog::close);
    layout->addWidget(buttons);

    getSettings()->lastSeenChanges.setValue(newest);

    dialog->show();
}

}  // namespace chatterino
