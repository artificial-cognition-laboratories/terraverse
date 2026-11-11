-- The world controls: the mod menu. Pages of controls as data, drawn by the
-- page (WorldControlsWidget), so a mod adds its own with tf_controls.page.
--
-- A control is { id, kind = "button" | "toggle" | "slider" | "choice" | "fact",
-- label, note?, get?, set? } and, by kind, min/max/step/format or options. With
-- `setting` it's this client's own engine setting: the page reads and writes it
-- (shell/options.cpp says which it may), and nothing reaches the server. `always`
-- shows it out of creative too, or there'd be no way back in. `confirm` asks twice.
--
-- Who gets it: holders of the server privilege, whose changes reach everyone.

return function(modes, set_mode, damage)

local PERIOD = 2
local terra = core.terra.enabled()
local pages = {}
local controls = {} -- id → control
local sent = {}     -- player name → what they were last sent, as JSON

local tf_controls = {}
rawset(_G, "tf_controls", tf_controls)

function tf_controls.page(page)
	for _, section in ipairs(page.sections) do
		for _, control in ipairs(section.controls) do
			if controls[control.id] then error("tf_controls: two controls called " .. control.id) end
			controls[control.id] = control
		end
	end
	table.insert(pages, page.at or #pages + 1, page)
end

local function can_control(player)
	return core.check_player_privs(player, { server = true })
end

-- What's sent of a control: its data, and its value now
local function shown(control, player)
	local out = {}
	for k, v in pairs(control) do
		if type(v) ~= "function" then out[k] = v end
	end
	if control.get then out.value = control.get(player) end
	return out
end

local function send(player, force)
	local name = player:get_player_name()
	local value
	if can_control(player) then
		local creative = modes[name]
		local out = {}
		for _, page in ipairs(pages) do
			local sections = {}
			for _, section in ipairs(page.sections) do
				local list = {}
				for _, control in ipairs(section.controls) do
					if (creative or control.always) and (not control.available or control.available(player)) then
						list[#list + 1] = shown(control, player)
					end
				end
				if #list > 0 then
					sections[#sections + 1] = { title = section.title, hint = section.hint, controls = list }
				end
			end
			if #sections > 0 then
				out[#out + 1] = { id = page.id, title = page.title, icon = page.icon, sections = sections }
			end
		end
		value = { mode = creative and "creative" or "survival", pages = out }
	end
	local json = value and core.write_json(value) or ""
	if force or sent[name] ~= json then
		sent[name] = json
		player:set_ui("world_controls", value)
	end
end

local function send_all()
	for _, player in ipairs(core.get_connected_players()) do send(player) end
end

core.register_on_joinplayer(function(player) send(player, true) end)
core.register_on_leaveplayer(function(player) sent[player:get_player_name()] = nil end)

local elapsed = 0
core.register_globalstep(function(dtime)
	elapsed = elapsed + dtime
	if elapsed < PERIOD then return end
	elapsed = 0
	send_all()
end)

-- A value's fit for its control; the page isn't trusted with the range
local VALID = {
	button = function(_, value) return value == nil end,
	toggle = function(_, value) return type(value) == "boolean" end,
	slider = function(c, value) return type(value) == "number" and value == value and value >= c.min and value <= c.max end,
	choice = function(c, value)
		for _, option in ipairs(c.options) do
			if option.value == value then return true end
		end
		return false
	end,
}

core.register_on_ui_event(function(player, name, payload)
	if name ~= "controls.set" then return end
	local who = player:get_player_name()
	local control = type(payload) == "table" and controls[payload.id]
	if not control or not control.set or not VALID[control.kind](control, payload.value) then
		core.log("warning", "tf_player: " .. who .. " sent a control that isn't one")
		return
	end
	if not can_control(player) or not (modes[who] or control.always)
			or (control.available and not control.available(player)) then
		core.log("warning", "tf_player: " .. who .. " may not use " .. payload.id)
		return
	end
	core.log("action", "tf_player: " .. who .. " used " .. payload.id)
	control.set(player, payload.value)
	send_all()
end)

-- Time ------------------------------------------------------------------

local BASE_SPEED = tonumber(core.settings:get("time_speed")) or error("tf_player: no time_speed")
local paused, speed = false, 1
local function apply_speed() core.set_time_speed(paused and 0 or BASE_SPEED * speed) end

-- Time is the player's planet's own: noon is noon, however long its day
local function hour_of(player)
	if terra then return (core.terra.hour(player:get_pos())) end
	return core.get_timeofday()
end
local function set_hour(player, hour)
	if not terra then
		core.set_timeofday(hour)
	elseif not core.terra.set_hour(player:get_pos(), hour) then
		core.chat_send_player(player:get_player_name(), "This body always faces its star: it has only the one time of day.")
	end
end
local function moment(id, label, hour)
	return { id = "time." .. id, kind = "button", label = label, set = function(player) set_hour(player, hour) end }
end
local function skip(id, label, days)
	return {
		id = "time." .. id, kind = "button", label = label,
		available = function() return terra end,
		set = function(player)
			local hour = hour_of(player)
			core.terra.skip_days(days())
			set_hour(player, hour)
		end,
	}
end

tf_controls.page({
	id = "time", title = "Time", icon = "time",
	sections = {
		{ title = "Time of day", hint = "On this planet", controls = {
			moment("dawn", "Dawn", 0.25), moment("noon", "Noon", 0.5),
			moment("dusk", "Dusk", 0.75), moment("midnight", "Midnight", 0),
			{ id = "time.hour", kind = "slider", label = "Set time", min = 0, max = 0.99, step = 0.01, format = "clock",
				get = hour_of, set = set_hour },
		} },
		{ title = "Clock", hint = "For everyone", controls = {
			{ id = "time.passes", kind = "toggle", label = "Time passes",
				get = function() return not paused end,
				set = function(_, value) paused = not value; apply_speed() end },
			{ id = "time.speed", kind = "choice", label = "Speed",
				options = { { value = 1, label = "×1" }, { value = 10, label = "×10" }, { value = 100, label = "×100" } },
				get = function() return speed end,
				set = function(_, value) speed = value; apply_speed() end },
		} },
		{ title = "Date", controls = {
			{ id = "time.date", kind = "fact", label = "Year, day",
				available = function() return terra end,
				get = function()
					local c = core.terra.calendar()
					return "Year " .. c.year .. ", day " .. c.day .. " of " .. c.year_days
				end },
			{ id = "time.season", kind = "fact", label = "Season here",
				available = function() return terra end,
				get = function(player) return core.terra.sun(player:get_pos()).season end },
			{ id = "time.length", kind = "fact", label = "Length of the day",
				available = function() return terra end,
				get = function(player)
					local _, hours = core.terra.hour(player:get_pos())
					return hours and string.format("%.1f h", hours) or "Faces its star"
				end },
			skip("next_day", "Next day", function() return 1 end),
			skip("next_season", "Next season", function()
				return math.max(1, math.floor(core.terra.calendar().year_days / 4))
			end),
		} },
	},
})

-- Player ----------------------------------------------------------------

tf_controls.page({
	id = "player", title = "Player", icon = "player",
	sections = {
		{ title = "Mode", hint = "For this player", controls = {
			{ id = "player.mode", kind = "choice", label = "Mode", always = true,
				options = { { value = "survival", label = "Survival" }, { value = "creative", label = "Creative" } },
				get = function(player) return modes[player:get_player_name()] and "creative" or "survival" end,
				set = function(player, value) set_mode(player, value == "creative", true) end },
		} },
		{ title = "Moving", controls = {
			{ id = "player.fly", kind = "toggle", label = "Fly", setting = "free_move" },
			{ id = "player.fast", kind = "toggle", label = "Fast", setting = "fast_move", note = "While flying" },
			{ id = "player.noclip", kind = "toggle", label = "Through walls", setting = "noclip", note = "While flying" },
		} },
		{ title = "Body", controls = {
			{ id = "player.heal", kind = "button", label = "Heal",
				set = function(player)
					player:set_hp(player:get_properties().hp_max)
					player:set_breath(player:get_properties().breath_max)
				end },
		} },
	},
})

-- Travel ----------------------------------------------------------------

local SPOT = "tf_controls_spot"
local function bodies()
	local planet = rawget(_G, "tf_planet")
	return planet and core.planet.bodies() or nil
end
local function travel_buttons()
	local list = {}
	for _, body in ipairs(bodies() or {}) do
		list[#list + 1] = {
			id = "travel.body." .. body.id, kind = "button", label = body.name,
			set = function(player)
				local ok, why = tf_planet.travel(player, body.id)
				if not ok then core.chat_send_player(player:get_player_name(), why) end
			end,
		}
	end
	return list
end

-- (other mods load after this one: the planets are known once they have)
core.register_on_mods_loaded(function()
	local section = { title = "Places", controls = {
		{ id = "travel.save", kind = "button", label = "Save this spot",
			set = function(player)
				player:get_meta():set_string(SPOT, core.pos_to_string(player:get_pos()))
			end },
		{ id = "travel.back", kind = "button", label = "Back to it",
			available = function(player) return player:get_meta():get_string(SPOT) ~= "" end,
			set = function(player)
				player:set_pos(core.string_to_pos(player:get_meta():get_string(SPOT)))
			end },
		{ id = "travel.map", kind = "button", label = "Open the map", opens = "map",
			available = function() return terra end },
	} }
	local sections = { section }
	local buttons = travel_buttons()
	if #buttons > 0 then sections[2] = { title = "Bodies", hint = "Go to its equator", controls = buttons } end
	tf_controls.page({ id = "travel", at = 3, title = "Travel", icon = "travel", sections = sections })
end)

-- World -----------------------------------------------------------------

local rules = Settings(core.get_worldpath() .. "/world.mt")
local function fact(id, label, get, available)
	return { id = "world." .. id, kind = "fact", label = label, get = get, available = available }
end
local function weather_fact(id, label, read)
	return fact(id, label, function(player) return read(core.terra.weather(player:get_pos())) end, function() return terra end)
end
tf_controls.page({
	id = "world", title = "World", icon = "world",
	sections = {
		{ title = "Rules", hint = "Set when the world was made", controls = {
			fact("damage", "Damage", function() return damage and "On" or "Off" end),
			fact("wiki", "Wiki", function() return rules:get_bool("tf_wiki", true) and "On" or "Off" end),
		} },
		{ title = "Weather here", hint = "Read only: nothing can set it yet", controls = {
			weather_fact("temperature", "Temperature", function(w) return string.format("%.1f °C", w.temperature) end),
			weather_fact("humidity", "Humidity", function(w) return string.format("%d%%", w.humidity * 100) end),
			weather_fact("cloud", "Cloud", function(w) return string.format("%d%%", w.cloud * 100) end),
			weather_fact("rain", "Rain or snow", function(w) return string.format("%.1f mm/h", w.precipitation) end),
		} },
	},
})

-- Items -----------------------------------------------------------------

local function kit(id, label, items)
	return {
		id = "items." .. id, kind = "button", label = label,
		set = function(player)
			local inv = player:get_inventory()
			for _, item in ipairs(items) do
				if not core.registered_items[item:match("^%S+")] then error("tf_player: no item " .. item) end
				local left = inv:add_item("main", item)
				if not left:is_empty() then
					core.chat_send_player(player:get_player_name(), "No room for all of it.")
					return
				end
			end
		end,
	}
end
tf_controls.page({
	id = "items", title = "Items", icon = "items",
	sections = {
		{ title = "Kits", controls = {
			kit("tools", "Tools", { "tf_matter:fire_drill", "tf_matter:heat_gun", "tf_matter:blowtorch", "tf_matter:blast_gun", "tf_matter:flamethrower", "tf_matter:probe", "tf_matter:scoop" }),
			kit("fire", "Fire", { "tf_matter:kindling 20", "tf_matter:charcoal 20", "tf_matter:fired_clay 40", "tf_matter:bellows", "tf_matter:crucible" }),
			kit("building", "Building", { "tf_world:stone 99", "tf_world:stone_bricks 99", "tf_world:sandstone 99", "tf_world:sandstone_bricks 99", "tf_world:dirt 99", "tf_world:sand 99" }),
		} },
		{ title = "Inventory", controls = {
			{ id = "items.clear", kind = "button", label = "Clear inventory", confirm = true, tone = "danger",
				set = function(player)
					local inv = player:get_inventory()
					inv:set_list("main", {})
				end },
		} },
	},
})

-- Tuning: this client's own movement numbers ----------------------------

local function knob(setting, label, min, max, step, default, unit)
	return { id = "tuning." .. setting, kind = "slider", label = label, setting = "tune_" .. setting,
		min = min, max = max, step = step, default = default, unit = unit }
end
tf_controls.page({
	id = "tuning", title = "Tuning", icon = "tuning",
	sections = {
		{ title = "Wall kick", controls = {
			knob("kick_out", "Out from the wall", 0, 12, 0.1, 4.5, "n/s"),
			knob("kick_steer", "Along the look", 0, 10, 0.1, 3.5, "n/s"),
		} },
		{ title = "Wall run", controls = {
			knob("wall_run_time", "Longest", 0.2, 4, 0.1, 1.2, "s"),
			knob("wall_run_lift", "Lift", 0, 6, 0.1, 2, "n/s"),
			knob("wall_run_gravity", "Gravity", 0, 1, 0.05, 0.35, "×"),
		} },
		{ title = "Hanging", controls = {
			knob("shimmy", "Shimmy", 0.2, 4, 0.1, 1.2, "n/s"),
		} },
		{ title = "Landing", controls = {
			knob("hard_landing", "Hard landing from", 3, 30, 0.5, 7, "n/s"),
			knob("roll_late", "Late roll window", 0, 1, 0.05, 0.2, "s"),
			knob("falls_from", "Falls at balance", 0.1, 1.5, 0.05, 0.85),
			knob("falls_at", "Falls at any landing", 5, 60, 0.5, 11, "n/s"),
		} },
		{ title = "Reset", controls = {
			{ id = "tuning.reset", kind = "button", label = "Back to defaults", resets = true },
		} },
	},
})

-- View: what this client draws over the world ---------------------------

tf_controls.page({
	id = "view", title = "View", icon = "view",
	sections = {
		{ title = "Readouts", controls = {
			{ id = "view.text", kind = "toggle", label = "Debug text", setting = "view_debug_text" },
			{ id = "view.movement", kind = "toggle", label = "Movement readout", setting = "view_movement", note = "In the debug text" },
			{ id = "view.profiler", kind = "toggle", label = "Profiler graph", setting = "view_profiler" },
		} },
		{ title = "Overlays", controls = {
			{ id = "view.wireframe", kind = "toggle", label = "Wireframe", setting = "view_wireframe" },
			{ id = "view.bboxes", kind = "toggle", label = "Object boxes", setting = "view_bboxes" },
			{ id = "view.bounds", kind = "choice", label = "Block bounds", setting = "view_block_bounds",
				options = { { value = "off", label = "Off" }, { value = "current", label = "This block" }, { value = "near", label = "Nearby" } } },
		} },
	},
})

-- `/time` means this planet's time too, as the menu does
if terra then
	local builtin = core.registered_chatcommands.time.func
	core.override_chatcommand("time", {
		func = function(name, param)
			local player = core.get_player_by_name(name)
			if not player then return builtin(name, param) end
			local get, set = core.get_timeofday, core.set_timeofday
			core.get_timeofday = function() return hour_of(player) end
			core.set_timeofday = function(hour) set_hour(player, hour) end
			local ok, ok_said, said = pcall(builtin, name, param)
			core.get_timeofday, core.set_timeofday = get, set
			if not ok then error(ok_said) end
			return ok_said, said
		end,
	})
end

end
