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
  is streaming - and switching tabs by keyboard reaches them too, live or
  not. Groups without it disappear along with their header when none of
  their channels are live. They also stay in the focus view.
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

## Spelling variants

Next to **Add** on the Messages highlights page, **Mit Schreibweisen...**
turns a word or short phrase into a highlight that also finds it written
differently. From `follower` it makes a pattern that finds `f0ll0w3r`,
`fооllower` with Cyrillic letters, `föllower`, `fooollower`, `folower`,
`f.o.l.l.o.w.e.r` and `f o l l o w e r` - and `dämlich` as `daemlich`,
`damlich` or `d.a.e.m.l.i.c.h`. Each kind of disguise can be switched off,
and **Nur als eigenes Wort** keeps `lol` from matching `lollipop`.

The window shows examples of what the pattern finds, the pattern itself to
copy, and a field to try it on a message first - which also says when the
word is there only inside a longer one. The highlight it adds starts with
the word as a comment, so it is easy to spot in the list.

## Named colors

The color picker keeps a list of colors you have named - "Mods",
"Regulars" - above the recently used ones. Highlighting the next moderator is
a click on "Mods" instead of hunting for the same shade again. Right click one
to rename it, point it at the selected color, or remove it.

A named color is a template, not a link: changing it later does not recolor
highlights that already use it.

## Look

Settings has a **Look** page in four tabs - Stil, Tabs, Chat and Farben.
Everything on it starts out as Chatterino looks, and each part has a
**Standard** button that puts it back.

Under Stil there are two looks:

- **Classic** - Chatterino as it has always looked
- **Modern** - rounder tabs with a little depth to them

Group headers keep their classic shape in both, so they stay
distinguishable from the tabs beneath them.

The Tabs tab has colors for the **tab bar** - the space around the tabs -
and for the **tabs** themselves: a background, a color for the selected tab,
and a gradient, all of which work under either look. Tabs with new messages
or a highlight keep their own color so they still stand out, and **Standard**
goes back to the theme. **Profilbilder in den Tabs** puts the channel's round
profile picture in front of each tab's name - the first split's, for a tab
with several. With **Live-Ring**, a live
channel's picture gets a red ring instead of the dot in the corner.

**Aktiver Split**: with several chats side by side in a tab, the one being
typed in gets a border - red to begin with, in a colour of your choice.

The **split header** can show the channel's picture and, while it is live,
the cover of what it streams, next to the name - and a small curve of how
lively the chat was over the last ten minutes, counted from when it is
switched on; its tooltip tells the numbers.

Over the curve stands what the channel was streaming at the time, centred
between the lines that mark each change - with how long it lasted where
there is room for both ("Just Chatting · 1 h 30"). Too narrow a stretch is
left unwritten; hovering names every one of them in full, the one under the
mouse first, with the times and how long it ran.

The title can carry four more numbers, each its own checkbox under Buttons ->
Titelleiste: the **Zuschauer-Trend** behind the viewer count ("↑ 18 %"
against the last half hour, nothing while the channel holds steady),
**Follows**, **Leute im Chat** - Twitch tells that to moderators only, so it
shows in your mod channels - and **Nachrichten pro Minute**, counted from
what arrives here rather than asked of Twitch, so it works offline too.

Every part of the title can be given a **colour of its own** on the same
page - the trend in green, the follows in purple - while the dashes between
them stay as they are. A part without one keeps the colour of the title.

The preview on that page is dragged, not typed: a part moves to another
place, its **right edge** makes it wider or narrower, its **left edge** sets
the room between all parts, and the curve's edge to the title divides those
two. A double click on an edge puts it back to standard.

A **TwitchTracker** button sits next to the chatter list: one click opens the
channel on twitchtracker.com - viewers, history and the numbers behind the
stream. Twitch channels only, as the site knows nothing of Kick. Like every
other part of the header it can be moved or switched off under Buttons ->
Titelleiste.

