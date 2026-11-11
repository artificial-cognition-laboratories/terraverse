-- Soil as a composition (sprint 111). Which block a soil is says what its
-- grains came from (granitic, basaltic, limy, shaly rock) and how stony it is;
-- its param2 is its fine earth: a point on the soil texture triangle (sand,
-- silt, clay) with its organic matter, one of the points terragen defines
-- (core.terra.soil_points). terragen chooses all of it for each place and
-- depth (src/terragen/soil.cpp); the kinds come from it too
-- (core.terra.soil_kinds), so the generator and these blocks can't disagree.
-- Its colour comes from its palette, its stones from the rock they are, its
-- name from the texture classes soil scientists use.

local POINTS = core.terra.soil_points()
local KINDS = core.terra.soil_kinds()

-- The USDA soil texture classes, by the shares of sand, silt and clay.
local function texture(sand, silt, clay)
	if clay >= 0.4 then
		return silt >= 0.4 and "silty clay" or sand >= 0.45 and "sandy clay" or "clay"
	elseif clay >= 0.27 then
		return sand > 0.45 and "sandy clay loam" or silt >= 0.4 and "silty clay loam" or "clay loam"
	elseif silt >= 0.8 and clay < 0.12 then
		return "silt"
	elseif silt >= 0.5 then
		return "silt loam"
	elseif sand >= 0.85 then
		return "sand"
	elseif sand >= 0.7 then
		return "loamy sand"
	elseif clay >= 0.2 and sand > 0.45 then
		return "sandy clay loam"
	elseif sand >= 0.52 or (clay < 0.07 and silt < 0.5) then
		return "sandy loam"
	end
	return "loam"
end

-- What each parent leaves: the rock its stones are, the minerals of its sand
-- and silt (what lasted of it), the clay's, how it looks
local PARENTS = {
	granitic = { rock = "granite", grains = { silica = 0.75, feldspar = 0.25 }, clay = { kaolinite = 1 },
		tint = "#ffffff", from = "granitic rock" },
	basaltic = { rock = "basalt", grains = { plagioclase = 0.6, pyroxene = 0.4 }, clay = { kaolinite = 0.9, hematite = 0.1 },
		tint = "#b8afa8", from = "basalt" },
	limy = { rock = "limestone", grains = { calcite = 0.5, silica = 0.5 }, clay = { kaolinite = 1 },
		tint = "#fff4e2", from = "limestone" },
	shaly = { rock = "shale", grains = { silica = 1 }, clay = { kaolinite = 1 },
		tint = "#d8d4d0", from = "shale" },
}
-- How each stony class holds and gives (a gravel slides like gravel)
local STONY = {
	[""] = { prefix = "", groups = { pile_yield = 16, pile_bearing = 60, pile_give = 3 } },
	stony = { prefix = "Stony ", groups = { pile_yield = 12, pile_bearing = 70, pile_give = 3 } },
	gravelly = { prefix = "Gravelly ", groups = { pile_yield = 6, pile_bearing = 80, pile_give = 2 } },
	gravel = { prefix = "", groups = { pile_yield = 0, pile_bearing = 80, pile_give = 2 } },
}

local function label(name)
	return name:sub(1, 1):upper() .. name:sub(2)
end

local function kind(parent, stony)
	local p, s = PARENTS[parent], STONY[stony.name]
	assert(p and s, "tf_world: no soil look for " .. parent .. " " .. stony.name)
	-- each point's matter: the rock's stones, the fine earth's grains and
	-- clay, organic matter as cellulose (it burns, and peat burns best)
	local points, holds = {}, {}
	for i, pt in ipairs(POINTS) do
		-- what its pores hold where the ground's wet (sprint 111): its fine
		-- earth's water at field capacity (Saxton and Rawls, simplified),
		-- clay and organic matter holding most, sand least, stones none,
		-- of the 45% of it that's pores
		local fc = math.min(0.45, 0.08 + 0.40 * pt.clay + 0.20 * pt.silt + 0.5 * pt.organic)
		holds[i] = math.min(1, fc * (1 - stony.stones) / 0.45)
		local mineral = 1 - pt.organic
		local stones, fine = mineral * stony.stones, mineral * (1 - stony.stones)
		local parts = {}
		local function add(id, share)
			if share > 0 then parts[id] = (parts[id] or 0) + share end
		end
		add(p.rock, stones)
		for id, share in pairs(p.grains) do add(id, fine * (pt.sand + pt.silt) * share) end
		for id, share in pairs(p.clay) do add(id, fine * pt.clay * share) end
		add("wood", pt.organic)
		points[i] = parts
	end
	local function named(k)
		local pt = POINTS[k + 1]
		if not pt then return "Soil" end
		local class = texture(pt.sand, pt.silt, pt.clay)
		local peat = pt.organic >= 0.3 and "peaty " or ""
		local what = stony.name == "gravel" and ("gravel in " .. peat .. class) or (s.prefix:lower() .. peat .. class)
		return string.format("%s, from %s (sand %d%% · silt %d%% · clay %d%%)", label(what), p.from,
			math.floor(pt.sand * 100 + 0.5), math.floor(pt.silt * 100 + 0.5), math.floor(pt.clay * 100 + 0.5))
	end
	local groups = { loose = 1, crumbly = 3, soil = 1, pile_repose = 1 }
	for g, v in pairs(s.groups) do groups[g] = v end
	local name = "tf_world:soil_" .. parent .. (stony.name ~= "" and "_" .. stony.name or "")
	local def = {
		description = stony.name == "gravel" and "Gravel" or label(s.prefix .. "soil"),
		tiles = { "tf_world_soil.png^[multiply:" .. p.tint },
		paramtype2 = "color",
		palette = "tf_world_soil_palette.png",
		groups = groups,
		_family = { share = 0.55, points = points, holds = holds }, -- grains with air (and water) between them
		_point_name = named,
	}
	-- its stones: the rock itself, in pebbles (left as they are, not the palette's colour)
	local k = ({ stony = 1, gravelly = 2, gravel = 3 })[stony.name]
	if k then
		def.overlay_tiles = { { name = "tf_goodvibes_" .. p.rock .. ".png^[mask:tf_world_pebbles_" .. k .. ".png", color = "white" } }
	end
	core.register_node(name, def)
	core.register_alias("mapgen_" .. name:sub(10), name)
	return name
end

for _, parent in ipairs(KINDS.parents) do
	for _, stony in ipairs(KINDS.stony) do
		kind(parent, stony)
	end
end
-- the column's soil, as biomes name it: the generator puts the kind it is
-- here in its place (and the soil of worlds before sprint 111)
core.register_alias("tf_world:soil", "tf_world:soil_granitic")
core.register_alias("mapgen_soil", "tf_world:soil_granitic")
