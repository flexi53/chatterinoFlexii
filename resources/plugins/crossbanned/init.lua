-- CrossBanned Chatterino Plugin
-- Befehl: /cbcheck             → First-Chatter-Check für diesen Channel an/aus
-- Befehl: /cbcheck <name|id>   → manueller Check
-- Befehl: /cbcheck clean       → "Kein Report"-Meldung für Auto-Check an/aus

local API_URL     = "https://crossbanned.de/api/bot/check?q="
local RESOLVE_URL = "https://crossbanned.de/api/bot/resolve?username="

local function load_set(filename)
    local ok, f = pcall(io.open, filename, "r")
    if not ok or not f then return {} end
    local content = f:read("*a")
    f:close()
    local t = {}
    for ch in content:gmatch('"([^"]+)"') do t[ch] = true end
    return t
end

local function save_set(t, filename)
    local ok, f = pcall(io.open, filename, "w")
    if not ok or not f then
        c2.log(c2.LogLevel.Warning, "CrossBanned: State konnte nicht gespeichert werden.")
        return
    end
    local names = {}
    for ch in pairs(t) do names[#names + 1] = '"' .. ch .. '"' end
    f:write("[" .. table.concat(names, ",") .. "]")
    f:close()
end

local enabled    = load_set("enabled.json") -- { channelName = true }
local handles    = {}                        -- { channelName = ConnectionHandle }

local function load_show_clean()
    local ok, f = pcall(io.open, "show_clean.json", "r")
    if not ok or not f then return false end
    local content = f:read("*a")
    f:close()
    return content:match("true") ~= nil
end

local function save_show_clean(val)
    local ok, f = pcall(io.open, "show_clean.json", "w")
    if not ok or not f then return end
    f:write(val and "true" or "false")
    f:close()
end

local show_clean = load_show_clean() -- global boolean

local function post(channel, text)
    channel:add_system_message("[CrossBanned] " .. text)
end

local function parse_response(body)
    if body:match('"found":(%a+)') ~= "true" then return nil end
    local function unescape(s)
        if not s then return "" end
        return s:gsub('\\"', '"'):gsub('\\n', ' '):gsub('\\/', '/'):gsub('\\\\', '\\')
    end
    return {
        username   = unescape(body:match('"username":"([^"]*)"')),
        reason     = unescape(body:match('"reason":"([^"]*)"')),
        report_url = unescape(body:match('"reportUrl":"([^"]*)"')),
        proof      = unescape(body:match('"proof":"([^"]*)"')),
        evidence   = unescape(body:match('"evidence":{[^}]-"url":"([^"]*)"')),
        banned     = tonumber(body:match('"bannedOnChannels":(%d+)'))   or 0,
        unbanned   = tonumber(body:match('"unbannedOnChannels":(%d+)')) or 0,
    }
end

local function post_report(channel, data, prefix)
    local ban_info = ""
    if data.banned > 0 or data.unbanned > 0 then
        ban_info = string.format(" | gebannt auf %d Kanal%s",
            data.banned, data.banned == 1 and "" or "en")
        if data.unbanned > 0 then
            ban_info = ban_info .. string.format(", entbannt auf %d", data.unbanned)
        end
    end
    local reason = data.reason
    if #reason > 80 then reason = reason:sub(1, 77) .. "..." end

    local links = ""
    if data.report_url and #data.report_url > 0 then
        links = links .. " | Report: " .. data.report_url
    end
    if data.evidence and #data.evidence > 0 then
        links = links .. " | Video/Logs: " .. data.evidence
    elseif data.proof and #data.proof > 0 then
        links = links .. " | Beweis: " .. data.proof
    end

    post(channel, string.format("%s%s%s | Grund: %s%s",
        prefix, data.username, ban_info, reason, links))

    -- Eigene System-Message mit Highlighted-Flag versehen → Ping-Sound + roter Tab
    pcall(function()
        local msg = channel:last_message()
        if msg then
            channel:replace_message(msg, msg, c2.MessageFlag.Highlighted)
        end
    end)

end

local function check_by_id(twitch_id, display_name, channel, prefix, silent_on_miss)
    local req = c2.HTTPRequest.create(c2.HTTPMethod.Get, API_URL .. twitch_id)
    req:on_success(function(res)
        if res:status() == 429 then
            if not silent_on_miss then
                post(channel, "Rate-Limit erreicht – bitte kurz warten.")
            end
            return
        end
        if res:status() ~= 200 then
            if not silent_on_miss then
                post(channel, string.format("Fehler (HTTP %d).", res:status()))
            end
            return
        end
        local data = parse_response(res:data())
        if not data then
            if not silent_on_miss then
                post(channel, string.format("✓ Kein Report für: %s", display_name))
            end
            return
        end
        post_report(channel, data, prefix)
    end)
    req:on_error(function()
        if not silent_on_miss then post(channel, "API nicht erreichbar.") end
    end)
    req:execute()
end

-- First-Chatter-Callback für einen Channel registrieren
local function register_channel(channel)
    local ch_name = channel:get_name()
    if handles[ch_name] then return end

    local ok, handle = pcall(function()
        return channel:on_message_appended(function(msg)
            if (msg.flags & c2.MessageFlag.FirstMessage) == 0 then return end
            local user_id   = msg.user_id
            local user_name = msg.display_name or msg.login_name or "Unbekannt"
            if not user_id or user_id == "" then return end
            check_by_id(user_id, user_name, channel, "⚠ FIRST CHATTER: ", not show_clean)
        end)
    end)

    if ok and handle then
        handles[ch_name] = handle
        c2.log(c2.LogLevel.Info, "CrossBanned: First-Chatter-Check aktiv für #" .. ch_name)
    else
        c2.log(c2.LogLevel.Warning, "CrossBanned: on_message_appended nicht verfügbar (Chatterino zu alt). Nur /cbcheck manuell nutzbar.")
    end
end

-- Beim Laden gespeicherte Channels wiederherstellen (falls bereits geöffnet)
for ch_name in pairs(enabled) do
    local channel = c2.Channel.by_name(ch_name)
    if channel then
        register_channel(channel)
    else
        c2.log(c2.LogLevel.Info, "CrossBanned: #" .. ch_name .. " noch nicht geöffnet, wird beim nächsten /cbcheck reaktiviert.")
    end
end

-- /cbcheck Befehl
c2.register_command("/cbcheck", function(ctx)
    local query = ctx.words[2]

    if query and #query > 0 then
        -- Toggle "Sauber"-Meldung für Auto-Check
        if query == "clean" then
            show_clean = not show_clean
            save_show_clean(show_clean)
            if show_clean then
                post(ctx.channel, "\"Kein Report\"-Meldung global aktiviert (saubere First-Chatter werden gemeldet).")
            else
                post(ctx.channel, "\"Kein Report\"-Meldung global deaktiviert (nur Funde werden gemeldet).")
            end
            return
        end

        -- Manueller Check
        if query:match("^%d+$") then
            check_by_id(query, query, ctx.channel, "Report gefunden: ", false)
        else
            post(ctx.channel, string.format("Suche Twitch-ID für '%s'...", query))
            local req = c2.HTTPRequest.create(c2.HTTPMethod.Get, RESOLVE_URL .. query)
            req:on_success(function(res)
                if res:status() == 429 then
                    post(ctx.channel, "Rate-Limit erreicht – bitte kurz warten.")
                    return
                end
                if res:status() == 404 then
                    post(ctx.channel, string.format("Twitch-User '%s' nicht gefunden.", query))
                    return
                end
                if res:status() ~= 200 then
                    post(ctx.channel, string.format("Fehler beim Auflösen (HTTP %d).", res:status()))
                    return
                end
                local twitch_id = res:data():match('"twitchId":"(%d+)"')
                local login     = res:data():match('"login":"([^"]*)"') or query
                if not twitch_id then
                    post(ctx.channel, string.format("Twitch-User '%s' nicht gefunden.", query))
                    return
                end
                check_by_id(twitch_id, login, ctx.channel, "Report gefunden: ", false)
            end)
            req:on_error(function(res)
                local err = ""
                if res and res.error then err = ": " .. tostring(res.error) end
                post(ctx.channel, "Resolve-Anfrage fehlgeschlagen" .. err .. ". Bitte Internetverbindung prüfen.")
            end)
            req:execute()
        end
    else
        -- Toggle für diesen Channel
        local ch_name = ctx.channel:get_name()
        if enabled[ch_name] then
            enabled[ch_name] = nil
            if handles[ch_name] then
                handles[ch_name]:block()
                handles[ch_name] = nil
            end
            save_set(enabled, "enabled.json")
            post(ctx.channel, string.format("First-Chatter-Check für #%s deaktiviert.", ch_name))
        else
            enabled[ch_name] = true
            register_channel(ctx.channel)
            save_set(enabled, "enabled.json")
            post(ctx.channel, string.format("First-Chatter-Check für #%s aktiviert.", ch_name))
        end
    end
end)

c2.log(c2.LogLevel.Info, "CrossBanned Plugin geladen.")
