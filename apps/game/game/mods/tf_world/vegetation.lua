-- Vegetation. terragen decides how much can grow on each column and where
-- trees stand (core.terra.cover: warmth, moisture, slope, banks, soil); this
-- decides *what* grows: each species has a niche, and the one that fits a
-- spot is planted there. Add a species by adding a row.

local function tex(name, tint)
	local t = "tf_goodvibes_" .. name .. ".png"
	return tint and (t .. "^[multiply:" .. tint) or t
end

-- ---- blocks -------------------------------------------------------------

local function log(id, label, side, top)
	core.register_node("tf_world:" .. id, {
		description = label,
		tiles = { tex(top), tex(top), tex(side) },
		paramtype2 = "facedir",
		on_place = core.rotate_node,
		groups = { choppy = 2, tree = 1 },
		-- ~40% wood by volume, the rest air in its pores; oak's log is the form wood takes
		_matter = { matter = "wood", share = 0.4, form = id == "log" },
	})
end

local function leaves(id, label, texture, tint)
	core.register_node("tf_world:" .. id, {
		description = label,
		drawtype = "allfaces_optional",
		tiles = { tex(texture, tint) },
		use_texture_alpha = "clip",
		paramtype = "light",
		waving = 2,
		groups = { snappy = 3, leaves = 1 },
		_matter = { matter = "wood", share = 0.05 }, -- a loose tangle: tinder
	})
end

-- The pack draws some foliage grey, for tinting: each gets its own green.
log("log", "Oak log", "oak_log", "oak_log_top")
leaves("leaves", "Oak leaves", "oak_leaves", "#6aa84f")
log("birch_log", "Birch log", "birch_log", "birch_log_top")
leaves("birch_leaves", "Birch leaves", "birch_leaves")
log("spruce_log", "Spruce log", "spruce_log", "spruce_log_top")
leaves("needles", "Spruce needles", "spruce_leaves")
log("jungle_log", "Jungle log", "jungle_log", "jungle_log_top")
leaves("jungle_leaves", "Jungle leaves", "jungle_leaves", "#3f9a2e")
log("acacia_log", "Acacia log", "acacia_log", "acacia_log_top")
leaves("acacia_leaves", "Acacia leaves", "acacia_leaves", "#a2ab4a")
log("palm_log", "Palm log", "palm_log", "palm_log_top")
leaves("palm_leaves", "Palm fronds", "palm_leaves", "#7fae4a")
log("willow_log", "Willow log", "willow_log", "willow_log_top")
leaves("willow_leaves", "Willow leaves", "willow_leaves", "#6a9a4a")
-- the seasonal climates' trees, on the pack's foliage in their own greens
leaves("teak_leaves", "Teak leaves", "oak_leaves", "#86a03c")
leaves("olive_leaves", "Olive leaves", "acacia_leaves", "#9aa888")
leaves("cypress_leaves", "Cypress leaves", "spruce_leaves", "#6f8f62")

core.register_node("tf_world:cactus", {
	description = "Cactus",
	tiles = { tex("cactus_top"), tex("cactus_top"), tex("cactus_side") },
	drawtype = "nodebox",
	paramtype = "light",
	node_box = { type = "fixed", fixed = { -7 / 16, -0.5, -7 / 16, 7 / 16, 0.5, 7 / 16 } },
	damage_per_second = 1,
	groups = { choppy = 3 },
})

-- small plants: drawn as crossed quads, walked through, replaced by building
local function plant(id, label, texture, tint, extra)
	local def = {
		description = label,
		drawtype = "plantlike",
		tiles = { tex(texture, tint) },
		inventory_image = tex(texture, tint),
		wield_image = tex(texture, tint),
		paramtype = "light",
		sunlight_propagates = true,
		walkable = false,
		buildable_to = true,
		waving = 1,
		selection_box = { type = "fixed", fixed = { -0.3, -0.5, -0.3, 0.3, 0.3, 0.3 } },
		groups = { snappy = 3, plant = 1, attached_node = 1 },
	}
	for k, v in pairs(extra or {}) do def[k] = v end
	core.register_node("tf_world:" .. id, def)
end

plant("grass_tuft", "Grass", "grass", "#7ab84f")
plant("fern", "Fern", "fern", "#6a9a50")
plant("dead_bush", "Dead bush", "dead_bush")
plant("dandelion", "Dandelion", "dandelion")
plant("poppy", "Poppy", "poppy")
plant("orange_tulip", "Orange tulip", "orange_tulip")
plant("white_tulip", "White tulip", "white_tulip")
plant("lavender", "Lavender", "lavender")
plant("brown_mushroom", "Brown mushroom", "brown_mushroom", nil, { light_source = 1 })
plant("red_mushroom", "Red mushroom", "red_mushroom")
plant("reed", "Reeds", "reed")
plant("cattail", "Cattail", "cattail")
plant("desert_grass", "Desert grass", "desert_grass")
plant("dune_grass", "Dune grass", "dune_grass")
plant("berry_bush", "Berry bush", "berry_bush")

