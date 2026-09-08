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

## Look

Settings has a **Look** page with two choices:

- **Classic** - Chatterino as it has always looked
- **Modern** - rounder tabs with a little depth to them

Group headers keep their classic shape in both, so they stay
distinguishable from the tabs beneath them.

## User card

- Shows the last **7 days** of a user's messages instead of roughly the last
  hour, up to 25 of them. Anything older than the channel's own buffer is read
  back from the chat logs, so it needs logging switched on for that channel.

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
