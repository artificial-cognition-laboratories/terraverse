-- Terra Firma: the world's own blocks, as data (sprint 119: were tf_world's). The
-- generator's palette is the engine's: rocks, soils, water, plants, biomes and tree
-- species live in assets/world/*.json, and this registers them. TEMP(2026-11-19):
-- registered through Lua while digging and placing look blocks up in
-- core.registered_nodes; read natively once they don't, and this goes.

local function read(file)
	local data, err = core.parse_json(core.terra.data(file), nil, true)
	if not data then error("world: " .. file .. " isn't JSON: " .. tostring(err)) end
	return data
end

-- The hand: what digs when nothing's held.
core.register_item(":", read("hand.json").hand)

-- Blocks, each as its definition. `_rotates`: placed facing the way it's put down.
local blocks = read("blocks.json").blocks
local names = {}
for name in pairs(blocks) do names[#names + 1] = name end
table.sort(names) -- (registration order doesn't matter; a fixed one keeps content ids stable)
for _, name in ipairs(names) do
	local def = blocks[name]
	if def._rotates then
		def.on_place = core.rotate_node
		def._rotates = nil
	end
	core.register_node(":" .. name, def)
end

-- Soils (sprint 111): a block for each parent rock and stoniness terragen names, each
-- point of its fine earth (its param2) a composition: the rock's stones, the fine
-- earth's grains and clay, organic matter as wood (it burns, and peat burns best).
do
	local soils = read("soils.json")
	local points = core.terra.soil_points()
	local kinds = core.terra.soil_kinds()
	for _, parent in ipairs(kinds.parents) do
		for _, stony in ipairs(kinds.stony) do
			local p, s = soils.parents[parent], soils.stony[stony.name]
			if not (p and s) then error("world: soils.json has no soil for " .. parent .. " " .. stony.name) end
			local family, holds = {}, {}
			for i, pt in ipairs(points) do
				-- what its pores hold where the ground's wet: its fine earth's water at field
				-- capacity (Saxton and Rawls, simplified), clay and organic matter holding
				-- most, sand least, stones none, of the 45% of it that's pores
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
				family[i] = parts
			end
			local groups = table.copy(soils.groups)
			for g, v in pairs(s.groups) do groups[g] = v end
			local id = "soil_" .. parent .. (stony.name ~= "" and "_" .. stony.name or "")
			local def = {
				description = s.description or (s.prefix .. "soil"):gsub("^%l", string.upper),
				tiles = { "tf_world_soil.png^[multiply:" .. p.tint },
				paramtype2 = "color",
				palette = "tf_world_soil_palette.png",
				groups = groups,
				_family = { share = soils.share, points = family, holds = holds }, -- grains with air (and water) between them
			}
			-- its stones: the rock itself, in pebbles (as they are, not the palette's colour)
			if s.pebbles then
				def.overlay_tiles = { { name = "tf_goodvibes_" .. p.rock .. ".png^[mask:tf_world_pebbles_" .. s.pebbles .. ".png", color = "white" } }
			end
			core.register_node(":core:" .. id, def)
			core.register_alias("mapgen_" .. id, "core:" .. id)
			core.register_alias("tf_world:" .. id, "core:" .. id) -- worlds from before
		end
	end
	-- the column's soil, as biomes name it: the generator puts the kind it is in its place
	core.register_alias("core:soil", "core:soil_granitic")
	core.register_alias("tf_world:soil", "core:soil_granitic")
	core.register_alias("mapgen_soil", "core:soil_granitic")
end

for _, biome in ipairs(read("biomes.json").biomes) do
	core.register_biome(biome)
end

for _, species in ipairs(read("species.json").species) do
	core.terra.register_species(species)
end

for from, to in pairs(read("aliases.json").aliases) do
	core.register_alias(from, to)
end

-- Machines off the world's grid (sprint 49): the engine makes, holds, edits and moves
-- them (src/workshop); this entity only carries one (its `piece` property: the machine's
-- text, kept as its staticdata). Physical, so players stand on it and bump into it.
local piece = {
	initial_properties = {
		visual = "piece",
		physical = true,
		collide_with_objects = true,
		pointable = true,
		static_save = true,
	},
	on_activate = function(self, staticdata)
		if staticdata == "" then
			error("core:piece: made without its machine")
		end
		self.object:set_properties({ piece = staticdata })
		self.object:set_armor_groups({ immortal = 1 })
	end,
	get_staticdata = function(self)
		return self.object:get_properties().piece
	end,
}
core.register_entity(":core:piece", piece)
core.register_entity(":tf_world:piece", table.copy(piece)) -- machines saved before (entities have no aliases)

-- What's broken falls where it was, as items on the ground to be picked up, never
-- straight into the inventory; items lying close merge into stacks (item_entity.lua).
function core.handle_node_drops(pos, drops)
	for _, item in ipairs(drops) do
		local at = vector.offset(pos, math.random() * 0.4 - 0.2, 0.1, math.random() * 0.4 - 0.2)
		local obj = core.add_item(at, item)
		if obj then obj:set_velocity({ x = math.random() - 0.5, y = 1.5, z = math.random() - 0.5 }) end
	end
end
