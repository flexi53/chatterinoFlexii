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
    /// Dated rather than numbered - the fork does not carry a version of its
    /// own, and a date says more about when something landed anyway.
    const char *date;
    std::initializer_list<const char *> changes;
};

/// Newest first. Add to the top when something lands that is worth telling
/// the user about; everything above what they last saw is shown at once.
const std::array<Release, 2> RELEASES{{
    {"13 September 2026",
     {
         "A moderation assistant, behind the shield button next to the emote "
         "button in channels you moderate. It learns from the timeouts and "
         "bans handed out there, and once switched to Suggest it marks "
         "messages that look like earlier cases with the usual action and "
         "duration. Clicking the mark only puts the command into your input "
         "box.",
         "Colors for the tab bar and for the tabs on the Look page - a "
         "background, a selected tab and a gradient, under either look.",
         "Name the colors you keep reusing in the color picker, so picking "
         "\"Mods\" gives every moderator the same color.",
         "An alert for chatters sending the same message - or nearly the "
         "same - three times in a row, with their messages, the timeouts in "
         "between and a timeout button that steps up each time they carry on "
         "(30s, 1m, 5m, 10m, 30m to begin with, adjustable). Switched on per "
         "channel in the moderation assistant.",
         "A little more room between rows of tabs.",
     }},
    {"8 September 2026",
     {
         "Tab groups: gather tabs under a named header, collapse them, colour "
         "a whole group at once, drag a group to move it, and pin one open so "
         "it survives \"only show live tabs\".",
         "A Caption column on every highlight page. What you type there is "
         "drawn next to matching messages, at the end of the last line.",
         "The first message label is a caption like any other now - it lives "
         "in the First Messages row and starts out as FIRST.",
         "A Look page in the settings, with the classic look and a modern one "
         "that rounds things off.",
         "The user card shows seven days of a user's messages instead of "
         "roughly an hour.",
         "Chatterino Homies badges, and 7TV badges that load when you join a "
         "channel rather than when the user first writes.",
         "The combined viewer count of a Stream Together in the split header.",
         "Chat logs older than fourteen days are cleaned up at startup.",
         "crossbanned and WhoseTheMod ship with the app. Switch them on under "
         "Settings, Plugins.",
     }},
}};

QString buildBody(const QStringList &dates)
{
    QString html;

    for (const auto &release : RELEASES)
    {
        if (!dates.contains(QString::fromUtf8(release.date)))
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
    const auto newest = QString::fromUtf8(RELEASES.front().date);
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
        const auto date = QString::fromUtf8(release.date);
        if (date == lastSeen)
        {
            break;
        }

        unseen.append(date);
    }

    if (unseen.isEmpty())
    {
        return;
    }

    auto *parent = static_cast<QWidget *>(
        &getApp()->getWindows()->getMainWindow());

    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("What's new in ChattiFlexii"));
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
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Got it"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog,
                     &QDialog::close);
    layout->addWidget(buttons);

    getSettings()->lastSeenChanges.setValue(newest);

    dialog->show();
}

}  // namespace chatterino
