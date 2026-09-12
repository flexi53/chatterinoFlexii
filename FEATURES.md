# What ChattiFlexii adds

ChattiFlexii is a fork of [Chatterino7](https://github.com/SevenTV/chatterino7),
pinned to version 7.5.5. Everything Chatterino does, it still does - this page
only lists what is different.

It installs alongside Chatterino under its own name, with its own icon and its
own settings, so both can be used at the same time without getting in each
other's way.

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

## Moderation assistant

In channels you moderate, a shield button sits next to the emote button. It
opens the assistant for that channel, which learns from the timeouts and bans
moderators hand out there.

- **Off**, **Learn** or **Suggest**, chosen per channel
- A case is the action - duration, moderator, reason - together with what the
  user wrote before it
- **Import from chat logs** reads the timeouts already in the channel's logs,
  so there is something to go on from the first day
- Once enough cases are collected, a message that resembles earlier ones gets
  an orange suggestion at the end of the line, with the duration moderators
  usually gave. Hovering it shows how many cases it rests on and how they
  ended.
- Clicking the suggestion **only puts the command into your input box**.
  Nothing is sent until you press Enter.
- How many cases it needs and how similar a message has to be are under
  Settings -> Moderation -> Assistant

"Similar" means similar wording. It is good at spam, repeated insults and
links, and knows nothing about context - someone quoting a message to
complain about it looks the same to it. That is why it suggests and never
acts.

Cases stay on your computer. Moderators, VIPs and the broadcaster never get
suggestions, and neither do your own messages.

### Repeated messages

The same popup has a switch that watches for a chatter sending the same
message three times in a row. When it happens a small window opens - without
taking the keyboard from you - with their recent messages, any timeouts in
between, and a timeout button. Each time they send that message again after
having been timed out, by you or another moderator, the window comes back
offering the next step up.

- The steps start out as **30s, 1m, 5m, 10m, 30m**; past the last one it
  stays there
- "The same message" ignores case, extra spaces and the invisible character
  used to get past Twitch's duplicate check. Messages of 10 characters or
  more also count when they are **nearly** the same - 65% alike to begin with -
  so swapping a word does not get around it.
- The window **closes by itself** after 15 seconds, counting down on the
  Ignore button, and stays open while the mouse is over it

Steps, similarity and the closing time are under Settings -> Moderation ->
Assistant, along with **Show a test alert**, which opens the window with made
up messages and buttons that do nothing.

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
