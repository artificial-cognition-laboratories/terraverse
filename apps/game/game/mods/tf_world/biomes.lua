-- Biomes: one for every class terragen assigns (src/terragen/world.cpp
-- BIOME_NAMES). terragen decides *where* each biome is from the world's
-- climate; these say what it is made of. The terra map generator refuses to
-- run if one is missing. heat/humidity points only matter to Luanti's own
-- generators.

core.register_node("tf_world:snow", {
	description = "Snow",
	tiles = { "tf_goodvibes_snow.png" },
	groups = { loose = 1, crumbly = 3, pile_repose = 1, pile_yield = 1, pile_bearing = 4, pile_give = 8 },
	_matter = { matter = "water", share = 0.15 } -- snow is mostly air,
})

core.register_node("tf_world:ice", {
	description = "Ice",
	tiles = { "tf_goodvibes_packed_ice.png" },
	groups = { cracky = 3 },
	_matter = { matter = "water", share = 1 } -- the polar wall: ice, but never the form water freezes into,
})

core.register_node("tf_world:gravel", {
	description = "Gravel",
	tiles = { "tf_goodvibes_gravel.png" },
	groups = { loose = 1, crumbly = 3, falling_node = 1, pile_repose = 1, pile_yield = 0, pile_bearing = 80, pile_give = 2 },
	_matter = { matter = "gravel", share = 0.6 },
})

core.register_node("tf_world:dry_grass", {
	description = "Dry Grass",
	tiles = { "tf_goodvibes_grass_block_top.png^[colorize:#b49b5a:140",
		"tf_goodvibes_dirt.png",
		"tf_goodvibes_grass_block_side.png^[colorize:#b49b5a:100" },
	groups = { loose = 1, crumbly = 3, pile_repose = 1, pile_yield = 16, pile_bearing = 60, pile_give = 3 },
	_matter = { matter = "silica", share = 0.55 }, -- TEMP(2026-10-23): soil as its mineral part, quartz sand, until soils are modelled,
	drop = "tf_world:dirt",
})

core.register_node("tf_world:tundra", {
	description = "Tundra Moss",
	tiles = { "tf_goodvibes_grass_block_top.png^[colorize:#879a83:140",
		"tf_goodvibes_dirt.png",
		"tf_goodvibes_grass_block_side.png^[colorize:#879a83:100" },
	groups = { loose = 1, crumbly = 3, pile_repose = 1, pile_yield = 16, pile_bearing = 60, pile_give = 3 },
	_matter = { matter = "silica", share = 0.55 }, -- TEMP(2026-10-23): soil as its mineral part, quartz sand, until soils are modelled,
	drop = "tf_world:dirt",
})

-- The polar wall at the north and south edges of the world.

-- Beds under rivers and lakes: sand unless the biome says otherwise.
local function biome(name, def)
	def.name = name
	def.y_min = -31000
	def.y_max = 31000
	def.node_riverbed = def.node_riverbed or "tf_world:sand"
	def.depth_riverbed = 2
	core.register_biome(def)
end

-- Regolith: rock ground to dust by impacts where no air stops them (sprint
-- 95). Fine and sharp, it holds a footprint and a little wall.
for _, r in ipairs({ { "regolith", "Regolith" }, { "mare_regolith", "Mare regolith" } }) do
	core.register_node("tf_world:" .. r[1], {
		description = r[2],
		tiles = { "tf_goodvibes_" .. r[1] .. ".png" },
		groups = { loose = 1, crumbly = 3, pile_repose = 1, pile_yield = 8, pile_bearing = 20, pile_give = 4 },
		_matter = { matter = r[1], share = 0.55, form = true },
	})
end

-- the sea floor
biome("deep", { node_top = "tf_world:gravel", depth_top = 1, node_filler = "tf_world:gravel", depth_filler = 2, heat_point = 50, humidity_point = 50 })
biome("ocean", { node_top = "tf_world:sand", depth_top = 1, node_filler = "tf_world:sand", depth_filler = 3, heat_point = 50, humidity_point = 50 })
biome("seaice", { node_top = "tf_world:gravel", depth_top = 1, node_filler = "tf_world:gravel", depth_filler = 2,
	node_water_top = "tf_world:ice", depth_water_top = 2, heat_point = 0, humidity_point = 50 })

-- the land
biome("beach", { node_top = "tf_world:sand", depth_top = 2, node_filler = "tf_world:sand", depth_filler = 2, heat_point = 50, humidity_point = 50 })
biome("desert", { node_top = "tf_world:sand", depth_top = 2, node_filler = "tf_world:sandstone", depth_filler = 6, heat_point = 90, humidity_point = 10 })
biome("savanna", { node_top = "tf_world:dry_grass", depth_top = 1, node_filler = "tf_world:soil", depth_filler = 3, heat_point = 80, humidity_point = 40 })
biome("grassland", { node_top = "tf_world:grass", depth_top = 1, node_filler = "tf_world:soil", depth_filler = 3, heat_point = 50, humidity_point = 50 })
biome("forest", { node_top = "tf_world:grass", depth_top = 1, node_filler = "tf_world:soil", depth_filler = 3, heat_point = 60, humidity_point = 80 })
biome("jungle", { node_top = "tf_world:grass", depth_top = 1, node_filler = "tf_world:soil", depth_filler = 4, heat_point = 90, humidity_point = 90 })
-- the seasonal climates: monsoon forest (a wet summer, a dry winter), Mediterranean scrub (the reverse)
biome("monsoon", { node_top = "tf_world:grass", depth_top = 1, node_filler = "tf_world:soil", depth_filler = 4, heat_point = 85, humidity_point = 70 })
biome("mediterranean", { node_top = "tf_world:dry_grass", depth_top = 1, node_filler = "tf_world:soil", depth_filler = 2, heat_point = 65, humidity_point = 35 })
biome("taiga", { node_top = "tf_world:grass", depth_top = 1, node_filler = "tf_world:soil", depth_filler = 3, heat_point = 15, humidity_point = 55 })
biome("tundra", { node_top = "tf_world:tundra", depth_top = 1, node_filler = "tf_world:soil", depth_filler = 2, heat_point = 10, humidity_point = 30 })
biome("rock", { node_top = "tf_world:stone", depth_top = 1, node_filler = "tf_world:stone", depth_filler = 0,
	node_riverbed = "tf_world:gravel", heat_point = 40, humidity_point = 30 })
-- airless ground: the highlands' light dust, the maria's dark
biome("regolith", { node_top = "tf_world:regolith", depth_top = 2, node_filler = "tf_world:regolith", depth_filler = 0,
	node_riverbed = "tf_world:regolith", heat_point = 30, humidity_point = 0 })
biome("mare", { node_top = "tf_world:mare_regolith", depth_top = 2, node_filler = "tf_world:mare_regolith", depth_filler = 0,
	node_riverbed = "tf_world:mare_regolith", heat_point = 30, humidity_point = 0 })
biome("snow", { node_top = "tf_world:snow", depth_top = 1, node_filler = "tf_world:stone", depth_filler = 0,
	node_riverbed = "tf_world:gravel", heat_point = 0, humidity_point = 40 })
