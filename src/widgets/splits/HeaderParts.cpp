// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/HeaderParts.hpp"

#include "controllers/twitch/ChannelNumbers.hpp"
#include "singletons/Settings.hpp"
#include "util/Helpers.hpp"

#include <QHash>
#include <QStringList>

#include <algorithm>
#include <cmath>

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

int spacing()
{
    return std::clamp(getSettings()->splitHeaderSpacing.getValue(), 0,
                      MOST_SPACING);
}

void setSpacing(int pixels)
{
    getSettings()->splitHeaderSpacing.setValue(
        std::clamp(pixels, 0, MOST_SPACING));
}

bool canResize(Part part)
{
    // The title and the curve take whatever the others leave - the curve
    // has its own edge to drag - and the chat mode is as wide as its word
    return part != Part::Title && part != Part::Activity &&
           part != Part::Mode;
}

namespace {

/// The widths as they stand in the settings, by the part's id
QHash<QString, int> readWidths()
{
    QHash<QString, int> widths;
    const auto text = getSettings()->splitHeaderWidths.getValue();
    for (const auto &piece : text.split(',', Qt::SkipEmptyParts))
    {
        const auto at = piece.indexOf(':');
        if (at <= 0)
        {
            continue;
        }
        bool ok = false;
        const auto pixels = piece.mid(at + 1).trimmed().toInt(&ok);
        if (ok)
        {
            widths.insert(piece.left(at).trimmed(),
                          std::clamp(pixels, -MOST_DELTA, MOST_DELTA));
        }
    }
    return widths;
}

}  // namespace

int widthDelta(Part part)
{
    if (!canResize(part))
    {
        return 0;
    }
    return readWidths().value(info(part).id, 0);
}

void setWidthDelta(Part part, int pixels)
{
    if (!canResize(part))
    {
        return;
    }

    auto widths = readWidths();
    pixels = std::clamp(pixels, -MOST_DELTA, MOST_DELTA);
    if (pixels == 0)
    {
        widths.remove(info(part).id);
    }
    else
    {
        widths.insert(info(part).id, pixels);
    }

    // In the order the parts stand, so the setting reads the same however
    // it was changed
    QStringList pieces;
    for (const auto &one : all())
    {
        const auto found = widths.find(one.id);
        if (found != widths.end())
        {
            pieces.append(one.id + ':' + QString::number(*found));
        }
    }
    getSettings()->splitHeaderWidths.setValue(pieces.join(','));
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
    s->headerFollowers.setValue(s->headerFollowers.getDefaultValue());
    s->headerChatters.setValue(s->headerChatters.getDefaultValue());
    s->headerMessageRate.setValue(s->headerMessageRate.getDefaultValue());
    s->headerViewerTrend.setValue(s->headerViewerTrend.getDefaultValue());
    s->headerColors.setValue(s->headerColors.getDefaultValue());
    s->splitHeaderSpacing.setValue(s->splitHeaderSpacing.getDefaultValue());
    s->splitHeaderWidths.setValue(s->splitHeaderWidths.getDefaultValue());
}

const std::vector<ItemInfo> &items()
{
    static const std::vector<ItemInfo> all{
        {.item = Item::Name, .id = "name", .name = "Name des Kanals"},
        {.item = Item::Live, .id = "live", .name = "(live)"},
        {.item = Item::Uptime, .id = "uptime", .name = "Laufzeit"},
        {.item = Item::Viewers, .id = "viewers", .name = "Zuschauer"},
        {.item = Item::Trend, .id = "trend", .name = "Zuschauer-Trend"},
        {.item = Item::Followers, .id = "followers", .name = "Follows"},
        {.item = Item::Chatters, .id = "chatters", .name = "Leute im Chat"},
        {.item = Item::Rate, .id = "rate", .name = "Nachrichten pro Minute"},
        {.item = Item::Game, .id = "game", .name = "Kategorie"},
        {.item = Item::StreamTitle, .id = "title", .name = "Streamtitel"},
    };
    return all;
}

namespace {

const ItemInfo &infoOf(Item item)
{
    return *std::find_if(items().begin(), items().end(),
                         [item](const ItemInfo &i) {
                             return i.item == item;
                         });
}

/// The colours as they stand in the settings, by their id
QHash<QString, QColor> readColors()
{
    QHash<QString, QColor> colors;
    const auto text = getSettings()->headerColors.getValue();
    for (const auto &piece : text.split(',', Qt::SkipEmptyParts))
    {
        const auto at = piece.indexOf(':');
        if (at <= 0)
        {
            continue;
        }
        const QColor color(piece.mid(at + 1).trimmed());
        if (color.isValid())
        {
            colors.insert(piece.left(at).trimmed(), color);
        }
    }
    return colors;
}

}  // namespace

QColor colorOf(Item item)
{
    return readColors().value(infoOf(item).id, QColor());
}

void setColorOf(Item item, const QColor &color)
{
    auto colors = readColors();
    if (color.isValid())
    {
        colors.insert(infoOf(item).id, color);
    }
    else
    {
        colors.remove(infoOf(item).id);
    }

    // Written in the order the parts stand, so the setting reads the same
    // whatever was changed last
    QStringList pieces;
    for (const auto &info : items())
    {
        const auto found = colors.find(info.id);
        if (found != colors.end())
        {
            pieces.append(info.id + ':' + found->name(QColor::HexArgb));
        }
    }
    getSettings()->headerColors.setValue(pieces.join(','));
}

