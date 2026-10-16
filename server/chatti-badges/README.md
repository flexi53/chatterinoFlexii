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
| `GET /badges` | everyone | every approved badge, by Twitch user id |
| `GET /image/<id>/<version>.png` | everyone | a picture |
| `GET /me` | signed in | one's own badge, approved and waiting, and whether one is an admin |
| `POST /upload` | signed in | a PNG of at most 128 × 128 and 64 KB for one's own name |
| `DELETE /mine` | signed in | takes one's own badge away |
| `GET /pending` | admins | what waits for approval |
| `POST /approve/<id>`, `POST /reject/<id>` | admins | approves or turns down what waits |
| `DELETE /user/<id>` | admins | takes someone's badge away |

"Signed in" means an `Authorization: OAuth <twitch token>` header.
