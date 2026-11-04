-- The world's blocks as matter. Every block whose definition names its
-- `_matter` is handed to the world's physics (core.matter, the same physics
-- as the wiki's sandboxes): what it's made of, how much of its volume that
-- takes (the rest is air in its pores), its state, and whether it's the
-- block that matter becomes. Then the world runs: a fire in a hole, a kiln,
-- lava meeting water, and nothing anywhere else.
--
-- This mod runs it and shows it: steps the awake blocks, tells them the
-- weather, lights flames, sends up smoke, burns whoever stands in the heat,
-- keeps dug hot things hot, and records what each player has seen happen
-- for the wiki.

local modpath = core.get_modpath("tf_matter")
dofile(modpath .. "/blocks.lua")
dofile(modpath .. "/substances.lua")
dofile(modpath .. "/tools.lua")
dofile(modpath .. "/vessels.lua")
dofile(modpath .. "/pour.lua")
dofile(modpath .. "/lab.lua")

local AIR, FLAME = "air", "tf_matter:flame"

-- `_matter` is a matter id, or { matter, share, state, form }. Luanti's own
-- liquids (water, lava) flow by the engine, not by the world's matter.
local function spec(def)
	local m = def._matter
	if type(m) == "string" then m = { matter = m } end
	m = table.copy(m)
	m.flowing = def.liquidtype ~= nil and def.liquidtype ~= "none"
	return m
end

