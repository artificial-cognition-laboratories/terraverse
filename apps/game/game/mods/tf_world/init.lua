local water_texture = "tf_goodvibes_water_still.png"
local water_animation = { type = "vertical_frames", aspect_w = 512, aspect_h = 512, length = 4.0 }

core.register_item(":", {
	type = "none",
	wield_image = "blank.png",
	tool_capabilities = {
		full_punch_interval = 0.9,
		max_drop_level = 0,
		groupcaps = {
			crumbly = { times = { [1] = 3.0, [2] = 1.5, [3] = 0.6 }, uses = 0 },
			choppy = { times = { [2] = 6, [3] = 1.2 }, uses = 0 }, -- a trunk by hand, slowly; an axe is the way
			cracky = { times = { [3] = 6.0 }, uses = 0 },
			snappy = { times = { [3] = 0.4 }, uses = 0 },
		},
		damage_groups = { fleshy = 1 },
	},
})

core.register_node("tf_world:stone", {
	description = "Stone",
	tiles = { "tf_goodvibes_stone.png" },
	groups = { cracky = 3 },
	_matter = { matter = "granite", share = 0.99 },
})

-- pile_* (sprint 84): how loose ground moves when pressed (src/pile): it
-- rests at pile_repose sixteenths a column, holds pile_yield more before it
-- gives, sinks a sixteenth per pile_bearing kPa, packs at most pile_give.
core.register_node("tf_world:dirt", {
	description = "Dirt",
	tiles = { "tf_goodvibes_dirt.png" },
	groups = { loose = 1, crumbly = 3, pile_repose = 1, pile_yield = 16, pile_bearing = 60, pile_give = 3 },
	_matter = { matter = "silica", share = 0.55 }, -- TEMP(2026-10-23): soil as its mineral part, quartz sand, until soils are modelled,
})

core.register_node("tf_world:grass", {
	description = "Grass",
	tiles = { "tf_goodvibes_grass_block_top.png", "tf_goodvibes_dirt.png",
		"tf_goodvibes_grass_block_side.png" },
	groups = { loose = 1, crumbly = 3, pile_repose = 1, pile_yield = 16, pile_bearing = 60, pile_give = 3 },
	_matter = { matter = "silica", share = 0.55 }, -- TEMP(2026-10-23): soil as its mineral part, quartz sand, until soils are modelled,
	drop = "tf_world:dirt",
})

core.register_node("tf_world:sand", {
	description = "Sand",
	tiles = { "tf_goodvibes_sand.png" },
	groups = { loose = 1, crumbly = 3, falling_node = 1, pile_repose = 1, pile_yield = 0, pile_bearing = 12, pile_give = 4 },
	_matter = { matter = "silica", share = 0.6, form = true }, -- grains, air between them,
})

-- Water is the world's matter's, not the engine's (sprint 80): a full block
-- (the sea, a lake, a river) and a levelled one (a puddle, a pool's edge,
-- water running), and core.matter runs both: it falls, runs level, fills
-- holes, evaporates. The sea is asleep and costs nothing until something
-- changes beside it. You swim in both.
local water = {
	description = "Water",
	drawtype = "liquid",
	tiles = { { name = water_texture, animation = water_animation } },
	special_tiles = { { name = water_texture, animation = water_animation, backface_culling = false } },
	use_texture_alpha = "blend",
	paramtype = "light",
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	liquid_move_physics = true,
	drowning = 1,
	post_effect_color = { a = 110, r = 30, g = 70, b = 140 },
	color = "#3a7fc4", -- water's own colour (the matter's), drawn by the liquid layer
	groups = { water = 3, liquid = 3, liquid_layer = 1 }, -- drawn by the liquid layer, so it meets the shore's fills (sprint 82)
	_matter = { matter = "water", share = 1, state = "liquid", form = true }, -- the form liquid water takes
}
core.register_node("tf_world:water_source", water)

