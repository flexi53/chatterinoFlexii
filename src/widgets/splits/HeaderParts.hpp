// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/twitch/TwitchChannel.hpp"

#include <QColor>
#include <QString>

#include <optional>
#include <vector>

/// Buttons -> Title bar: which parts the split header has, in what order,
/// and how wide its curve is. Nothing changed, it is the header Chatterino
/// has.
namespace chatterino::headerparts {

/// The parts of the split header, in the order they stand when nothing
/// was changed
enum class Part {
    Picture,
    Cover,
    Title,
    Activity,
    Mode,
    Moderation,
    Chatters,
    Tracker,
    Menu,
    Add,
};

struct Info {
    Part part;
    /// How it is written in the settings
    QString id;
    /// What it is called on the settings page
    QString name;
    /// What it is, and when it shows at all
    QString about;
    /// The title and the menu always stay: without the title nobody knows
    /// whose chat it is, without the menu a split can not be closed
    bool canHide;
};

/// Every part, in the order Chatterino has them
const std::vector<Info> &all();
const Info &info(Part part);

/// The order written in @a text. A part it misses goes where it stands by
/// default, and what it does not know is left out.
std::vector<Part> parseOrder(const QString &text);
/// @a order as the settings keep it - empty when it is the standard one
QString writeOrder(const std::vector<Part> &order);

/// The order the settings ask for
std::vector<Part> order();
void setOrder(const std::vector<Part> &order);

/// Whether @a part is wanted in the header. A few still show only where
/// they make sense: the chat mode where one is on, the moderation button
/// and the chatter list where you are a mod, the plus on the last split.
bool isShown(Part part);
void setShown(Part part, bool shown);

/// Room between the parts, in unscaled pixels - 0 is how Chatterino has it
int spacing();
void setSpacing(int pixels);
constexpr int MOST_SPACING = 24;

/// Whether @a part can be made wider or narrower. The title and the curve
/// take what is left over, so they are not among them - the curve has its
/// own edge to drag.
bool canResize(Part part);
/// How many pixels wider than usual @a part is drawn, which may be below
/// zero. Unscaled, as everything the settings keep.
int widthDelta(Part part);
void setWidthDelta(Part part, int pixels);
/// As far as a part may be dragged, either way
constexpr int MOST_DELTA = 24;
constexpr int LEAST_WIDTH = 10;

/// Everything back to the header as Chatterino has it
void reset();

/// Where @a channel stands on twitchtracker.com. The site wants the name in
/// small letters; empty in, empty out.
QString trackerUrl(const QString &channel);

/// The numbers the title can show beside what the stream itself says.
/// Each is there only when it is switched on and known.
struct Extras {
    std::optional<int> followers;
    std::optional<int> chatters;
    std::optional<int> messagesPerMinute;
    /// Which way the audience went, as a share of what it was
    std::optional<double> viewerTrend;
};

/// A stretch of the title drawn in a colour of its own
struct Run {
    int from;
    int length;
    QColor color;

    bool operator==(const Run &other) const = default;
};

/// The things the title is made of, each of which can be given a colour
enum class Item {
    Name,
    Live,
    Uptime,
    Viewers,
    Trend,
    Followers,
    Chatters,
    Rate,
    Game,
    StreamTitle,
};

struct ItemInfo {
    Item item;
    /// How it is written in the settings
    QString id;
    /// What it is called on the settings page
    QString name;
};

/// Every part of the title, in the order it stands
const std::vector<ItemInfo> &items();

/// The colour @a item was given, or an invalid one where it keeps the
/// colour of the title
QColor colorOf(Item item);
/// Gives @a item a colour - an invalid one takes it back to the title's
void setColorOf(Item item, const QColor &color);
/// Whether any part was given a colour at all
bool anyColor();

/// What follows the channel's name in the title while it is live: "(live)"
/// and, as far as they are switched on, uptime, viewers, category and the
/// stream's title, followed by @a extras. @a runs, where one is handed in,
/// collects the stretches that carry a colour.
QString titleAfterName(const TwitchChannel::StreamStatus &s,
                       const Extras &extras = {},
                       std::vector<Run> *runs = nullptr);

/// Only the numbers of @a extras - what a channel that is not live can
/// still say. @a at is where the text it returns will stand, so the runs
/// it adds point at the right letters.
QString extrasAfterName(const Extras &extras, std::vector<Run> *runs = nullptr,
                        int at = 0);

/// The whole title: @a name and @a afterName, or only what comes after it
/// when the name is switched off. @a pictureShown says whether the
/// channel's picture stands before the title - without it the name stays,
/// or nobody would know whose chat it is.
QString composeTitle(const QString &name, const QString &afterName,
                     bool pictureShown, std::vector<Run> *runs = nullptr);

/// The least room the title keeps however wide the curve is made, in
/// unscaled pixels - a narrow split still says whose chat it is
constexpr int TITLE_KEEPS = 100;
/// How far the curve can be dragged, as its share of the room it has
/// with the title
constexpr int LEAST_SHARE = 10;
constexpr int MOST_SHARE = 90;

/// How wide the curve should be. @a shared is the room it has together
/// with the title, @a needed what the title needs to show in full, @a own
/// the curve's own width, @a share the percent of @a shared it was set to
/// - 0 for half of what the title leaves free - and @a titleKeeps what
/// the title keeps at the least.
int curveWidth(int shared, int needed, int own, int share, int titleKeeps);

}  // namespace chatterino::headerparts