-- ---- shapes (L-systems: F trunk, f leaves, A/B rules, +/- turn, &/^ pitch)

local SHAPES = {
	oak = { axiom = "FFFFFAFFBF", rules_a = "[&&&FFFFF&&FFFF][&&&++++FFFFF&&FFFF][&&&----FFFFF&&FFFF]",
		rules_b = "[&&&++FFFF&&FFFF][&&&--FFFF&&FFFF][&&&------FFFF&&FFFF]", angle = 30, iterations = 2,
		random_level = 0, trunk_type = "single", thin_branches = true },
	birch = { axiom = "FFFFFFFA", rules_a = "[&&&ffff][++&&&ffff][----&&&ffff][++++++&&&fff]ff", angle = 60, iterations = 2,
		random_level = 1, trunk_type = "single", thin_branches = true },
	spruce = { axiom = "FFFFA", rules_a = "[&&fff][++&&fff][++++&&fff][++++++&&fff]FFA", angle = 45, iterations = 5,
		random_level = 2, trunk_type = "single", thin_branches = true },
	jungle = { axiom = "FFFFFFFFFFAFFB", rules_a = "[&&&FFFFFF&&ffff][&&&++++FFFFFF&&ffff][&&&----FFFFFF&&ffff]",
		rules_b = "[&&ffff][++&&ffff][----&&ffff]f", angle = 30, iterations = 2, random_level = 1,
		trunk_type = "crossed", thin_branches = true },
	acacia = { axiom = "FFFF[&&&FFFA][&&&++++FFFA][&&&----FFFA]", rules_a = "[&fff][+&fff][-&fff][++&fff]",
		angle = 30, iterations = 1, random_level = 1, trunk_type = "single", thin_branches = true },
	palm = { axiom = "FFFFFFFA", rules_a = "[&&&fff][++&&&fff][----&&&fff][++++++&&&fff][--&&&fff]", angle = 60,
		iterations = 1, random_level = 0, trunk_type = "single", thin_branches = true },
	teak = { axiom = "FFFFFFFAFB", rules_a = "[&&&FFFF&&ffff][&&&++++FFFF&&ffff][&&&----FFFF&&ffff]",
		rules_b = "[&&fff][++&&fff][----&&fff]", angle = 30, iterations = 2, random_level = 1,
		trunk_type = "single", thin_branches = true },
	olive = { axiom = "FFF[&&FFA][&&++++FFA][&&----FFA]", rules_a = "[&ff][+&ff][-&ff][++&ff]f",
		angle = 35, iterations = 1, random_level = 1, trunk_type = "single", thin_branches = true },
	cypress = { axiom = "FFAFAFAFAF", rules_a = "[&f][++&f][----&f][++++++&f]", angle = 60, iterations = 1,
		random_level = 0, trunk_type = "single", thin_branches = true },
	willow = { axiom = "FFFFFA", rules_a = "[&&&FFFF&&&ffff][++&&&FFFF&&&ffff][----&&&FFFF&&&ffff][++++++&&&FFFF&&&ffff]",
		angle = 45, iterations = 1, random_level = 1, trunk_type = "single", thin_branches = true },
}

-- ---- species: niche → shape ---------------------------------------------
-- heat and wet are the column's climate (−1..1; heat falls with height);
-- season is when its rain falls (1 all in summer .. −1 all in winter), for
-- the trees of monsoon and Mediterranean lands.
-- `bank` means it grows only beside rivers and lakes, `shore` only at the
-- coast or at an oasis (low sand, or a bank); `on` the soils it takes.

local SOIL = { ["tf_world:grass"] = true, ["tf_world:dirt"] = true, ["tf_world:dry_grass"] = true, ["tf_world:tundra"] = true }
local SAND = { ["tf_world:sand"] = true }

local SPECIES = {
	{ name = "spruce", heat = { -0.45, -0.05 }, wet = { -0.4, 1 }, trunk = "tf_world:spruce_log", leaves = "tf_world:needles", on = SOIL },
	{ name = "birch", heat = { -0.25, 0.15 }, wet = { -0.1, 1 }, trunk = "tf_world:birch_log", leaves = "tf_world:birch_leaves", on = SOIL },
	{ name = "oak", heat = { -0.05, 0.45 }, wet = { -0.25, 1 }, trunk = "tf_world:log", leaves = "tf_world:leaves", on = SOIL },
	{ name = "willow", heat = { -0.1, 0.45 }, wet = { -1, 1 }, bank = true, trunk = "tf_world:willow_log", leaves = "tf_world:willow_leaves", on = SOIL },
	{ name = "jungle", heat = { 0.35, 1 }, wet = { 0.15, 1 }, trunk = "tf_world:jungle_log", leaves = "tf_world:jungle_leaves", on = SOIL },
	{ name = "acacia", heat = { 0.25, 1 }, wet = { -0.45, 0.2 }, trunk = "tf_world:acacia_log", leaves = "tf_world:acacia_leaves", on = SOIL },
	{ name = "palm", heat = { 0.3, 1 }, wet = { -1, 1 }, shore = true, trunk = "tf_world:palm_log", leaves = "tf_world:palm_leaves", on = SAND },
	{ name = "teak", heat = { 0.2, 1 }, wet = { 0, 1 }, season = { 0.45, 1 }, trunk = "tf_world:jungle_log", leaves = "tf_world:teak_leaves", on = SOIL },
	{ name = "olive", heat = { 0, 0.4 }, wet = { -0.7, 0.4 }, season = { -1, -0.25 }, trunk = "tf_world:log", leaves = "tf_world:olive_leaves", on = SOIL },
	{ name = "cypress", heat = { 0, 0.4 }, wet = { -0.4, 0.4 }, season = { -1, -0.25 }, trunk = "tf_world:spruce_log", leaves = "tf_world:cypress_leaves", on = SOIL },
}

