// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// ChattiFlexii's own badges: everyone with ChattiFlexii can upload one for
// their Twitch name, an admin approves it, and from then on every
// ChattiFlexii shows it next to that name.
//
// Runs as a Cloudflare Worker with one KV namespace bound as BADGES. Who
// someone is comes from Twitch: the login they send is checked once against
// id.twitch.tv and never kept. Admins are listed by Twitch user id in the
// variable ADMIN_IDS, separated by commas.
//
// What is kept in KV:
//   user:<id>         {login, approved, pending, approvedSize, pendingSize,
//                     uploadedAt} - approved and pending are file names, a
//                     random word with .png or .gif, the sizes their width
//   image:<id>:<file> the picture itself
//   list              {"<id>": {"login", "version", "size"}} of everything
//                     approved, so the list is one read rather than a scan
//   seen              {"<id>": {"login", "at"}} of who uses ChattiFlexii -
//                     each says so at most once a day, and drops out after
//                     30 days without a word
//   supporters        {"<id>": "<login>"}, given by an admin
//   image:flexii:<file> the pictures of those two badges, put there by hand
//                     (see README.md)
//
// Everyone in seen wears the user badge, every supporter the supporter
// badge instead - never both.

// A PNG is made small by ChattiFlexii; a GIF comes as it is, to keep moving
const MAX_PNG_BYTES = 64 * 1024;
const MAX_GIF_BYTES = 1024 * 1024;
const MAX_SIDE = 240;
// The program a badge was uploaded with, shown when hovering it - every
// client that uses this server sends its own name along
const DEFAULT_CLIENT = "ChattiFlexii";

/// The name of the program in the request's X-Badge-Client header - letters,
/// digits, spaces and a few marks, at most 32 of them
function clientOf(request) {
    const sent = (request.headers.get("X-Badge-Client") || "")
        .replace(/[^\p{L}\p{N} ._-]/gu, "")
        .trim()
        .slice(0, 32);
    return sent || DEFAULT_CLIENT;
}
const UPLOAD_EVERY_SECONDS = 60;
// Who has not said hello for this long loses the user badge
const SEEN_FOR_SECONDS = 30 * 24 * 60 * 60;
// A hello within this long of the last one is not written down again, so
// a day of restarts costs one write
const HELLO_EVERY_SECONDS = 20 * 60 * 60;
// The two badges everyone can get, served like an uploaded one
const STATUS_IMAGES = {
    user: "nutzer.png",
    supporter: "supporter.webp",
};
const LIST_CACHE_SECONDS = 300;

function json(body, status = 200, extraHeaders = {}) {
    return new Response(JSON.stringify(body), {
        status,
        headers: {
            "Content-Type": "application/json; charset=utf-8",
            ...extraHeaders,
        },
    });
}

function fail(status, message) {
    return json({ error: message }, status);
}

/// Width and height of a GIF, read from its header - or null for anything
/// that is not one
function gifSize(bytes) {
    const head = String.fromCharCode(...bytes.slice(0, 6));
    if (bytes.length < 10 || (head !== "GIF87a" && head !== "GIF89a")) {
        return null;
    }
    return {
        width: bytes[6] | (bytes[7] << 8),
        height: bytes[8] | (bytes[9] << 8),
    };
}

/// The file name a version is served under - versions from before GIFs
/// were taken have no ending and are PNGs
function fileOf(version) {
    return version.includes(".") ? version : `${version}.png`;
}

function randomVersion() {
    const bytes = new Uint8Array(9);
    crypto.getRandomValues(bytes);
    return [...bytes].map((b) => b.toString(16).padStart(2, "0")).join("");
}

/// Who the Twitch login in the request belongs to - or null. The login is
/// only handed to Twitch to ask, never kept.
async function whoIs(request) {
    const header = request.headers.get("Authorization") || "";
    const token = header.replace(/^(OAuth|Bearer)\s+/i, "").trim();
    if (!token) {
        return null;
    }
    const answer = await fetch("https://id.twitch.tv/oauth2/validate", {
        headers: { Authorization: `OAuth ${token}` },
    });
    if (!answer.ok) {
        return null;
    }
    const body = await answer.json();
    if (!body.user_id || !body.login) {
        return null;
    }
    return {
        id: String(body.user_id),
        login: String(body.login),
        clientId: String(body.client_id || ""),
        token,
    };
}

