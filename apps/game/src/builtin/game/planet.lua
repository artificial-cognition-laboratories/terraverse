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
