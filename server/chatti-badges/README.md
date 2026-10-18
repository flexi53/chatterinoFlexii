# Chatti-Badges server

ChattiFlexii's own badges live here: a Cloudflare Worker with one KV store.
Everyone with ChattiFlexii uploads a picture for their own Twitch name under
Settings → Chatti-Badge, an admin approves it, and from then on every
ChattiFlexii shows it.

Who someone is comes from Twitch: the login ChattiFlexii sends along is
checked once at `id.twitch.tv/oauth2/validate` and never kept.

## Setting it up

You need Node.js and a free Cloudflare account. Everything runs in
Cloudflare's free plan; KV needs no payment details.

```sh
cd server/chatti-badges

# 1. Sign in - opens the browser once
npx wrangler login

# 2. Make the store and put the id it prints into wrangler.toml,
#    in place of REPLACE_WITH_KV_NAMESPACE_ID
npx wrangler kv namespace create BADGES

# 3. Put it online - prints the address, something like
#    https://chatti-badges.<your-name>.workers.dev
npx wrangler deploy
```

Then put that address under Settings → Chatti-Badge → Badge-Server in
ChattiFlexii - or have it set as the default in `Settings.hpp`
(`chattiBadgesServer`), so every build has it already.

## Who may approve

`ADMIN_IDS` in `wrangler.toml` lists the Twitch user ids that may approve,
separated by commas. Change it and run `npx wrangler deploy` again. An
admin's own upload is approved at once.

## What it answers

| Request | Who | What |
|---|---|---|
| `GET /badges` | everyone | every approved badge by Twitch user id (`users`), who wears the user or the supporter badge (`status`) and their pictures (`statusImages`) |
| `GET /image/<id>/<file>` | everyone | a picture, `.png`, `.gif` or `.webp` |
| `GET /me` | signed in | one's own badge, approved and waiting, whether one is an admin and whether a supporter |
| `POST /hello`, `DELETE /hello` | signed in | "I use ChattiFlexii" - written down at most once a day, forgotten after 30 days - or no longer |
| `POST /upload` | signed in | a PNG (64 KB; ChattiFlexii makes it 72 × 72) or an animated GIF (1 MB), at most 240 × 240, for one's own name |
| `DELETE /mine` | signed in | takes one's own badge away |
| `GET /pending` | admins | what waits for approval |
| `POST /approve/<id>`, `POST /reject/<id>` | admins | approves or turns down what waits |
| `DELETE /user/<id>` | admins | takes someone's badge away |
| `GET /supporters` | admins | who is a supporter |
| `POST /supporters/<login>`, `DELETE /supporters/<id>` | admins | makes someone a supporter, by Twitch name, or no longer |

"Signed in" means an `Authorization: OAuth <twitch token>` header.

## The user and the supporter badge

Their pictures are drawn by `bilder/make.py` from the logo and kept in KV
like an uploaded one. After changing them, put them there again:

    npx wrangler kv key put --binding BADGES "image:flexii:nutzer.png" --path bilder/nutzer.png --remote
    npx wrangler kv key put --binding BADGES "image:flexii:supporter.webp" --path bilder/supporter.webp --remote

The list of who uses ChattiFlexii is a single KV entry. Cloudflare's free plan
writes at most 1000 entries a day, and a hello is written once a day per
person - so it holds for some hundreds of people using it daily.

## Using it from another client

Any Chatterino fork can show these badges and let its users upload theirs.
Hovering a badge says which program it came from, so each client sends its
own name along.

**Showing them** - fetch the list every few minutes:

```
GET https://chatti-badges.chattiflexii.workers.dev/badges

{"users": {"<twitch user id>": {
    "login": "fx_flexii",
    "image": "https://…/image/<id>/<file>.png",   // or .gif, animated
    "size": 72,                                   // width in pixels
    "client": "ChattiFlexii"                      // shown as "ChattiFlexii-Badge"
}}}
```

Show `image` as a badge next to the user with that id, at a scale of
`18 / size`, with `<client>-Badge` as its tooltip. In ChattiFlexii this is
`src/providers/chattibadges/ChattiBadges.cpp` (MIT), hooked in where the
Moltorino badges are.

**Uploading** - with the user's Twitch login and the program's name:

```
POST /upload
Authorization: OAuth <the user's twitch token>
X-Badge-Client: Sonkerino
Content-Type: image/png          (or image/gif)

<the picture: PNG up to 64 KB, or animated GIF up to 1 MB, at most 240 × 240>
```

It waits until an admin approves it. `GET /me` with the same header says
whether it is approved or still waiting.
