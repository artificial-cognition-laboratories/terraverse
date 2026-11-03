-- Rock, ore and lava: the blocks terragen's geology places (src/terragen/
-- geology.*). The map generator finds them by alias — mapgen_rock_<rock>,
-- mapgen_ore_<ore>, mapgen_placer, mapgen_lava_source — and refuses to start
-- if one is missing. `_matter` names what each is made of in the engine's
-- matter tables (matter/*.json) — see tf_matter, which hands it to the
-- world's physics (core.matter).

local function tex(name)
	return "tf_goodvibes_" .. name .. ".png"
end

-- Rocks, by how hard they are to break (cracky 1 hardest … 3 softest).
local ROCKS = {
	{ "granite", "Granite", 2, "granite" },
	{ "gneiss", "Gneiss", 2, "gneiss" },
	{ "slate", "Slate", 3, "slate" },
	{ "basalt", "Basalt", 2, "basalt" },
	{ "andesite", "Andesite", 2, "andesite" },
	{ "tuff", "Tuff", 3, "tuff" },
	{ "obsidian", "Obsidian", 1, "obsidian" },
	{ "shale", "Shale", 3, "shale" },
	{ "limestone", "Limestone", 3, "limestone" },
	-- a cratered lid's (sprint 95): the highlands' crust and its shattered top
	{ "anorthosite", "Anorthosite", 2, "anorthosite" },
	{ "breccia", "Breccia", 3, "breccia" },
}
-- Each rock blends toward a geological neighbour (granite toward gneiss…):
-- its param2 is how far, 0..15, coloured by its palette and made of both
-- (scripts/assets/palettes.py draws the palettes from the same pairs).
local PARTNERS = { granite = "gneiss", gneiss = "slate", slate = "shale", basalt = "andesite", andesite = "tuff",
	tuff = "andesite", obsidian = "basalt", shale = "limestone", limestone = "sandstone",
	anorthosite = "breccia", breccia = "anorthosite" }
local BLENDS = 16

local function blends(rock, partner)
	local points = {}
	for k = 0, BLENDS - 1 do
		local t = k / (BLENDS - 1)
		points[k + 1] = t == 0 and { [rock] = 1 } or { [rock] = 1 - t, [partner] = t }
	end
	return points
end

for _, r in ipairs(ROCKS) do
	local id, label, hardness, matter = r[1], r[2], r[3], r[4]
	local partner = PARTNERS[id]
	core.register_node("tf_world:" .. id, {
		description = label,
		tiles = { tex(id) },
		paramtype2 = "color",
		palette = "tf_world_" .. id .. "_palette.png",
		groups = { cracky = hardness, stone = 1 },
		-- lava cools into basalt: its pure point is the form basalt's matter takes
		_family = { share = 0.99, form = id == "basalt", points = blends(matter, partner) },
		_point_name = function(k)
			if k == 0 then return label end
			return string.format("%s, %d%% %s", label, math.floor(100 * k / (BLENDS - 1) + 0.5), partner)
		end,
	})
	core.register_alias("mapgen_rock_" .. id, "tf_world:" .. id)
end
core.register_alias("mapgen_rock_sandstone", "tf_world:sandstone")
core.register_alias("mapgen_rock_ice", "tf_world:ice") -- an ice crust (sprint 98)

-- Ores: what they are, and the rock-like block they sit in.
local ORES = {
	{ "copper", "Copper ore (chalcopyrite)", "copper_ore", "chalcopyrite", 2 },
	{ "tin", "Tin ore (cassiterite)", "tin_ore", "cassiterite", 2 },
	{ "lead", "Lead ore (galena)", "lead_ore", "galena", 2 },
	{ "gold", "Gold ore", "gold_ore", "gold", 2 },
	{ "iron", "Banded iron ore (hematite)", "iron_ore", "hematite", 2 },
	{ "magnetite", "Magnetite", "magnetite_ore", "magnetite", 2 },
	{ "coal", "Coal", "coal_ore", "coal", 3 },
	{ "salt", "Rock salt", "salt", "salt", 3 },
	{ "sulfur", "Sulfur", "sulfur_ore", "sulfur", 3 },
	{ "ilmenite", "Ilmenite (iron and titanium)", "ilmenite_ore", "ilmenite", 2 },
	{ "kreep", "KREEP (thorium and rare earths)", "kreep_ore", "apatite", 2 },
}
for _, o in ipairs(ORES) do
	local id, label, texture, matter, hardness = o[1], o[2], o[3], o[4], o[5]
	core.register_node("tf_world:ore_" .. id, {
		description = label,
		tiles = { tex(texture) },
		groups = { cracky = hardness, ore = 1 },
		_matter = { matter = matter, share = 0.6 }, -- the ore mineral, in rock pores and veins
	})
	core.register_alias("mapgen_ore_" .. id, "tf_world:ore_" .. id)
end

-- Water ice in the cold traps: frozen into the dust of polar crater floors
core.register_alias("mapgen_ore_ice", "tf_world:ice")

-- River gravel out of the mountains, with gold and tin washed into it.
core.register_node("tf_world:placer_gravel", {
	description = "Gold-bearing gravel",
	tiles = { tex("placer_gravel") },
	groups = { loose = 1, crumbly = 2, falling_node = 1, pile_repose = 1, pile_yield = 0, pile_bearing = 80, pile_give = 2 },
	_matter = { matter = "gravel", share = 0.6 },
})
core.register_alias("mapgen_placer", "tf_world:placer_gravel")

core.register_node("tf_world:clay", {
	description = "Clay",
	tiles = { tex("clay") },
	groups = { loose = 1, crumbly = 3, pile_repose = 1, pile_yield = 24, pile_bearing = 50, pile_give = 4 },
	_matter = { matter = "kaolinite", share = 0.6, form = true },
})

-- Lava: molten rock. Slow, bright, and it burns.
local lava = {
	description = "Lava",
	drawtype = "liquid",
	tiles = { { name = tex("lava_still"), animation = { type = "vertical_frames", aspect_w = 64, aspect_h = 64, length = 3.0 } } },
	special_tiles = { { name = tex("lava_still"), animation = { type = "vertical_frames", aspect_w = 64, aspect_h = 64, length = 3.0 }, backface_culling = false } },
	paramtype = "light",
	light_source = 13,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	drowning = 1,
	damage_per_second = 8,
	liquid_alternative_flowing = "tf_world:lava_flowing",
	liquid_alternative_source = "tf_world:lava_source",
	liquid_viscosity = 7,
	liquid_renewable = false,
	liquid_range = 3,
	post_effect_color = { a = 200, r = 255, g = 90, b = 0 },
	groups = { lava = 3, liquid = 2, igniter = 1 },
	_matter = { matter = "basalt", share = 1, state = "liquid", form = true },
}
local source = table.copy(lava)
source.liquidtype = "source"
core.register_node("tf_world:lava_source", source)

local flowing = table.copy(lava)
flowing.drawtype = "flowingliquid"
flowing.liquidtype = "flowing"
flowing.paramtype2 = "flowingliquid"
flowing.tiles = { tex("lava_flow") }
flowing.special_tiles = {
	{ name = tex("lava_flow"), animation = { type = "vertical_frames", aspect_w = 128, aspect_h = 128, length = 2.0 }, backface_culling = false },
	{ name = tex("lava_flow"), animation = { type = "vertical_frames", aspect_w = 128, aspect_h = 128, length = 2.0 }, backface_culling = true },
}
flowing._matter = { matter = "basalt", share = 0.5, state = "liquid" } -- not the form: lava sources are
core.register_node("tf_world:lava_flowing", flowing)
core.register_alias("mapgen_lava_source", "tf_world:lava_source")
