-- Terra Firma: the planet's own blocks and its admin command (sprint 119: were
-- tf_planet's). Standing on it, spawning and travel are native (server/planet.cpp).

-- The seam: every node that is no cell, between the planet's regions. A wall
-- nothing passes, never drawn; the generator places it (mapgen_seam), cell by cell.
core.register_node(":core:seam", {
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
core.register_alias("mapgen_seam", "core:seam")
core.register_alias("tf_planet:seam", "core:seam") -- worlds saved before it was the engine's

-- TEMP(2026-10-27): the debug sphere's paints (planet_atlas_debug, sprint 30):
-- a cell coloured by what it is, its edges lined so the grid shows.
for name, colour in pairs({
	segment_a = "#8a9a8f", segment_b = "#a7b5aa", cap = "#5f86c4", core = "#7a5aa6",
	segment_seam = "#e0b050", cap_edge = "#f08a3c", cut = "#e8463a", core_edge = "#d36ca0", line = "#3b4a45",
}) do
	core.register_node(":core:atlas_debug_" .. name, {
		description = "Atlas debug: " .. name,
		tiles = { "[fill:16x16:" .. colour .. "^[fill:16x1:0,0:#00000050^[fill:1x16:0,0:#00000050" },
		groups = { cracky = 3, not_in_creative_inventory = 1 },
		is_ground_content = false,
	})
	core.register_alias("tf_planet:debug_" .. name, "core:atlas_debug_" .. name)
end

-- /travel: to another body, behind the travel screen. An admin's, as /teleport is:
-- getting between bodies in play is flight's, not a command's.
core.register_chatcommand("travel", {
	params = "<body> [<latitude> <longitude>]",
	description = "Go to another body of the world (degrees; its equator when no place is given)",
	privs = { teleport = true },
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then return false, "not in the world" end
		local bodies = core.planet.bodies()
		if not bodies then return false, "this world has no other bodies" end
		local body, lat, lon = param:match("^(%S+)%s*(%S*)%s*(%S*)$")
		if not body then
			local ids = {}
			for _, b in ipairs(bodies) do ids[#ids + 1] = b.id end
			return false, "travel to one of: " .. table.concat(ids, ", ")
		end
		lat, lon = tonumber(lat), tonumber(lon)
		if (lat == nil) ~= (lon == nil) or (lat and math.abs(lat) > 90) then
			return false, "a place is a latitude (−90 to 90) and a longitude, in degrees"
		end
		local ok, why = core.planet.travel(player, body, lat, lon)
		return ok, ok and ("travelling to " .. body) or why
	end,
})