-- The engine chooses among them (core.terra.species: of the species that
-- fit, one by the column's roll; banks favour willows), so the far view
-- grows the same trees the world does.
for _, s in ipairs(SPECIES) do
	core.terra.register_species({
		name = s.name, heat = s.heat, wet = s.wet, season = s.season, bank = s.bank, shore = s.shore,
		on = s.on, trunk = s.trunk, leaves = s.leaves, shape = SHAPES[s.name],
	})
end

-- the coast: sand barely above the sea (terragen elevation; 0.03 ≈ 5 blocks)
local function shore(c, i)
	return c.elevation[i] < 0.03 or c.bank[i]
end

-- what covers the ground where no tree stands
local FLOWERS = { "tf_world:dandelion", "tf_world:poppy", "tf_world:orange_tulip", "tf_world:white_tulip" }
local function ground_plant(c, i, ground)
	if not c.alive[i] then return nil end -- (a dead world's sand is only sand)
	local d, h, w, r = c.density[i], c.heat[i], c.wet[i], c.roll[i] -- r: 0 .. 2^32 − 1
	local pick = math.floor(r / 256) % 1000 / 1000
	if SAND[ground] then
		if shore(c, i) then -- beaches, and the water's edge
			if pick < 0.05 then return c.bank[i] and "tf_world:reed" or "tf_world:dune_grass" end
		elseif h > 0.3 then -- desert: a cactus here and there, scrub, a few tufts
			if pick < 0.004 then return "cactus" end
			if pick < 0.02 then return "tf_world:dead_bush" end
			if pick < 0.03 then return "tf_world:desert_grass" end
		elseif pick < 0.01 then
			return "tf_world:dune_grass"
		end
		return nil
	end
	if not SOIL[ground] then return nil end
	if c.bank[i] and pick < 0.06 then
		return (r % 2 == 0) and "tf_world:reed" or "tf_world:cattail"
	end
	if d > 0.7 then -- under the canopy
		if pick < 0.02 then return (r % 3 == 0) and "tf_world:red_mushroom" or "tf_world:brown_mushroom" end
		if pick < 0.25 then return "tf_world:fern" end
		return nil
	end
	if d > 0.08 then -- meadow and open woodland
		if pick < d * 0.6 then
			if math.floor(r / 1048576) % 12 == 0 and h > -0.35 then -- no flowers at the treeline
				if h > 0.2 and w < 0 then return "tf_world:lavender" end
				return FLOWERS[math.floor(r / 16) % #FLOWERS + 1]
			end
			if math.floor(r / 1048576) % 40 == 1 and h > -0.2 and h < 0.4 then return "tf_world:berry_bush" end
			return "tf_world:grass_tuft"
		end
		return nil
	end
	if w < -0.3 and pick < 0.015 then return "tf_world:dead_bush" end
	return nil
end

-- ---- planting, as each chunk generates ------------------------------------

if not core.terra.enabled() then
	return -- other worlds (the Lab) have no terragen to ask
end

core.register_on_generated(function()
	local c = core.get_mapgen_object("terra_cover")
	local vm
	-- keeping the light the generator gave the open node
	local function put(p, name)
		local was = vm:get_node_at(p)
		if was.name == "air" then
			vm:set_node_at(p, { name = name, param1 = was.param1 })
			return true
		end
	end
	local any = false
	for i = 1, #c.y do
		local plant = ground_plant(c, i, core.get_name_from_content_id(c.ground[i]))
		if plant then
			vm = vm or core.get_mapgen_object("voxelmanip")
			any = true
			if plant == "cactus" then
				for k = 1, 1 + c.roll[i] % 3 do
					put({ x = c.x[i], y = c.y[i] + k, z = c.z[i] }, "tf_world:cactus")
				end
			else
				put({ x = c.x[i], y = c.y[i] + 1, z = c.z[i] }, plant)
			end
		end
	end
	if any then
		vm:write_to_map(false)
	end
end)