function isAdmin(env, user) {
    const admins = String(env.ADMIN_IDS || "")
        .split(",")
        .map((id) => id.trim())
        .filter(Boolean);
    return user !== null && admins.includes(user.id);
}

/// Width and height of a PNG, read from its header - or null for anything
/// that is not one
function pngSize(bytes) {
    const signature = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
    if (bytes.length < 24 || !signature.every((b, i) => bytes[i] === b)) {
        return null;
    }
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.length);
    return { width: view.getUint32(16), height: view.getUint32(20) };
}

async function readUser(env, id) {
    return (await env.BADGES.get(`user:${id}`, "json")) || null;
}

async function readList(env) {
    return (await env.BADGES.get("list", "json")) || {};
}

async function readSeen(env) {
    return (await env.BADGES.get("seen", "json")) || {};
}

async function readSupporters(env) {
    return (await env.BADGES.get("supporters", "json")) || {};
}

/// Who wears which of the two badges: "supporter" or "user" by Twitch id
async function readStatus(env) {
    const status = {};
    const now = Math.floor(Date.now() / 1000);
    for (const [id, entry] of Object.entries(await readSeen(env))) {
        if (now - (entry.at || 0) < SEEN_FOR_SECONDS) {
            status[id] = "user";
        }
    }
    for (const id of Object.keys(await readSupporters(env))) {
        status[id] = "supporter";
    }
    return status;
}

/// The Twitch user behind a login, asked with the admin's own login
async function twitchUser(admin, login) {
    const answer = await fetch(
        `https://api.twitch.tv/helix/users?login=${encodeURIComponent(login)}`,
        {
            headers: {
                Authorization: `Bearer ${admin.token}`,
                "Client-Id": admin.clientId,
            },
        });
    if (!answer.ok) {
        return null;
    }
    const found = ((await answer.json()).data || [])[0];
    return found ? { id: String(found.id), login: String(found.login) }
                 : null;
}

function imageUrl(origin, id, version) {
    return `${origin}/image/${id}/${fileOf(version)}`;
}

async function setListed(env, id, login, version, size, client) {
    const list = await readList(env);
    if (version) {
        list[id] = { login, version, size: size || 72,
                     client: client || DEFAULT_CLIENT };
    } else {
        delete list[id];
    }
    await env.BADGES.put("list", JSON.stringify(list));
}

