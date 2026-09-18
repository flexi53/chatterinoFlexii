# What ChattiFlexii adds

ChattiFlexii is a fork of [Chatterino7](https://github.com/SevenTV/chatterino7),
pinned to version 7.5.5. Everything Chatterino does, it still does - this page
only lists what is different.

It installs alongside Chatterino under its own name, with its own icon and its
own settings, so both can be used at the same time without getting in each
other's way.

The windows it adds - its settings pages, the alert windows, the moderation
assistant, the welcome window and What's new - are in German.

## Tab groups

Tabs can be gathered under a named header, which is drawn as a rounded chip in
front of the group.

- **Collapse** a group by clicking its header, expand it the same way
- **Rename** by double clicking the header, or through its context menu
- **Colour** a whole group at once - every tab in it takes the colour
- **Move** a group by dragging its header; the tabs come along
- **Always Show Group** keeps a group on screen even with *Only show live
  tabs* switched on, for the channels you want in view whether or not anyone
  is streaming. Groups without it disappear along with their header when none
  of their channels are live.
- **One group per row** is a setting, if you would rather not have groups
  share a row
- The **Tab Groups** window (right click the tab bar) lists every group and
  lets you reorder, rename, recolour, collapse and dissolve them

Groups, their colours and their collapsed state are saved with the window
layout.

## Captions on highlights

The highlight pages gained a **Caption** column - on Messages, Users and
Badges alike, and on the built-in rows as well as your own.

Whatever you type there is drawn next to every message that matches the
highlight: small, lifted slightly off the baseline, pushed to the right hand
edge of the message's last line. It was built to note which channel a
moderator belongs to, but it takes any text.

A Twitch name written as `@name` shows as that channel's **round profile
picture** instead - `@zarbex @trymacs` for someone who moderates both, or
mixed with text like `Mod @zarbex`. Hovering a picture shows the name, and
clicking it opens the user card. Names are looked up on Twitch once; a name
that is no Twitch user stays as text, so a typo is easy to spot. The reply
button stays right behind the message, however long the caption.

Changes take effect as you type them - no restart.

The **First Messages** row starts out with `FIRST` in its caption, which
labels a first-time chatter the way Twitch does. Clear the cell to switch it
off; that is the same rule every caption follows.

## Named colors

The color picker keeps a list of colors you have named - "Mods",
"Regulars" - above the recently used ones. Highlighting the next moderator is
a click on "Mods" instead of hunting for the same shade again. Right click one
to rename it, point it at the selected color, or remove it.

A named color is a template, not a link: changing it later does not recolor
highlights that already use it.

## Look

Settings has a **Look** page with two choices:

- **Classic** - Chatterino as it has always looked
- **Modern** - rounder tabs with a little depth to them

Group headers keep their classic shape in both, so they stay
distinguishable from the tabs beneath them.

The same page has colors for the **tab bar** - the space around the tabs -
and for the **tabs** themselves: a background, a color for the selected tab,
and a gradient, all of which work under either look. Tabs with new messages
or a highlight keep their own color so they still stand out, and **Use theme
colors** goes back to the theme.

## User card

- Shows the last **7 days** of a user's messages instead of roughly the last
  hour, up to 25 of them. Anything older than the channel's own buffer is read
  back from the chat logs, so it needs logging switched on for that channel.
- Shows where the user **moderates**: the pictures of those channels - the
  ones picked under Mod-Highlights first - and an **Alle** button with the
  whole list, former channels included. It asks whosthemod.xyz the way /wtm
  in the WhoseTheMod plugin does, so it needs that plugin switched on, and it
  can be turned off on the Mod-Highlights page.

## Moderation assistant

In channels you moderate, a shield button sits next to the emote button. It
opens the assistant for that channel, which learns from the timeouts and bans
moderators hand out there.

- **Off**, **Learn** or **Suggest**, chosen per channel
- A case is the action - duration, moderator, reason - together with what the
  user wrote before it
- **Import from chat logs** reads the timeouts already in the channel's logs,
  so there is something to go on from the first day
- Once enough cases are collected, a message that resembles earlier ones
  opens a window laid out like a user card - the same one the repeated
  message alert uses - saying how many cases it resembles, what moderators
  gave, and offering the most common action on a button
- It gives a **reason**: what can be read off the message outright - a link, a
  repeated message, caps, character or emote spam, a wall of text - the
  words it shares with earlier cases, and the closest earlier case: what that
  chatter wrote and what they got for it. Hovering shows the next closest.
- **Nothing happens unless you press the button.** The window closes by itself
  otherwise, and at most three suggestions are open at once
- How many cases it needs and how similar a message has to be are under
  Settings -> Mod-Assistent

"Similar" means similar wording. It is good at spam, repeated insults and
links, and knows nothing about context - someone quoting a message to
complain about it looks the same to it. That is why it suggests and never
acts. **Show a test suggestion** under Settings -> Mod-Assistent
opens one with made up messages.

Cases stay on your computer. Moderators, VIPs and the broadcaster never get
suggestions, and neither do your own messages.

### Repeated messages

The same popup has a switch that watches for a chatter sending the same
message three times in a row. When it happens a window laid out like a user
card opens - without taking the keyboard from you - with their picture and how
old their account is, their recent messages as they looked in chat with
Twitch's timeout notices in between, and a timeout button. Each time they send that message again after
having been timed out, by you or another moderator, the window comes back
offering the next step up.

- The steps start out as **30s, 1m, 5m, 10m, 30m**; past the last one it
  stays there
- "The same message" ignores case, extra spaces and the invisible character
  used to get past Twitch's duplicate check. Messages of 10 characters or
  more also count when they are **nearly** the same - 65% alike to begin with -
  so swapping a word does not get around it.
- The window **closes by itself** after 15 seconds, shown by a bar running
  down along its bottom, and stays open while the mouse is over it

Steps, similarity and the closing time are under Settings ->
Mod-Assistent, along with **Show a test alert**, which opens the window with made
up messages and buttons that do nothing.

### Emote spam

A second switch in the same popup watches for chatters flooding the chat with
emotes. It adds up the emotes of their messages over a short time - **8 within
60 seconds** to begin with - counting the messages where emotes outweigh words,
so a string of short bursts with the odd word in between counts as much as one
long wall. The same window comes up, offering what the steps say - to begin
with **delete, delete, 30s**. Delete takes down every message it counted.

The next step only comes once the chatter has actually had a message deleted
or been timed out, by you or anyone else, so a hype moment someone let pass
does not count against them, and a burst that was ignored does not bring the
window straight back. Emojis count as emotes; cheers do not. The number, the
time, the steps and a **Show a test emote spam alert** button are under
Settings -> Mod-Assistent.

### All alert windows

They stay on top of every other program to begin with - over the browser or a
game - without taking the keyboard from you, and can play the ping when a new
one comes up (off to begin with). A test window can be held back five
seconds, so you can switch to another program and watch it come up there.
Drag one to the size and place you want and the next ones open that size and
there, stepping aside when several are up at once; width and height can also
be set directly, and the saved place reset.

Every alert shows its **reason** at the top in a glowing box - "Same message
repeated", "Emote spam", or what the assistant found - in a colour of its own
for each kind of alert, which the running bar takes too. The colours can be
changed; whatever is picked is made bright, so the reason always stands out.

Settings -> Mod-Assistent is split into tabs: **General** for what all alert
windows share, then one each for **suggestions**, **repeated messages** and
**emote spam** with everything about that alert - detection, steps, colour and
its test button.

### Mod highlights

Settings has a **Mod-Highlights** page, right below Mod-Assistent, that
works with the **WhoseTheMod** plugin switched on: pick channels, and messages
from their moderators are
marked with the **profile pictures** of those channels at the end of the line -
someone who moderates two of them gets both - on a background colour you can
set or turn off. The mod lists come from whosthemod.xyz, are kept on your
computer and fetched again every six hours. Type a channel name and press
Enter: it is checked against the same public list /modcheck in the plugin
shows, and added. A chatter's own user or badge highlight keeps its colour
and caption; the pictures join it.

Bots are left out, on a tab of their own: fossabot, nightbot, moobot,
aecrobot, streamelements and other common ones are listed to begin with, the
list can be edited, and any name ending in "bot" is left out too unless that
is switched off.

## Moving to another computer

Settings has an **Export & Import** page. **Export** puts the whole setup -
settings, tabs and tab groups, highlights, commands, hotkeys, named colours,
the mod assistant's cases, mod highlights, themes and plugins with their data -
into one folder on the desktop, ready to AirDrop or copy over. Chat logs and
the cache stay behind, and so does the Twitch login unless you tick it - for
your own devices only.

**Import** takes such a folder on the other computer: ChattiFlexii sets its
own settings aside in a backup, restarts and comes up exactly as it was on the
first one. A login already there is kept when the export brings none. On a new
computer the welcome window offers the same import on first start.

**Automatic backups**, on a third tab and off to begin with, make such an
export by themselves every few days - seven to begin with - into
"ChattiFlexii-Sicherungen" in iCloud Drive where there is one, otherwise in
Documents, and keep the newest five. A backup is imported like any export and
never holds the Twitch login.

## Pinning messages

Moderators can pin messages without the Twitch website: **/pin text** sends
a message and pins it for 20 minutes, **/pin -d none text** until the stream
ends, and **/unpin** takes it down. A message's **Moderate** menu has **Pin**
- until the stream ends, or for 1, 10 or 30 minutes - and **Unpin**. This is
taken over from Chatterino ahead of its next release; the banner Chatterino
shows above the chat for the pinned message is left out.

## Badges and Twitch

- **Chatterino Homies** badges, switchable under Appearance
- **7TV** badges and paints load when you join a channel, rather than the
  first time the user writes something
- The split header shows the **combined viewer count** when a streamer is in a
  Stream Together (only while Shared Chat is on - Twitch publishes nothing
  otherwise)
- `/predictioninfo` prints the running prediction

## Logs

Chat logs older than **14 days** are deleted at startup, and the size shown in
settings keeps up with it.

## Getting started

- Two plugins ship with the app - **crossbanned** and **WhoseTheMod**. They are
  unpacked into your plugin folder on first start but stay switched off, since
  both want network access and one wants the filesystem. Enable them under
  Settings -> Plugins.
- On a fresh profile with Chatterino already installed, ChattiFlexii offers to
  **copy that setup across** - tabs, highlights, commands, themes, plugins.
  Chat logs are left behind. Chatterino itself is only read from; nothing is
  moved or deleted, and afterwards the two keep their own settings.
- Your Twitch login carries over on its own: it lives in the system keychain,
  which both read.

## Downloads

The [releases page](https://github.com/flexi53/chatterinoFlexii/releases)
carries a build of the current code for both platforms, rebuilt on every push.

- **macOS**: Apple Silicon, not notarised - right click the app and pick *Open*
  the first time
- **Windows**: unzip anywhere and run `ChattiFlexii.exe`

Update checks are switched off, so nothing will offer to replace the fork with
upstream Chatterino. New versions have to be fetched from that page by hand.