bool anyColor()
{
    return !readColors().isEmpty();
}

/// Adds what @a text says about @a item to @a title, and notes its colour
/// where it has one
void appendItem(QString &title, std::vector<Run> *runs, Item item,
                const QString &text, int at = 0)
{
    const auto start = title.size();
    title += text;
    if (runs == nullptr)
    {
        return;
    }
    const auto color = colorOf(item);
    if (!color.isValid())
    {
        return;
    }

    // The separator before it keeps the title's own colour - only what the
    // part says is painted
    auto from = start;
    auto length = text.size();
    if (text.startsWith(QStringLiteral(" - ")))
    {
        from += 3;
        length -= 3;
    }
    else if (text.startsWith(' '))
    {
        from += 1;
        length -= 1;
    }
    if (length > 0)
    {
        runs->push_back({.from = at + int(from),
                         .length = int(length),
                         .color = color});
    }
}

QString composeTitle(const QString &name, const QString &afterName,
                     bool pictureShown, std::vector<Run> *runs)
{
    const auto shift = [runs](int by) {
        if (runs == nullptr || by == 0)
        {
            return;
        }
        for (auto &run : *runs)
        {
            run.from += by;
        }
    };

    if (getSettings()->headerChannelName || !pictureShown)
    {
        shift(name.size());
        if (runs != nullptr && !name.isEmpty())
        {
            const auto color = colorOf(Item::Name);
            if (color.isValid())
            {
                runs->insert(
                    runs->begin(),
                    {.from = 0, .length = int(name.size()), .color = color});
            }
        }
        return name + afterName;
    }

    // What followed the name, without the dash it was joined on with.
    // Whatever falls away at the front moves the coloured stretches along.
    auto rest = afterName;
    qsizetype cut = 0;
    while (cut < rest.size() && rest.at(cut).isSpace())
    {
        cut++;
    }
    rest = rest.mid(cut);
    while (!rest.isEmpty() && rest.back().isSpace())
    {
        rest.chop(1);
    }
    if (rest.startsWith(QStringLiteral("- ")))
    {
        rest = rest.mid(2);
        cut += 2;
    }
    shift(int(-cut));
    return rest;
}

QString extrasAfterName(const Extras &extras, std::vector<Run> *runs, int at)
{
    const auto &settings = *getSettings();
    QString title;

    if (settings.headerFollowers && extras.followers)
    {
        appendItem(title, runs, Item::Followers,
                   " - " + localizeNumbers(*extras.followers) + " Follows", at);
    }
    if (settings.headerChatters && extras.chatters)
    {
        appendItem(title, runs, Item::Chatters,
                   " - " + localizeNumbers(*extras.chatters) + " im Chat", at);
    }
    if (settings.headerMessageRate && extras.messagesPerMinute)
    {
        appendItem(title, runs, Item::Rate,
                   " - " + QString::number(*extras.messagesPerMinute) + "/min",
                   at);
    }
    return title;
}

QString titleAfterName(const TwitchChannel::StreamStatus &s,
                       const Extras &extras, std::vector<Run> *runs)
{
    const auto &settings = *getSettings();
    auto title = QString();

    // live - ChattiFlexii: can be left out, Buttons -> Title bar
    if (settings.headerLiveMarker)
    {
        if (s.rerun)
        {
            appendItem(title, runs, Item::Live, " (rerun)");
        }
        else if (s.streamType.isEmpty())
        {
            appendItem(title, runs, Item::Live, " (" + s.streamType + ")");
        }
        else
        {
            appendItem(title, runs, Item::Live, " (live)");
        }
    }

    // description
    if (settings.headerUptime)
    {
        appendItem(title, runs, Item::Uptime, " - " + s.uptime);
    }
    if (settings.headerViewerCount)
    {
        QString viewers = localizeNumbers(s.viewerCount);

        // In a Stream Together session the channel's own count is only part of
        // the audience, so show the combined one next to it.
        if (s.sharedParticipantCount > 1 && s.sharedViewerCount > s.viewerCount)
        {
            viewers += " (" + localizeNumbers(s.sharedViewerCount) + " total)";
        }
        appendItem(title, runs, Item::Viewers, " - " + viewers);

        // ChattiFlexii: which way the audience is going, where it is worth
        // saying - a channel holding steady says nothing
        if (settings.headerViewerTrend && extras.viewerTrend &&
            std::abs(*extras.viewerTrend) >= channelnumbers::TREND_WORTH_SAYING)
        {
            appendItem(
                title, runs, Item::Trend,
                QStringLiteral(" %1%2 %")
                    .arg(*extras.viewerTrend > 0 ? "↑" : "↓")
                    .arg(int(std::round(std::abs(*extras.viewerTrend) *
                                        100.0))));
        }
    }
    // The numbers stay with the viewer count, before what is streamed: the
    // stream's own title can run long and is cut with "...", and whatever
    // stands behind it is never seen
    title += extrasAfterName(extras, runs, int(title.size()));

    if (settings.headerGame && !s.game.isEmpty())
    {
        appendItem(title, runs, Item::Game, " - " + s.game);
    }
    if (settings.headerStreamTitle && !s.title.isEmpty())
    {
        appendItem(title, runs, Item::StreamTitle,
                   " - " + s.title.simplified());
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
