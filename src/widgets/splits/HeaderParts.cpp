// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/HeaderParts.hpp"

#include "singletons/Settings.hpp"
#include "util/Helpers.hpp"

#include <QStringList>

#include <algorithm>

namespace chatterino::headerparts {

namespace {

QStringList hiddenIds()
{
    return getSettings()->splitHeaderHidden.getValue().split(
        ',', Qt::SkipEmptyParts);
}

}  // namespace

const std::vector<Info> &all()
{
    static const std::vector<Info> parts{
        {
            .part = Part::Picture,
            .id = "picture",
            .name = "Profilbild",
            .about = "Das Bild des Kanals.",
            .canHide = true,
        },
        {
            .part = Part::Cover,
            .id = "cover",
            .name = "Kategoriebild",
            .about = "Das Cover dessen, was der Kanal streamt - nur solange "
                     "er live ist.",
            .canHide = true,
        },
        {
            .part = Part::Title,
            .id = "title",
            .name = "Titel",
            .about = "Der Name des Kanals und, solange er live ist, was "
                     "du unter „Was im Titel steht“ angehakt hast - dort "
                     "stellst du ein, was davon.",
            .canHide = false,
        },
        {
            .part = Part::Activity,
            .id = "activity",
            .name = "Aktivitäts-Kurve",
            .about = "Wie viel im Chat los war. Ihre Breite ziehst du oben "
                     "in der Vorschau an ihrem Rand zum Titel hin.",
            .canHide = true,
        },
        {
            .part = Part::Mode,
            .id = "mode",
            .name = "Chatmodus",
            .about = "Slow, Sub-only, Emote-only und Co. - nur wenn einer "
                     "an ist.",
            .canHide = true,
        },
        {
            .part = Part::Moderation,
            .id = "moderation",
            .name = "Moderationsmodus",
            .about = "Das Schwert für die Mod-Knöpfe an jeder Nachricht - "
                     "nur wo du Mod bist.",
            .canHide = true,
        },
        {
            .part = Part::Chatters,
            .id = "chatters",
            .name = "Chatterliste",
            .about = "Wer gerade im Chat ist - nur wo du Mod bist. Ohne "
                     "den Knopf öffnest du sie über das Menü.",
            .canHide = true,
        },
        {
            .part = Part::Tracker,
            .id = "tracker",
            .name = "TwitchTracker",
            .about = "Öffnet den Kanal auf twitchtracker.com - Zuschauer, "
                     "Verlauf und Zahlen zum Stream. Nur bei Twitch-Kanälen.",
            .canHide = true,
        },
        {
            .part = Part::Menu,
            .id = "menu",
            .name = "Menü",
            .about = "Die drei Punkte mit allem, was der Split kann. Bleibt "
                     "immer, sonst ließe er sich nicht schließen.",
            .canHide = false,
        },
        {
            .part = Part::Add,
            .id = "add",
            .name = "Neuer Split (+)",
            .about = "Nur am letzten Split rechts. Ohne ihn geht es über "
                     "das Menü oder ⌘T.",
            .canHide = true,
        },
    };
    return parts;
}

const Info &info(Part part)
{
    const auto &parts = all();
    return *std::find_if(parts.begin(), parts.end(), [part](const Info &i) {
        return i.part == part;
    });
}

std::vector<Part> parseOrder(const QString &text)
{
    std::vector<Part> order;
    for (const auto &id : text.split(',', Qt::SkipEmptyParts))
    {
        const auto trimmed = id.trimmed();
        for (const auto &i : all())
        {
            if (i.id == trimmed &&
                std::find(order.begin(), order.end(), i.part) == order.end())
            {
                order.push_back(i.part);
            }
        }
    }

    // What is missing goes behind the one it follows by default, or to the
    // very front when it is the first
    const auto &parts = all();
    for (size_t i = 0; i < parts.size(); i++)
    {
        const auto part = parts.at(i).part;
        if (std::find(order.begin(), order.end(), part) != order.end())
        {
            continue;
        }
        auto at = order.begin();
        for (size_t before = i; before > 0; before--)
        {
            auto found = std::find(order.begin(), order.end(),
                                   parts.at(before - 1).part);
            if (found != order.end())
            {
                at = std::next(found);
                break;
            }
        }
        order.insert(at, part);
    }
    return order;
}

QString writeOrder(const std::vector<Part> &order)
{
    const auto complete = parseOrder([&order] {
        QStringList ids;
        for (const auto part : order)
        {
            ids.append(info(part).id);
        }
        return ids.join(',');
    }());

    QStringList ids;
    bool standard = true;
    for (size_t i = 0; i < complete.size(); i++)
    {
        ids.append(info(complete.at(i)).id);
        standard = standard && complete.at(i) == all().at(i).part;
    }
    return standard ? QString() : ids.join(',');
}

std::vector<Part> order()
{
    return parseOrder(getSettings()->splitHeaderOrder.getValue());
}

void setOrder(const std::vector<Part> &order)
{
    getSettings()->splitHeaderOrder.setValue(writeOrder(order));
}

bool isShown(Part part)
{
    const auto *s = getSettings();
    switch (part)
    {
        case Part::Title:
        case Part::Menu:
            return true;
        case Part::Activity:
            return s->splitHeaderActivity;
        case Part::Picture:
        case Part::Cover:
            // Look -> Title bar switches both on and off together
            if (!s->splitHeaderPictures)
            {
                return false;
            }
            break;
        default:
            break;
    }
    return !hiddenIds().contains(info(part).id);
}

void setShown(Part part, bool shown)
{
    auto *s = getSettings();
    if (!info(part).canHide || isShown(part) == shown)
    {
        return;
    }
    if (part == Part::Activity)
    {
        s->splitHeaderActivity.setValue(shown);
        return;
    }

    auto hidden = hiddenIds();
    const auto &id = info(part).id;
    const bool picture = part == Part::Picture || part == Part::Cover;
    const auto &other =
        info(part == Part::Picture ? Part::Cover : Part::Picture).id;

    // The pictures come on together; the other one stays as it looked
    if (picture && shown && !s->splitHeaderPictures && !hidden.contains(other))
    {
        hidden.append(other);
    }
    hidden.removeAll(id);
    if (!shown)
    {
        hidden.append(id);
    }

    // Both pictures off is what Look -> Title bar calls off
    const bool bothOff =
        picture && hidden.contains(id) && hidden.contains(other);
    if (bothOff)
    {
        hidden.removeAll(id);
        hidden.removeAll(other);
    }

    s->splitHeaderHidden.setValue(hidden.join(','));
    if (picture)
    {
        s->splitHeaderPictures.setValue(!bothOff);
    }
}

QString trackerUrl(const QString &channel)
{
    const auto name = channel.trimmed().toLower();
    if (name.isEmpty())
    {
        return {};
    }
    return QStringLiteral("https://twitchtracker.com/") + name;
}

void reset()
{
    auto *s = getSettings();
    s->splitHeaderOrder.setValue(s->splitHeaderOrder.getDefaultValue());
    s->splitHeaderHidden.setValue(s->splitHeaderHidden.getDefaultValue());
    s->splitHeaderActivityShare.setValue(
        s->splitHeaderActivityShare.getDefaultValue());
    s->splitHeaderPictures.setValue(s->splitHeaderPictures.getDefaultValue());
    s->splitHeaderActivity.setValue(s->splitHeaderActivity.getDefaultValue());
    s->headerLiveMarker.setValue(s->headerLiveMarker.getDefaultValue());
    s->headerChannelName.setValue(s->headerChannelName.getDefaultValue());
    s->headerUptime.setValue(s->headerUptime.getDefaultValue());
    s->headerViewerCount.setValue(s->headerViewerCount.getDefaultValue());
    s->headerGame.setValue(s->headerGame.getDefaultValue());
    s->headerStreamTitle.setValue(s->headerStreamTitle.getDefaultValue());
}

QString composeTitle(const QString &name, const QString &afterName,
                     bool pictureShown)
{
    if (getSettings()->headerChannelName || !pictureShown)
    {
        return name + afterName;
    }

    // What followed the name, without the dash it was joined on with
    auto rest = afterName.trimmed();
    if (rest.startsWith(QStringLiteral("- ")))
    {
        rest = rest.mid(2);
    }
    return rest;
}

QString titleAfterName(const TwitchChannel::StreamStatus &s)
{
    const auto &settings = *getSettings();
    auto title = QString();

    // live - ChattiFlexii: can be left out, Buttons -> Title bar
    if (settings.headerLiveMarker)
    {
        if (s.rerun)
        {
            title += " (rerun)";
        }
        else if (s.streamType.isEmpty())
        {
            title += " (" + s.streamType + ")";
        }
        else
        {
            title += " (live)";
        }
    }

    // description
    if (settings.headerUptime)
    {
        title += " - " + s.uptime;
    }
    if (settings.headerViewerCount)
    {
        title += " - " + localizeNumbers(s.viewerCount);

        // In a Stream Together session the channel's own count is only part of
        // the audience, so show the combined one next to it.
        if (s.sharedParticipantCount > 1 && s.sharedViewerCount > s.viewerCount)
        {
            title += " (" + localizeNumbers(s.sharedViewerCount) + " total)";
        }
    }
    if (settings.headerGame && !s.game.isEmpty())
    {
        title += " - " + s.game;
    }
    if (settings.headerStreamTitle && !s.title.isEmpty())
    {
        title += " - " + s.title.simplified();
    }

    return title;
}

int curveWidth(int shared, int needed, int own, int share, int titleKeeps)
{
    int width = 0;
    if (share <= 0)
    {
        // Half of what the title does not need, on top of its own width
        width = own + std::max((shared - own - needed) / 2, 0);
    }
    else
    {
        width = shared * std::clamp(share, LEAST_SHARE, MOST_SHARE) / 100;
    }

    // The title keeps enough to be read
    width = std::min(width, shared - std::min(needed, titleKeeps));
    return std::max(width, 0);
}

}  // namespace chatterino::headerparts