**Fokus-Ansicht** hides a window's tabs, the buttons next to them and the
split headers, leaving the chats - and the tab groups set to **Always Show
Group**, so the channels that matter most stay one click away. A button
drawn as four corners, in every split's input bar between the moderation
assistant and the emotes, switches it on and off for the window it is in -
a second window stays as it is - and it stays in reach when everything else
goes; the tab bar's right-click menu has it too. Each window remembers it,
and each computer keeps it to itself when the setup is synced.

Next to it, a button drawn as a bin **empties the chat** of its split, here
only - what the split menu's *Clear messages* does, one click away, say for
mentions that have been dealt with.

### Chat

- **Abstand zwischen Nachrichten**: a little more room above and below each
  message - 2, 4 or 8 pixels
- **Nachricht unter der Maus hervorheben**: lights up the message under the
  pointer, in a faint shade that suits the theme or a colour of your own
- **Neue Nachrichten sanft einblenden**: new messages fade in and glide the
  last few pixels into place, quickly at first and settling gently, rather
  than appear at once. **Chat weich nachrutschen lassen** - Chatterino's
  smooth scrolling on new messages - goes well with it, so the chat does not
  jump a line either.
- **Profilbild vor jedem Namen**: the chatter's round picture in front of
  their name; a click on it opens the user card. New messages get it from
  when it is switched on; switching it off hides it everywhere at once.
- **Ereignisse markieren**: subs ⭐, gifts 🎁, raids 🚀, announcements 📣,
  timeouts ⏱️, bans 🔨, bits 💎, redeemed points 🎟️ and watch streaks 🔥
  get their symbol in front and a stripe in their colour
- **Erwähnungen aufblinken lassen**: a message that calls you by name - or a
  whisper - flashes three times as it comes in, so it is caught even in a
  fast chat. Only a real ping does that; a highlight on a word or on someone
  watched stays quiet, and history read in does too
- **Rollen-Streifen**: a narrow stripe at the left edge of every message shows
  who wrote it. A role with a badge highlight (Highlights -> Badges) takes
  that highlight's colour, made solid - for a lead moderator their own
  highlight or else the moderators' - and its field on the page is greyed
  out, showing where the colour comes from. The others have their own:
  the streamer in red, moderators in green, VIPs in pink, and subscribers if
  given a colour. A role without a colour gets no stripe.

### Readability

Under **Lesbarkeit** on the Chat tab, every other message can be set apart
from the one before it, and made to stand out as much as you like - from the
theme's faint grey through **Dezent**, **Mittel** and **Deutlich** to
**Stark** - in a neutral shade or a colour of your own, such as a light
violet. With **Nur wechseln, wenn jemand anderes schreibt** the background
changes only when someone else writes, so several messages from one chatter
in a row read as one block. It all shows in the chat at once, and
**Standard** goes back to the theme's shade.

### Farben

**Eigenes Farbschema** lays colours of your own over the theme: the chat
background, text, system text, links, the accent colour, split headers and
the input box. A colour left empty stays as the theme has it; switching it
off brings the theme back and keeps your colours for later.

## User card

- Shows the last **7 days** of a user's messages instead of roughly the last
  hour, up to 25 of them. Anything older than the channel's own buffer is read
  back from the chat logs, so it needs logging switched on for that channel.
- Shows where the user **moderates**: the pictures of those channels - the
  ones picked under Mod-Highlights first - and an **Alle** button with the
  whole list, former channels included. It asks whosthemod.xyz the way /wtm
  in the WhoseTheMod plugin does, so it needs that plugin switched on, and it
  can be turned off on the Mod-Highlights page.

## Erweitert

The moderation assistant and the User tab sit together on one settings page
called **Erweitert**, one tab each. That page is only there while the
account it was built for is logged in - anyone else gets ChattiFlexii
without those parts: no page, no shield and bell beside the input, no entry
in the right-click menu, no User tab and no alert window ever opens. The
rest of the program is the same for everyone. It is tidying up rather than
a lock: the code is public, and whoever builds it can put their own name in
(`src/util/Advanced.hpp`).

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
offering the next step up - and so it does when nobody acts: every three more
repeats without a timeout bring it back a step higher, so ignoring it does
not keep it at the first step.

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

