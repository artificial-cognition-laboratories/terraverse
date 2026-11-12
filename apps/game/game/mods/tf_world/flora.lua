-- Plant blocks (sprint 113): what the generator's grown plants are made of.
-- terragen says where a plant stands and flora grows it (apps/game/src/flora);
-- these are its parts as blocks, each cut to the sixteenths its voxels fill
-- and coloured its lineage's, from one palette (param2: scripts/flora-palette.py).

local PALETTE = "tf_flora_palette.png"

local function part(id, label, tiles, extra)
	local def = {
		description = label,
		tiles = tiles,
		paramtype = "light",
		sunlight_propagates = true, -- (a canopy shades, it doesn't black out)
		paramtype2 = "color",
		palette = PALETTE,
	}
	for k, v in pairs(extra) do
		def[k] = v
	end
	core.register_node("tf_world:" .. id, def)
end

part("plant_wood", "Wood", { "tf_flora_wood.png", "tf_flora_wood.png", "tf_flora_bark.png" }, {
	groups = { choppy = 2, tree = 1 },
	_matter = { matter = "wood", share = 0.4 },
})
part("plant_leaf", "Leaves", { "tf_flora_leaf.png" }, {
	groups = { snappy = 3, leaves = 1 },
	_matter = { matter = "wood", share = 0.05 },
})
part("plant_flesh", "Stem", { "tf_flora_flesh.png" }, {
	groups = { snappy = 3, choppy = 3, plant = 1 },
	_matter = { matter = "wood", share = 0.1 },
})
part("plant_flower", "Flower", { "tf_flora_leaf.png" }, {
	groups = { snappy = 3, plant = 1 },
	_matter = { matter = "wood", share = 0.05 },
})

-- A small plant's foot: the plant itself is a sprig the engine keeps with
-- this node (which plant, how it stands) and the client grows and draws.
-- Space you walk through and build into; replacing it drops the plant.
core.register_node("tf_world:growth", {
	description = "Small plant",
	drawtype = "airlike",
	paramtype = "light",
	sunlight_propagates = true,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	floodable = true,
	drop = "",
	groups = { plant = 1, not_in_creative_inventory = 1 },
})

-- How small plants look: never placed, the engine draws sprigs with its tiles,
-- one for each part of a plant in flora::Part's order (wood, branch, leaf,
-- flesh, flower, vein), greys its colours tint. Waving: they bend in the wind.
core.register_node("tf_world:sprig", {
	description = "Small plant (look)",
	drawtype = "nodebox",
	node_box = { type = "regular" },
	tiles = { "tf_flora_bark.png", "tf_flora_bark.png", "tf_flora_leaf.png", "tf_flora_flesh.png", "tf_flora_leaf.png",
		"tf_flora_leaf.png" },
	use_texture_alpha = "clip",
	waving = 1,
	paramtype = "light",
	sunlight_propagates = true,
	walkable = false,
	pointable = false,
	diggable = false,
	drop = "",
	groups = { not_in_creative_inventory = 1 },
})