async function handle(request, env) {
    const url = new URL(request.url);
    const origin = url.origin;
    const parts = url.pathname.split("/").filter(Boolean);
    const method = request.method;

    // Everything approved, for every ChattiFlexii to show
    if (method === "GET" && parts.length === 1 && parts[0] === "badges") {
        const list = await readList(env);
        const users = {};
        for (const [id, entry] of Object.entries(list)) {
            users[id] = {
                login: entry.login,
                image: imageUrl(origin, id, entry.version),
                size: entry.size || 72,
                client: entry.client || DEFAULT_CLIENT,
            };
        }
        const status = await readStatus(env);
        return json({
            users,
            status,
            statusImages: {
                user: imageUrl(origin, "flexii", STATUS_IMAGES.user),
                supporter: imageUrl(origin, "flexii", STATUS_IMAGES.supporter),
            },
        }, 200, {
            "Cache-Control": `public, max-age=${LIST_CACHE_SECONDS}`,
        });
    }

    // A picture, approved or waiting - its version is a random word, so
    // only who was given the link finds one that waits
    if (method === "GET" && parts.length === 3 && parts[0] === "image") {
        const id = parts[1];
        const file = parts[2];
        let bytes = await env.BADGES.get(`image:${id}:${file}`, "arrayBuffer");
        if (!bytes && file.endsWith(".png")) {
            // Kept from before GIFs were taken, without an ending
            bytes = await env.BADGES.get(`image:${id}:${file.slice(0, -4)}`,
                                         "arrayBuffer");
        }
        if (!bytes) {
            return fail(404, "Kein Bild.");
        }
        return new Response(bytes, {
            headers: {
                "Content-Type": file.endsWith(".gif")    ? "image/gif"
                                : file.endsWith(".webp") ? "image/webp"
                                                         : "image/png",
                "Cache-Control": "public, max-age=31536000, immutable",
            },
        });
    }

    const user = await whoIs(request);
    if (user === null) {
        return fail(401, "Twitch kennt diesen Login nicht.");
    }

    // What this person has, and whether they may approve
    if (method === "GET" && parts.length === 1 && parts[0] === "me") {
        const record = await readUser(env, user.id);
        return json({
            userId: user.id,
            login: user.login,
            approved: record && record.approved
                          ? imageUrl(origin, user.id, record.approved)
                          : null,
            pending: record && record.pending
                         ? imageUrl(origin, user.id, record.pending)
                         : null,
            isAdmin: isAdmin(env, user),
            supporter: user.id in (await readSupporters(env)),
        });
    }

    // "I use ChattiFlexii" - for the user badge, at most once a day
    if (method === "POST" && parts.length === 1 && parts[0] === "hello") {
        const seen = await readSeen(env);
        const now = Math.floor(Date.now() / 1000);
        const last = seen[user.id];
        if (last && last.login === user.login &&
            now - (last.at || 0) < HELLO_EVERY_SECONDS) {
            return json({ seen: true });
        }
        seen[user.id] = { login: user.login, at: now };
        for (const [id, entry] of Object.entries(seen)) {
            if (now - (entry.at || 0) >= SEEN_FOR_SECONDS) {
                delete seen[id];
            }
        }
        await env.BADGES.put("seen", JSON.stringify(seen));
        return json({ seen: true });
    }

    // The user badge no longer wanted
    if (method === "DELETE" && parts.length === 1 && parts[0] === "hello") {
        const seen = await readSeen(env);
        if (user.id in seen) {
            delete seen[user.id];
            await env.BADGES.put("seen", JSON.stringify(seen));
        }
        return json({ seen: false });
    }

    // A new picture for one's own name - it waits until an admin approves it
    if (method === "POST" && parts.length === 1 && parts[0] === "upload") {
        const bytes = new Uint8Array(await request.arrayBuffer());
        const png = pngSize(bytes);
        const gif = png === null ? gifSize(bytes) : null;
        const size = png || gif;
        if (size === null) {
            return fail(415, "Nur PNG oder GIF.");
        }
        const limit = gif ? MAX_GIF_BYTES : MAX_PNG_BYTES;
        if (bytes.length > limit) {
            return fail(413, gif ? "Das GIF ist zu groß - höchstens 1 MB."
                                 : "Das Bild ist zu groß - höchstens 64 KB.");
        }
        if (size.width > MAX_SIDE || size.height > MAX_SIDE) {
            return fail(415, "Das Bild ist zu groß - höchstens 240 × 240.");
        }

        const record = (await readUser(env, user.id)) || {};
        const now = Math.floor(Date.now() / 1000);
        if (record.uploadedAt && now - record.uploadedAt < UPLOAD_EVERY_SECONDS) {
            return fail(429, "Gerade erst hochgeladen - in einer Minute wieder.");
        }

        if (record.pending) {
            await env.BADGES.delete(`image:${user.id}:${record.pending}`);
        }
        const version = `${randomVersion()}${gif ? ".gif" : ".png"}`;
        await env.BADGES.put(`image:${user.id}:${version}`, bytes);
        record.login = user.login;
        record.pending = version;
        record.pendingSize = size.width;
        record.pendingClient = clientOf(request);
        record.uploadedAt = now;
        await env.BADGES.put(`user:${user.id}`, JSON.stringify(record));

        // An admin's own needs nobody else to look at it
        if (isAdmin(env, user)) {
            return approve(env, origin, user.id);
        }
        return json({ pending: imageUrl(origin, user.id, version) });
    }

    // Taking one's own away, approved and waiting alike
    if (method === "DELETE" && parts.length === 1 && parts[0] === "mine") {
        await removeAll(env, user.id);
        return json({ removed: true });
    }

    if (!isAdmin(env, user)) {
        return fail(403, "Nur für Admins.");
    }

    // What waits for approval
    if (method === "GET" && parts.length === 1 && parts[0] === "pending") {
        const waiting = [];
        let cursor;
        do {
            const page = await env.BADGES.list({ prefix: "user:", cursor });
            for (const key of page.keys) {
                const record = await env.BADGES.get(key.name, "json");
                if (record && record.pending) {
                    const id = key.name.slice("user:".length);
                    waiting.push({
                        userId: id,
                        login: record.login,
                        image: imageUrl(origin, id, record.pending),
                        client: record.pendingClient || DEFAULT_CLIENT,
                        uploadedAt: record.uploadedAt || 0,
                    });
                }
            }
            cursor = page.list_complete ? undefined : page.cursor;
        } while (cursor);
        waiting.sort((a, b) => a.uploadedAt - b.uploadedAt);
        return json({ pending: waiting });
    }

    if (method === "POST" && parts.length === 2 && parts[0] === "approve") {
        return approve(env, origin, parts[1]);
    }

    if (method === "POST" && parts.length === 2 && parts[0] === "reject") {
        const record = await readUser(env, parts[1]);
        if (!record || !record.pending) {
            return fail(404, "Da wartet nichts.");
        }
        await env.BADGES.delete(`image:${parts[1]}:${record.pending}`);
        delete record.pending;
        await env.BADGES.put(`user:${parts[1]}`, JSON.stringify(record));
        return json({ rejected: true });
    }

    // The supporters, by name
    if (method === "GET" && parts.length === 1 && parts[0] === "supporters") {
        const supporters = await readSupporters(env);
        return json({
            supporters: Object.entries(supporters)
                .map(([id, login]) => ({ userId: id, login }))
                .sort((a, b) => a.login.localeCompare(b.login)),
        });
    }

    // Making someone a supporter, by their Twitch name
    if (method === "POST" && parts.length === 2 && parts[0] === "supporters") {
        const found = await twitchUser(user, parts[1].toLowerCase());
        if (found === null) {
            return fail(404, `Twitch kennt „${parts[1]}“ nicht.`);
        }
        const supporters = await readSupporters(env);
        supporters[found.id] = found.login;
        await env.BADGES.put("supporters", JSON.stringify(supporters));
        return json({ supporter: found });
    }

    if (method === "DELETE" && parts.length === 2 &&
        parts[0] === "supporters") {
        const supporters = await readSupporters(env);
        delete supporters[parts[1]];
        await env.BADGES.put("supporters", JSON.stringify(supporters));
        return json({ removed: true });
    }

    // Taking someone's badge away altogether
    if (method === "DELETE" && parts.length === 2 && parts[0] === "user") {
        await removeAll(env, parts[1]);
        return json({ removed: true });
    }

    return fail(404, "Unbekannt.");
}

async function approve(env, origin, id) {
    const record = await readUser(env, id);
    if (!record || !record.pending) {
        return fail(404, "Da wartet nichts.");
    }
    if (record.approved) {
        await env.BADGES.delete(`image:${id}:${record.approved}`);
    }
    record.approved = record.pending;
    record.approvedSize = record.pendingSize || 72;
    record.approvedClient = record.pendingClient || DEFAULT_CLIENT;
    delete record.pendingClient;
    delete record.pending;
    delete record.pendingSize;
    await env.BADGES.put(`user:${id}`, JSON.stringify(record));
    await setListed(env, id, record.login, record.approved,
                    record.approvedSize, record.approvedClient);
    return json({ approved: imageUrl(origin, id, record.approved) });
}

async function removeAll(env, id) {
    const record = await readUser(env, id);
    if (!record) {
        return;
    }
    for (const version of [record.approved, record.pending]) {
        if (version) {
            await env.BADGES.delete(`image:${id}:${version}`);
        }
    }
    await env.BADGES.delete(`user:${id}`);
    await setListed(env, id, record.login, null);
}

export default {
    async fetch(request, env) {
        try {
            return await handle(request, env);
        } catch (error) {
            return fail(500, `Fehler im Server: ${error}`);
        }
    },
};