The next step comes once the chatter has had a message deleted or been timed
out, by you or anyone else - or, when nobody acts, once they have flooded as
many emotes again as the alert starts at: an ignored alert comes back a step
higher rather than at the first step again. Emojis count as emotes; cheers do
not. The number, the time, the steps and a **Show a test emote spam alert**
button are under Settings -> Mod-Assistent.

### All alert windows

They stay on top of every other program to begin with - over the browser or a
game - without taking the keyboard from you, and can play a sound when a new one
comes up (off to begin with) - its own for each kind of alert, picked from
five that come with the app or any file, so an alert is told from a live
notification by ear. A test window can be held back five
seconds, so you can switch to another program and watch it come up there.
Drag one to the size and place you want and the next ones open that size and
there, stepping aside when several are up at once; width and height can also
be set directly, and the saved place reset.

At the bottom of every alert is a button for each action - deleting where
there is something to delete, and every timeout from the steps - with the one
recommended lit up in the alert's colour, so a special case can still get
something else. None of them is a default, so no key press sets one off.

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

The **User** tab of that page is where the watched people are kept - the
list, the filter and the button that opens their tab. What you note about
people and the messages you kept with "Merken" stay under **Notizen**,
where everyone has them.

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
export by themselves every day, every three days, every week or every two
weeks - weekly to begin with - into "ChattiFlexii-Sicherungen" in iCloud
Drive where there is one, otherwise in Documents. There is only ever one
backup: the next replaces it, once it is complete. A backup is imported like
any export and never holds the Twitch login.

**Ansichten**, the fifth tab, saves every setting under a name - say
"Moderieren" with stripes, symbols and the activity curve, "Entspannt" all
plain - to switch between with a click, there or from the tab bar's
right-click menu. A view leaves the tabs, the Twitch login and what belongs
to this computer alone. Switching restarts the app, and what was set up
before is kept as "Vor dem Wechsel", so there is always a way back.

**Abgleich**, the fourth tab, keeps two computers alike - the Mac and the
MacBook, say. Switched on on both, each leaves its setup as
"ChattiFlexii-Abgleich" in the backup folder whenever it changed - every half
hour and on quitting - and iCloud Drive carries it to the other. When
ChattiFlexii finds a newer setup from the other computer there, it asks:
**Übernehmen** restarts it set up like the other one, with its own setup
backed up first; **Meine behalten** puts this one in its place; **Später**
asks again at the next start. It never takes anything without asking, and
never writes over a setup from the other computer that has not been
answered. The Twitch login stays on each computer, and so does the way the
windows are arranged: taking a setup brings the settings and the tabs, but
the main window, popups and alert windows keep their places and sizes on
this computer's screens - and moving a window is not a change to pass on.

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

- Two plugins ship with the app - **crossbanned** and **WhoseTheMod**. Both are
  unpacked into your plugin folder on first start. **WhoseTheMod** is switched
  on as it is put there, together with Chatterino's plugin support, since
  Mod-Highlights and the user card's list of channels are built on it and it
  only ever asks Twitch and whosthemod.xyz. **crossbanned** wants to read and
  write files and stays off until you ask for it, under Settings -> Plugins.
  Switching either of them off there is final: what is already on disk is never
  switched back on.
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

Chatterino's own update check is switched off, so nothing will offer to
replace the fork with upstream Chatterino. Instead, the downloads look on
that page a little after starting and every six hours whether there is a
newer build, and offer it. On macOS, **Jetzt aktualisieren** does it in one
click: the new version is downloaded, checked against the checksum GitHub
gives for it, copied out next to the running app - all while that still
runs, so nothing is lost if any of it fails - and then ChattiFlexii quits,
the two are swapped and the new one opens. Settings stay, and as the
download never passes through a browser, macOS does not ask whether to open
it. **Herunterladen** fetches it in the browser instead - the only way on
Windows - and **Später** asks again a day later. Settings -> About has a
switch for it and **Jetzt nach Updates suchen**. A build made at home does
not look - it has no release to compare with.
