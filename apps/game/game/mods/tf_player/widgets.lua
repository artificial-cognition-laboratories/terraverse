-- Data for the map widget. The page chooses placement; the server chooses
-- what the player may see.

return function(modes)

local PERIOD = 2
local EXPLORE_GRID = 256
local EXPLORE_BYTES = EXPLORE_GRID * EXPLORE_GRID / 8
local elapsed = 0
local maps = {} -- player name → explored bitset and last visited cell
local terra = core.terra.enabled()
local world = terra and core.terra.world() or nil
local orbit = terra and core.orbit.planet() or nil
local sea = terra and tonumber(core.get_mapgen_setting("water_level")) or nil

-- An atlas world's bodies (each its own map: its world, its size, what
-- the player explored of it); a flat terra world's one map has no body.
local bodies = terra and core.planet.bodies() or nil
local home = bodies and bodies[1].id or nil
local named = {}
local listed = {} -- for the page: every body, the home first
for _, b in ipairs(bodies or {}) do
	named[b.id] = b
	listed[#listed + 1] = { id = b.id, name = b.name }
end

-- What a player has explored of a body, as kept (the home's under the
-- key it always had).
local function explored_key(body)
	return (body == nil or body == home) and "tf_explored" or ("tf_explored@" .. body)
end
local function load_explored(player, body)
	local stored = player:get_meta():get_string(explored_key(body))
	local explored = stored == "" and string.rep("\0", EXPLORE_BYTES) or core.decode_base64(stored)
	if #explored ~= EXPLORE_BYTES then
		error("tf_player: invalid explored map of " .. (body or "the world") .. " for " .. player:get_player_name())
	end
	return explored
end

-- The map of the body the player's on (the page draws it from the world
-- code and the body).
local function send_map(player, state)
	local b = state.body and named[state.body]
	player:set_ui("map", {
		code = world.code, body = state.body, name = b and b.name or nil, bodies = bodies and listed or nil,
		size = b and b.size or world.size, radius = b and b.radius or orbit.radius, sea = sea,
		star = world.star, tilt = world.tilt,
		mode = modes[player:get_player_name()] and "creative" or "survival",
		explored = core.encode_base64(state.explored), grid = EXPLORE_GRID,
	})
end

local function reveal(mask, cx, cz, radius)
	local changed = false
	for dz = -radius, radius do
		local z = cz + dz
		if z >= 0 and z < EXPLORE_GRID then
			for dx = -radius, radius do
				if dx * dx + dz * dz <= radius * radius then
					local x = (cx + dx) % EXPLORE_GRID
					local bit = z * EXPLORE_GRID + x
					local byte = math.floor(bit / 8) + 1
					local value = mask:byte(byte)
					local power = 2 ^ (bit % 8)
					if math.floor(value / power) % 2 == 0 then
						mask = mask:sub(1, byte - 1) .. string.char(value + power) .. mask:sub(byte + 1)
						changed = true
					end
				end
			end
		end
	end
	return mask, changed
end

local function map_at(player)
	if not terra then return end
	local name = player:get_player_name()
	local state = maps[name]
	-- in seam there's no column to place them by: as they were till they're
	-- out of it (one who joined in it is moved out: tf_planet)
	if core.terra.seam(player:get_pos()) then return end
	-- the body they're on (none in seam between tiles: as they were)
	local body = home and core.planet.body_at(player:get_pos()) or state.body
	if body ~= state.body then
		state.body = body
		state.explored = load_explored(player, body)
		state.x, state.z = nil, nil
		player:set_ui("body", { id = body, name = named[body].name, home = body == home })
		if state.ready then send_map(player, state) end
	end
	local size = body and named[body].size or world.size
	-- where they are as the map counts it: longitude east across `size`,
	-- latitude north across size / 2 (core.terra.place: the column's direction)
	local d = core.terra.place(player:get_pos())
	local pos = {
		x = math.atan2(d.z, d.x) / (2 * math.pi) * size,
		z = math.atan2(d.y, math.sqrt(d.x * d.x + d.z * d.z)) / math.pi * size,
	}
	local cx = math.floor((pos.x + size / 2) * EXPLORE_GRID / size) % EXPLORE_GRID
	local cz = math.max(0, math.min(EXPLORE_GRID - 1,
		math.floor((size / 2 - pos.z) * EXPLORE_GRID / size)))
	if state.x ~= cx or state.z ~= cz then
		state.x, state.z = cx, cz
		local radius = math.max(2, math.ceil(160 * EXPLORE_GRID / size))
		local changed
		state.explored, changed = reveal(state.explored, cx, cz, radius)
		if changed then
			player:get_meta():set_string(explored_key(body), core.encode_base64(state.explored))
			if state.ready and not modes[name] then send_map(player, state) end
		end
	end
	player:set_ui("map_position", { x = pos.x, z = pos.z, body = body, date = core.terra.calendar().date })
end

core.register_on_joinplayer(function(player)
	if terra then
		local state = { explored = load_explored(player, home) }
		maps[player:get_player_name()] = state
		map_at(player)
		send_map(player, state)
		state.ready = true
	end
end)

core.register_on_leaveplayer(function(player)
	maps[player:get_player_name()] = nil
end)

core.register_globalstep(function(dtime)
	elapsed = elapsed + dtime
	if elapsed < PERIOD then return end
	elapsed = 0
	for _, player in ipairs(core.get_connected_players()) do
		map_at(player)
	end
end)

-- The map's Go there (creative): to the ground at a place on the planet,
-- found as spawn finds it (the column's own grid, at its spawn level; over
-- the sea, at its surface). The page asks; the server decides.
core.register_on_ui_event(function(player, name, payload)
	if name ~= "map.teleport" then return end
	local who = player:get_player_name()
	if not core.planet.atlas() then
		core.log("warning", "tf_player: " .. who .. " asked to teleport, and this world isn't a planet")
		return
	end
	if not modes[who] then
		core.log("warning", "tf_player: " .. who .. " asked to teleport out of creative")
		return
	end
	local lat, lon = type(payload) == "table" and payload.latitude, type(payload) == "table" and payload.longitude
	if type(lat) ~= "number" or type(lon) ~= "number" or lat ~= lat or lon ~= lon or math.abs(lat) > math.pi / 2 then
		core.log("warning", "tf_player: " .. who .. " sent a teleport that isn't a place")
		return
	end
	-- on the body the map shows (the home's when it names none)
	local body = type(payload) == "table" and payload.body or nil
	if body ~= nil and not named[body] then
		core.log("warning", "tf_player: " .. who .. " sent a teleport to no body of this world")
		return
	end
	local column = core.planet.atlas_node(lat, lon, 0, body)
	if not column then
		core.log("warning", "tf_player: no ground at " .. lat .. ", " .. lon)
		return
	end
	local layer = core.get_spawn_level(column.x, column.z) or sea + 1
	local at_lat, at_lon = core.planet.atlas_place(column)
	local node = core.planet.atlas_node(at_lat, at_lon, layer, body)
	if not node then
		core.log("warning", "tf_player: no node at layer " .. layer .. " there")
		return
	end
	player:set_pos(node)
	core.log("action", "tf_player: " .. who .. " went to " .. core.pos_to_string(node))
end)

core.register_on_ui_event(function(player, name, payload)
	if name ~= "controls.set" or not terra then return end
	-- a change of mode changes what the map shows
	if type(payload) == "table" and payload.id == "player.mode" then
		core.after(0, function()
			if player:is_player() then send_map(player, maps[player:get_player_name()]) end
		end)
	end
end)

end
