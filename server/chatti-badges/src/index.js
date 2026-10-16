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
//   user:<id>         {login, approved, pending, uploadedAt} - approved and
//                     pending are versions, a random word each
//   image:<id>:<ver>  the PNG itself
//   list              {"<id>": {"login", "version"}} of everything approved,
//                     so the list is one read rather than a scan

const MAX_BYTES = 64 * 1024;
const MAX_SIDE = 128;
const UPLOAD_EVERY_SECONDS = 60;
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
    return { id: String(body.user_id), login: String(body.login) };
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

function imageUrl(origin, id, version) {
    return `${origin}/image/${id}/${version}.png`;
}

async function setListed(env, id, login, version) {
    const list = await readList(env);
    if (version) {
        list[id] = { login, version };
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
            };
        }
        return json({ users }, 200, {
            "Cache-Control": `public, max-age=${LIST_CACHE_SECONDS}`,
        });
    }

    // A picture, approved or waiting - its version is a random word, so
    // only who was given the link finds one that waits
    if (method === "GET" && parts.length === 3 && parts[0] === "image") {
        const id = parts[1];
        const version = parts[2].replace(/\.png$/, "");
        const bytes = await env.BADGES.get(`image:${id}:${version}`,
                                           "arrayBuffer");
        if (!bytes) {
            return fail(404, "Kein Bild.");
        }
        return new Response(bytes, {
            headers: {
                "Content-Type": "image/png",
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
        });
    }

    // A new picture for one's own name - it waits until an admin approves it
    if (method === "POST" && parts.length === 1 && parts[0] === "upload") {
        const bytes = new Uint8Array(await request.arrayBuffer());
        if (bytes.length > MAX_BYTES) {
            return fail(413, "Das Bild ist zu groß - höchstens 64 KB.");
        }
        const size = pngSize(bytes);
        if (size === null) {
            return fail(415, "Das ist kein PNG.");
        }
        if (size.width > MAX_SIDE || size.height > MAX_SIDE) {
            return fail(415, "Das Bild ist zu groß - höchstens 128 × 128.");
        }

        const record = (await readUser(env, user.id)) || {};
        const now = Math.floor(Date.now() / 1000);
        if (record.uploadedAt && now - record.uploadedAt < UPLOAD_EVERY_SECONDS) {
            return fail(429, "Gerade erst hochgeladen - in einer Minute wieder.");
        }

        if (record.pending) {
            await env.BADGES.delete(`image:${user.id}:${record.pending}`);
        }
        const version = randomVersion();
        await env.BADGES.put(`image:${user.id}:${version}`, bytes);
        record.login = user.login;
        record.pending = version;
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
    delete record.pending;
    await env.BADGES.put(`user:${id}`, JSON.stringify(record));
    await setListed(env, id, record.login, record.approved);
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
