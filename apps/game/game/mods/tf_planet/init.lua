-- The world as a planet (core.planet, src/planet/atlas.h): its segments,
-- caps and core are regions of the map, and every node that is no cell is
-- the seam: a wall nothing passes, never drawn. The generator places it
-- (mapgen_seam), cell by cell. Stepping from one region into the next is
-- the engine's (core.planet's) business; anything that walks or flows
-- across a seam without it meets the seam, never the wrong region's blocks.

local atlas = core.planet.atlas()
if not atlas then
	return
end

core.register_node("tf_planet:seam", {
	description = "Seam (between the planet's regions)",
	drawtype = "airlike",
	paramtype = "light",
	sunlight_propagates = true, -- no shadow along the joins
	walkable = true,
	pointable = false,
	diggable = false,
	buildable_to = false,
	is_ground_content = false,
	drop = "",
	groups = { not_in_creative_inventory = 1 },
	on_blast = function() end,
})

core.register_alias("mapgen_seam", "tf_planet:seam")

-- TEMP(2026-10-27): the debug sphere's paints (planet_atlas_debug, sprint 30):
-- a cell coloured by what it is, its edges lined so the grid shows.
do
	local PAINTS = {
		segment_a = "#8a9a8f", segment_b = "#a7b5aa", cap = "#5f86c4", core = "#7a5aa6",
		segment_seam = "#e0b050", cap_edge = "#f08a3c", cut = "#e8463a", core_edge = "#d36ca0", line = "#3b4a45",
	}
	for name, colour in pairs(PAINTS) do
		core.register_node("tf_planet:debug_" .. name, {
			description = "Atlas debug: " .. name,
			tiles = { "[fill:16x16:" .. colour .. "^[fill:16x1:0,0:#00000050^[fill:1x16:0,0:#00000050" },
			groups = { cracky = 3, not_in_creative_inventory = 1 },
			is_ground_content = false,
		})
	end
end

tf_planet = {}

-- On land, nearest the equator, east from longitude 0. Searched on the
-- half degree: whole degrees put it on the equator (the tiles' edge row)
-- and on every 15° (the segments' edges, whose columns narrow with depth).
-- The spawn level is a layer (the ground's height), found at the place
-- itself; only the one chosen gets a node (its tile put in the map's window).
-- `body`: its id (the home's when none is named).
-- A world that faces its star starts on the ring between day and night, on
-- either side (longitude 0 is under the star), if the ring has land at all.
local function search(body, west, east)
	for band = 0, 60, 5 do
		for _, lat in ipairs(band == 0 and { 0.5 } or { band + 0.5, -band - 0.5 }) do
			for lon = west, east, 2 do
				local layer = core.planet.spawn_level(math.rad(lat), math.rad(lon), body)
				local node = layer and core.planet.atlas_node(math.rad(lat), math.rad(lon), layer, body)
				if node then return node end
			end
		end
	end
end

function tf_planet.spawn_position(body)
	local found
	if core.terra.world().facing_star[body or ""] then
		found = search(body, 72.5, 90.5) or search(body, 270.5, 288.5)
	end
	return found or search(body, 0.5, 359.5)
		or error("tf_planet: no land within 60° of the equator to spawn on" .. (body and (" on " .. body) or ""))
end
local function spawn(player)
	player:set_pos(tf_planet.spawn_position())
	return true
end
core.register_on_newplayer(spawn)
core.register_on_respawnplayer(spawn)

dofile(core.get_modpath("tf_planet") .. "/travel.lua")