local puddle = table.copy(water)
puddle.description = "Shallow water"
puddle.drawtype = "nodebox"
puddle.node_box = { type = "leveled", fixed = { -0.5, -0.5, -0.5, 0.5, 0.5, 0.5 } }
puddle.tiles = { { name = water_texture, animation = water_animation, backface_culling = false } }
puddle.paramtype2 = "leveled"
puddle.leveled = 64
puddle.buildable_to = false -- it'd be lost: build into it and it's gone
-- you wade through it (Luanti swims by the block, not by how deep it is)
puddle.liquid_move_physics = false
puddle.drowning = 0
puddle.post_effect_color = nil
puddle.color = nil
puddle.groups = { water = 3, liquid = 3, matter_puddle = 1, not_in_creative_inventory = 1 } -- (sprint 81: a fill now; made one on load)
core.register_node("tf_world:water_puddle", puddle)
-- worlds from before kept the engine's running water
core.register_alias("tf_world:water_flowing", "tf_world:water_puddle")

-- The engine's built-in mapgens place these by alias.
core.register_alias("mapgen_stone", "tf_world:stone")
core.register_alias("mapgen_water_source", "tf_world:water_source")
core.register_alias("mapgen_river_water_source", "tf_world:water_source")

assert(loadfile(core.get_modpath("tf_world") .. "/geology.lua"))()
assert(loadfile(core.get_modpath("tf_world") .. "/soil.lua"))() -- before the biomes that fill with it
assert(loadfile(core.get_modpath("tf_world") .. "/biomes.lua"))()
assert(loadfile(core.get_modpath("tf_world") .. "/shore.lua"))()
assert(loadfile(core.get_modpath("tf_world") .. "/vegetation.lua"))()

-- Placeholder starter kit until survival progression exists: anyone joining
-- with an empty inventory gets a few of each block, so the HUD has something to show.
core.register_on_joinplayer(function(player)
	local inv = player:get_inventory()
	if not inv:is_empty("main") then
		return
	end
	for _, name in ipairs({ "tf_world:grass", "tf_world:dirt", "tf_world:stone", "tf_world:sand" }) do
		inv:add_item("main", name .. " 64")
	end
end)

-- Placeholder crafts so the inventory's crafting grid has something to make.
-- the rock, as the world's beds are: the sedimentary rocks' grain, sand-coloured
-- (the Good Vibes sandstone is dressed masonry, so it's the bricks below)
core.register_node("tf_world:sandstone", {
	description = "Sandstone",
	tiles = { "tf_goodvibes_limestone.png^[multiply:#f4d6a4" },
	groups = { cracky = 3 },
	_matter = { matter = "sandstone", share = 0.85 },
})

core.register_node("tf_world:sandstone_bricks", {
	description = "Sandstone Bricks",
	tiles = { "tf_goodvibes_sandstone_top.png", "tf_goodvibes_sandstone_bottom.png",
		"tf_goodvibes_sandstone.png" },
	groups = { cracky = 3 },
	_matter = { matter = "sandstone", share = 0.95 },
})

core.register_node("tf_world:stone_bricks", {
	description = "Stone Bricks",
	tiles = { "tf_goodvibes_stone_bricks.png" },
	groups = { cracky = 3 },
	_matter = { matter = "granite", share = 0.95 },
})

core.register_craft({
	output = "tf_world:sandstone",
	recipe = { { "tf_world:sand", "tf_world:sand" }, { "tf_world:sand", "tf_world:sand" } },
})

core.register_craft({
	output = "tf_world:sandstone_bricks 4",
	recipe = { { "tf_world:sandstone", "tf_world:sandstone" }, { "tf_world:sandstone", "tf_world:sandstone" } },
})

core.register_craft({
	output = "tf_world:stone_bricks 4",
	recipe = { { "tf_world:stone", "tf_world:stone" }, { "tf_world:stone", "tf_world:stone" } },
})
dofile(core.get_modpath("tf_world") .. "/lab.lua")
dofile(core.get_modpath("tf_world") .. "/pieces.lua")
dofile(core.get_modpath("tf_world") .. "/materials.lua")
dofile(core.get_modpath("tf_world") .. "/ground.lua")
dofile(core.get_modpath("tf_world") .. "/drops.lua")
dofile(core.get_modpath("tf_world") .. "/felling.lua")
