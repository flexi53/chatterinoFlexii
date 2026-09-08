local BASE_URL   = "https://whosthemod.xyz"
local MAX_INLINE = 5

local function extract_total(body, key)
    return tonumber(body:match('"' .. key .. '":(%d+)'))
end

local function extract_list(body, key)
    local result = {}
    local arr = body:match('"' .. key .. '":%[(.-)%]')
    if arr then
        for item in arr:gmatch('"([^"]+)"') do
            result[#result + 1] = item
        end
    end
    return result
end

local function build_list_str(items, total)
    local shown = {}
    for i = 1, math.min(MAX_INLINE, #items) do
        shown[#shown + 1] = items[i]
    end
    local str  = table.concat(shown, ", ")
    local rest = total - #shown
    if rest > 0 then str = str .. " (+" .. rest .. " weitere)" end
    return str
end

-- /wtm <username> — zeigt Mod-Kanäle inkl. ehemalige
c2.register_command("/wtm", function(ctx)
    local username = ctx.words[2]
    if not username or username == "" then
        ctx.channel:add_system_message("[WhosTheMod] Nutzung: /wtm <username>")
        return
    end

    username = username:lower():gsub("[^a-z0-9_]", "")
    if username == "" then
        ctx.channel:add_system_message("[WhosTheMod] Ungültiger Nutzername.")
        return
    end

    ctx.channel:add_system_message("[WhosTheMod] Suche Mod-Kanäle für " .. username .. "…")

    local req = c2.HTTPRequest.create(c2.HTTPMethod.Get,
        BASE_URL .. "/api/public/check?user=" .. username)
    req:set_timeout(10000)

    req:on_success(function(res)
        if res:status() ~= 200 then
            ctx.channel:add_system_message("[WhosTheMod] Fehler " .. tostring(res:status()))
            return
        end

        local body  = res:data()
        local total = extract_total(body, "total")
        if not total then
            ctx.channel:add_system_message("[WhosTheMod] Antwort konnte nicht verarbeitet werden.")
            return
        end

        local channels      = extract_list(body, "channels")
        local former        = extract_list(body, "former_channels")
        local former_total  = extract_total(body, "former_total") or 0
        local url           = BASE_URL .. "/?check=" .. username

        if total == 0 and former_total == 0 then
            ctx.channel:add_system_message(
                "[WhosTheMod] " .. username .. " ist in keinem bekannten Kanal Mod. → " .. url)
            return
        end

        local parts = {}
        if total > 0 then
            parts[#parts + 1] = "Mod in " .. total .. " Kanal" .. (total == 1 and "" or "en") ..
                ": " .. build_list_str(channels, total)
        end
        if former_total > 0 then
            parts[#parts + 1] = "Ehemals: " .. build_list_str(former, former_total)
        end

        ctx.channel:add_system_message(
            "[WhosTheMod] " .. username .. " — " .. table.concat(parts, " | ") .. " → " .. url)
    end)

    req:on_error(function()
        ctx.channel:add_system_message("[WhosTheMod] Verbindungsfehler. Bitte erneut versuchen.")
    end)

    req:execute()
end)

-- /modcheck <channel> — zeigt Mods eines Kanals
c2.register_command("/modcheck", function(ctx)
    local channel = ctx.words[2]
    if not channel or channel == "" then
        ctx.channel:add_system_message("[WhosTheMod] Nutzung: /modcheck <channel>")
        return
    end

    channel = channel:lower():gsub("^#", ""):gsub("[^a-z0-9_]", "")
    if channel == "" then
        ctx.channel:add_system_message("[WhosTheMod] Ungültiger Kanalname.")
        return
    end

    ctx.channel:add_system_message("[WhosTheMod] Lade Mods von #" .. channel .. "…")

    local req = c2.HTTPRequest.create(c2.HTTPMethod.Get,
        BASE_URL .. "/api/public/channel-mods?channel=" .. channel)
    req:set_timeout(10000)

    req:on_success(function(res)
        if res:status() ~= 200 then
            ctx.channel:add_system_message("[WhosTheMod] Fehler " .. tostring(res:status()))
            return
        end

        local body  = res:data()
        local total = extract_total(body, "total")
        if not total then
            ctx.channel:add_system_message("[WhosTheMod] Antwort konnte nicht verarbeitet werden.")
            return
        end

        local mods = extract_list(body, "mods")
        local url  = BASE_URL .. "/?channel=" .. channel

        if total == 0 then
            ctx.channel:add_system_message(
                "[WhosTheMod] #" .. channel .. " hat keine bekannten Mods. → " .. url)
            return
        end

        ctx.channel:add_system_message(
            "[WhosTheMod] #" .. channel .. " hat " .. total .. " Mod" .. (total == 1 and "" or "s") ..
            ": " .. build_list_str(mods, total) .. " → " .. url)
    end)

    req:on_error(function()
        ctx.channel:add_system_message("[WhosTheMod] Verbindungsfehler. Bitte erneut versuchen.")
    end)

    req:execute()
end)
