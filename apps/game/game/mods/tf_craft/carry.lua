-- Carrying (AGENTS.md: Building is easy, distance is what costs). Everything
-- in the inventory weighs what it is: an item says (`_mass`, kg each), and a
-- block weighs a litre of its matter. That's the magic that lets a house be
-- carried in, its mass still real enough to choose what's worth hauling: ore
-- is heavy, wood light. The page gets it as each item's group mass_g (grams:
-- groups are whole numbers), and it slows whoever carries it, never refusing.
local BLOCK = 0.001 -- m³: a block in the inventory
-- kg carried: no slower up to EASY, walking pace by HEAVY, a crawl by CRAWL
local EASY, HEAVY, CRAWL = 25, 50, 150
local PACE = { walk = 0.7, crawl = 0.2 }

local mass = {} -- kg each, by item name

core.register_on_mods_loaded(function()
	local unweighed = {}
	for name, def in pairs(core.registered_items) do
		local kg = def._mass
		if not kg and core.registered_nodes[name] and name ~= "air" and name ~= "ignore" then
			local density = core.matter.density(name)
			kg = density and density > 0 and density * BLOCK or nil
		end
		if kg then
			mass[name] = kg
			local groups = table.copy(def.groups or {})
			groups.mass_g = math.max(1, math.floor(kg * 1000 + 0.5))
			core.override_item(name, { groups = groups })
		elseif name ~= "" and name ~= "air" and name ~= "ignore" and name ~= "unknown" then
			unweighed[#unweighed + 1] = name
		end
	end
	if #unweighed > 0 then
		table.sort(unweighed)
		core.log("warning", "tf_craft: weighs nothing yet (no _mass or matter): " .. table.concat(unweighed, ", "))
	end
end)

local function carried(player)
	local kg = 0
	for _, list in pairs(player:get_inventory():get_lists()) do
		for _, stack in ipairs(list) do
			if not stack:is_empty() then kg = kg + (mass[stack:get_name()] or 0) * stack:get_count() end
		end
	end
	return kg
end

local function lerp(a, b, t) return a + (b - a) * math.min(1, math.max(0, t)) end
local function pace(kg)
	if kg <= EASY then return 1 end
	if kg <= HEAVY then return lerp(1, PACE.walk, (kg - EASY) / (HEAVY - EASY)) end
	return lerp(PACE.walk, PACE.crawl, (kg - HEAVY) / (CRAWL - HEAVY))
end

local told = {} -- what each player's page was last sent, in tenths of a kg
local every = 0
core.register_globalstep(function(dtime)
	every = every + dtime
	if every < 0.25 then return end
	every = 0
	for _, player in ipairs(core.get_connected_players()) do
		local who = player:get_player_name()
		local kg = carried(player)
		local tenths = math.floor(kg * 10 + 0.5)
		if told[who] ~= tenths then
			told[who] = tenths
			local p = pace(kg)
			player:set_physics_override({ speed_walk = p, jump = lerp(1, 0.6, (kg - EASY) / (CRAWL - EASY)) })
			player:set_ui("carrying", { kg = tenths / 10, easy = EASY, heavy = HEAVY, crawl = CRAWL, pace = p })
		end
	end
end)
core.register_on_leaveplayer(function(player) told[player:get_player_name()] = nil end)