core.register_on_mods_loaded(function()
	core.matter.register_block(AIR, { matter = "air", form = true })
	-- by name, so kinds (and which block is the form of what) come out the same
	-- every time; the world's own blocks first, so where two are the form of the
	-- same matter, theirs wins (lava cools to basalt, not to a feldspar block)
	local world, generated = {}, {}
	for name, def in pairs(core.registered_nodes) do
		if def._matter or def._family then
			local list = def._matter_generated and generated or world
			list[#list + 1] = name
		end
	end
	table.sort(world)
	table.sort(generated)
	for _, list in ipairs({ world, generated }) do
		for _, name in ipairs(list) do
			local def = core.registered_nodes[name]
			if def._family then
				-- a family: param2 picks which of its compositions this block is
				core.matter.register_family(name, def._family)
			else
				core.matter.register_block(name, spec(def))
			end
		end
	end
	-- what each solid block weighs in a machine (core.physics, sprint 57): its
	-- matter's density; it grips as most things do, or barely if it's
	-- slippery, or as its `_machine.friction` says (rubber: sprint 74)
	-- what each takes before it fails (Pa), roughly as the real thing: `_machine.tension`
	-- and `.compression`, or by what breaks it. Sand's only held by being damp.
	local BY_GROUP = {
		{ "pile_yield", function(y) return y == 0 and 2e3 or 3e4, y == 0 and 5e4 or 3e5, 100 end, true },
		{ "cracky", function(h) local t = ({ 1e7, 5e6, 2e6 })[h] or 2e6; return t, t * 10, 100 end },
		{ "choppy", function() return 4e7, 3e7, 1e4 end },
		{ "crumbly", function() return 3e4, 3e5, 100 end },
		{ "snappy", function() return 1e5, 1e5, 1e3 end },
	}
	local function strength(def, machine)
		if machine.tension then
			return machine.tension, machine.compression or machine.tension, machine.toughness
		end
		for _, g in ipairs(BY_GROUP) do
			local level = def.groups and def.groups[g[1]]
			if level then
				return g[2](level)
			end
		end
	end
	local unweighed, unjudged = {}, {}
	for name, def in pairs(core.registered_nodes) do
		if def.walkable ~= false and name ~= "air" and name ~= "ignore" then
			local density = core.matter.density(name)
			if density and density > 0 then
				local slippery = def.groups and def.groups.slippery
				local machine = def._machine or {}
				-- (what tyres get from it too, workshop::surfaceGrip: loose ground gives under them)
				local loose = def.groups and def.groups.pile_bearing
				local friction = machine.friction or (slippery and 0.1 or loose and 0.35 or 0.6)
				-- a fitting weighs what it says (sprint 78: an engine is a
				-- made thing, not a cubic metre of iron): its block's a metre
				-- cubed, so its mass is its density
				local tension, compression, toughness = strength(def, machine)
				if not tension and not machine.role then
					unjudged[#unjudged + 1] = name
				end
				core.physics.material(name, { density = machine.mass or density, friction = friction,
					role = machine.role, power = machine.power, rpm = machine.rpm,
					tension = tension, compression = compression, toughness = toughness })
			else
				unweighed[#unweighed + 1] = name
			end
		end
	end
	if #unjudged > 0 then
		table.sort(unjudged)
		core.log("action", "tf_matter: " .. #unjudged .. " solid blocks have no strength, so they never fail: "
			.. table.concat(unjudged, ", "))
	end
	if #unweighed > 0 then
		table.sort(unweighed)
		core.log("action", "tf_matter: " .. #unweighed .. " solid blocks have no matter, so a machine of them can't move: "
			.. table.concat(unweighed, ", "))
	end
end)

-- ---- running it --------------------------------------------------------

local STEP = 0.5   -- s between steps of the world's matter
local SHOW = 0.5   -- s between looking at it near players
local REACH = 20   -- blocks around a player that are shown
local HOT = 330    -- K: hot enough to hurt

local function weather_at(pos)
	if not core.terra.enabled() then return nil end
	return core.terra.weather(pos).temperature + 273.15
end

-- the weather's temperature for the cubes around each player
local function tell_weather()
	for _, player in ipairs(core.get_connected_players()) do
		local pos = vector.round(player:get_pos())
		local t = weather_at(pos)
		if t then
			for dx = -32, 32, 16 do
				for dy = -32, 32, 16 do
					for dz = -32, 32, 16 do
						core.matter.ambient(vector.offset(pos, dx, dy, dz), t)
					end
				end
			end
		end
	end
end

-- flames where air burns, smoke where gas isn't air, glow on hot solids
local lit = {} -- pos hash → true where a flame node stands
local function show()
	local burning = {}
	for _, player in ipairs(core.get_connected_players()) do
		local pos = vector.round(player:get_pos())
		local lo, hi = vector.subtract(pos, REACH), vector.add(pos, REACH)
		for _, a in ipairs(core.matter.awake(lo, hi)) do
			local flame = a.activity > 50 and a.temperature > 700 and a.fill < 0.5
			if flame then
				burning[core.hash_node_position(a.pos)] = a.pos
				core.add_particlespawner({
					amount = math.min(40, 8 + math.floor(a.activity / 1500)), time = SHOW,
					minpos = vector.offset(a.pos, -0.4, -0.5, -0.4), maxpos = vector.offset(a.pos, 0.4, 0.1, 0.4),
					minvel = { x = -0.1, y = 0.8, z = -0.1 }, maxvel = { x = 0.1, y = 1.8, z = 0.1 },
					minexptime = 0.25, maxexptime = 0.6, minsize = 0.8, maxsize = 2.2,
					texture = { name = "tf_matter_flame.png", alpha_tween = { 1, 0 }, scale_tween = { 1, 0.3 } }, glow = 14,
				})
			end
			if a.vapour and a.haze > 0.05 and a.vapour ~= "oxygen" then
				local steam = a.vapour == "water"
				core.add_particlespawner({
					amount = math.ceil(a.haze * 12), time = SHOW,
					minpos = vector.offset(a.pos, -0.5, 0, -0.5), maxpos = vector.offset(a.pos, 0.5, 0.5, 0.5),
					minvel = { x = -0.1, y = 0.4, z = -0.1 }, maxvel = { x = 0.1, y = 1.0, z = 0.1 },
					minexptime = 1.5, maxexptime = 3, minsize = 2, maxsize = 4.5,
					texture = { name = steam and "tf_matter_steam.png" or "tf_matter_smoke.png", alpha_tween = { 0.7, 0 }, scale_tween = { 0.6, 1.6 } },
				})
			end
			if a.fill >= 0.5 and a.temperature > 900 then -- glowing: sparks off it
				core.add_particlespawner({
					amount = 3, time = SHOW,
					minpos = vector.offset(a.pos, -0.5, 0.5, -0.5), maxpos = vector.offset(a.pos, 0.5, 0.5, 0.5),
					minvel = { x = -0.3, y = 1, z = -0.3 }, maxvel = { x = 0.3, y = 2, z = 0.3 },
					minexptime = 0.5, maxexptime = 1.2, minsize = 0.3, maxsize = 0.6,
					texture = { name = "tf_matter_spark.png", alpha_tween = { 1, 0 } }, glow = 14,
				})
			end
		end
	end
	-- a flame's light: the flame node where air burns, back to air where it stops
	for hash, pos in pairs(burning) do
		if not lit[hash] and core.get_node(pos).name == AIR then
			core.swap_node(pos, { name = FLAME })
			lit[hash] = true
		end
	end
	for hash in pairs(lit) do
		if not burning[hash] then
			local pos = core.get_position_from_hash(hash)
			if core.get_node(pos).name == FLAME then core.swap_node(pos, { name = AIR }) end
			lit[hash] = nil
		end
	end
end

-- standing in heat burns: by the hotter of the blocks at the feet and the head
local function scald()
	for _, player in ipairs(core.get_connected_players()) do
		if player:get_hp() > 0 then
			local feet = vector.round(player:get_pos())
			local t = math.max(core.matter.temperature(feet), core.matter.temperature(vector.offset(feet, 0, 1, 0)))
			if t > HOT then
				player:set_hp(player:get_hp() - math.ceil((t - HOT) / 60), { type = "set_hp", cause = "heat" })
			end
		end
	end
end

-- ---- hot things you carry ---------------------------------------------
-- A block dug while hot keeps its temperature as an item, cooling toward the
-- weather; held hot, it burns; placed hot, the block is as hot as it was.

local COOLING = 120 -- s: how fast a carried thing loses its heat (an e-folding time)

local function temperature_of(stack)
	local t = stack:get_meta():get_float("temperature")
	return t > 0 and t or nil
end

local function set_temperature(stack, t, ambient)
	local meta = stack:get_meta()
	if not t or t < ambient + 5 then
		meta:set_string("temperature", "")
		meta:set_string("description", "")
		return
	end
	meta:set_float("temperature", t)
	local def = core.registered_items[stack:get_name()]
	meta:set_string("description", (def and def.description or stack:get_name()) .. string.format("\n%d °C", math.floor(t - 273.15)))
end

-- A block of a family keeps its composition as an item (its palette_index,
-- which Luanti carries to the item and back to the block): named for it.
local function name_point(stack)
	local def = core.registered_nodes[stack:get_name()]
	local k = stack:get_meta():get_int("palette_index")
	if def and def._point_name and stack:get_meta():contains("palette_index") then
		stack:get_meta():set_string("description", def._point_name(k))
	end
end

local handle_drops = core.handle_node_drops
function core.handle_node_drops(pos, drops, digger)
	local t = core.matter.temperature(pos)
	local ambient = weather_at(pos) or 288.15
	for i, item in ipairs(drops) do
		local stack = ItemStack(item)
		name_point(stack)
		if t > ambient + 5 then set_temperature(stack, t, ambient) end
		drops[i] = stack
	end
	return handle_drops(pos, drops, digger)
end

core.register_on_placenode(function(pos, _, _, _, stack)
	local t = temperature_of(stack)
	if t then core.matter.heat(pos, t) end
end)

-- a block dug or built: liquids and heaps beside it run into the hole or
-- against the wall, and a liquid placed starts to run (sprint 80)
core.register_on_dignode(function(pos) core.matter.disturb(pos) end)

-- Sprint 81: liquid lies in open space as a fill, not as a levelled block.
-- A puddle block from before wakes, and the world's matter makes it the
-- air its liquid lies in.
core.register_lbm({
	label = "puddles become fills",
	name = "tf_matter:puddles_to_fills",
	nodenames = { "group:matter_puddle" },
	run_at_every_load = true,
	action = function(pos) core.matter.disturb(pos) end,
})
core.register_on_placenode(function(pos) core.matter.disturb(pos) end)

local function carry(dt)
	local keep = math.exp(-dt / COOLING)
	for _, player in ipairs(core.get_connected_players()) do
		local inv = player:get_inventory()
		local ambient = weather_at(player:get_pos()) or 288.15
		local list = inv:get_list("main")
		local changed = false
		for i, stack in ipairs(list) do
			local t = temperature_of(stack)
			if t then
				set_temperature(stack, ambient + (t - ambient) * keep, ambient)
				changed = true
			end
		end
		if changed then inv:set_list("main", list) end
		local held = temperature_of(player:get_wielded_item())
		if held and held > HOT and player:get_hp() > 0 then
			player:set_hp(player:get_hp() - math.ceil((held - HOT) / 120), { type = "set_hp", cause = "heat" })
		end
	end
end

-- ---- what each player has seen happen (the wiki fills from it) ----------

local SEEN = 16 -- blocks: close enough to have seen it

local function seen_by(player)
	local list = player:get_meta():get_string("tf_discovered")
	local out = {}
	for id in list:gmatch("[^,]+") do out[id] = true end
	return out
end

local function tell_seen(player)
	local ids = {}
	for id in pairs(seen_by(player)) do ids[#ids + 1] = id end
	table.sort(ids)
	player:set_ui("discovered", ids)
end

local function witness()
	for _, h in ipairs(core.matter.happened()) do
		for _, player in ipairs(core.get_connected_players()) do
			if vector.distance(player:get_pos(), h.pos) <= SEEN then
				local seen = seen_by(player)
				if not seen[h.reaction] then
					seen[h.reaction] = true
					local ids = {}
					for id in pairs(seen) do ids[#ids + 1] = id end
					table.sort(ids)
					player:get_meta():set_string("tf_discovered", table.concat(ids, ","))
					tell_seen(player)
				end
			end
		end
	end
end

core.register_on_joinplayer(tell_seen)

-- ---- the clock ------------------------------------------------------------

local since_step, since_show, since_weather, since_save = 0, 0, 5, 0
core.register_globalstep(function(dtime)
	since_step = since_step + dtime
	since_show = since_show + dtime
	since_weather = since_weather + dtime
	since_save = since_save + dtime
	if since_weather >= 5 then
		tell_weather()
		since_weather = 0
	end
	if since_step >= STEP then
		core.matter.step(since_step)
		carry(since_step)
		scald()
		witness()
		since_step = 0
	end
	if since_show >= SHOW then
		show()
		since_show = 0
	end
	if since_save >= 60 then
		core.matter.save()
		since_save = 0
	end
end)

core.register_on_shutdown(function()
	core.matter.save()
end)
