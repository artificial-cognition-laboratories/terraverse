-- Terra Firma: the matter's own blocks (sprint 119: were tf_matter's). The world's
-- matter itself is native (server/matter.cpp): it registers every block that says what
-- it's made of, and runs them. TEMP(2026-11-19): registered here, in Lua, while digging
-- and placing look blocks up in core.registered_nodes; native once they don't.

-- The flame: the light of air that's burning. Never placed: the engine sets and clears
-- it where the world's matter burns (never the form air takes).
core.register_node(":core:flame", {
	description = "Flame",
	drawtype = "airlike",
	paramtype = "light",
	light_source = 12,
	sunlight_propagates = true,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	floodable = true,
	drop = "",
	groups = { not_in_creative_inventory = 1 },
	_matter = { matter = "air" },
})
core.register_alias("tf_matter:flame", "core:flame")

-- A vessel (sprint 79): a crucible holds what's put in it, and its matter does what it
-- would: sodium dropped in water fizzes, sand and soda ash heated make glass. The block
-- is air that keeps what's put into it, so the world's physics runs it like any other.
-- The wiki's chemistry scenes are crucibles; in the world, nothing fills one yet.
core.register_node(":core:crucible", {
	description = "Crucible",
	drawtype = "nodebox",
	paramtype = "light",
	tiles = { "matter_substance.png^[multiply:#b0703c" },
	node_box = { type = "fixed", fixed = {
		{ -0.4, -0.5, -0.4, 0.4, -0.4, 0.4 },  -- floor
		{ -0.4, -0.4, -0.4, 0.4, 0.3, -0.32 }, -- walls
		{ -0.4, -0.4, 0.32, 0.4, 0.3, 0.4 },
		{ -0.4, -0.4, -0.32, -0.32, 0.3, 0.32 },
		{ 0.32, -0.4, -0.32, 0.4, 0.3, 0.32 },
	} },
	groups = { cracky = 3 },
	_matter = { matter = "air", vessel = true },
})
core.register_alias("tf_matter:crucible", "core:crucible")

-- A block of every substance the engine knows, so anything can be held,
-- placed, heated and mixed: its solid, and its liquid where it has one at
-- 1 atm. Each is the form its matter takes in that state, after the world's
-- own blocks (lava still cools into basalt, water still freezes into ice;
-- molten copper that cools becomes a copper block). The catalogue shows each
-- substance as it is at room temperature; the rest exist for the physics.

-- how each looks is the matter table's (sprint 79: substances.json's colour)
-- groups, hidden from the catalogue unless `shown` (a group that's present at all hides it)
local function groups(shown)
	local g = { cracky = 3, creative_matter = 1 }
	if not shown then g.not_in_creative_inventory = 1 end
	return g
end

-- a block's weight, for its description (sprint 80: what's carried weighs something)
local function weight(kg)
	if kg >= 1000 then return string.format("%.2f t", kg / 1000) end
	return string.format("%d kg", math.floor(kg + 0.5))
end

-- a levelled block: param2 says how full it is, in sixty-fourths (sprint 80)
local LEVELLED = { type = "leveled", fixed = { -0.5, -0.5, -0.5, 0.5, 0.5, 0.5 } }

for _, s in ipairs(core.matter.substances()) do
	local tint = s.colour
	local solid_shown = s.state == "solid"
	-- the solid: every substance has one (frozen nitrogen included); a gas or liquid at room temperature hides it
	core.register_node(":core:" .. s.id, {
		description = s.name .. " (" .. s.formula .. ")\n1 m³ · " .. weight(s.density),
		tiles = { "matter_substance.png^[multiply:" .. tint },
		groups = groups(solid_shown),
		_matter = { matter = s.id, share = 1, form = true },
		_matter_generated = true,
	})
	-- its powder: poured, it heaps and slides (sprint 80); levelled by how much is there
	core.register_node(":core:" .. s.id .. "_powder", {
		description = s.name .. " powder",
		drawtype = "nodebox",
		node_box = LEVELLED,
		paramtype = "light",
		paramtype2 = "leveled",
		leveled = 64,
		tiles = { "matter_substance.png^[multiply:" .. tint },
		groups = groups(false),
		drop = "",
		_matter = { matter = s.id, share = 1, form = true, loose = true },
		_matter_generated = true,
	})
	-- the liquid, where it has one at 1 atm: a full block, and a puddle
	-- levelled by how much is there; the world's matter runs both (sprint 80)
	local liquid = s.boils == nil or s.boils > s.melts
	if liquid and s.melts < 3500 then
		local hot = s.melts > 700
		local def = {
			description = "Liquid " .. s.name:lower() .. " (" .. s.formula .. ")\n1 m³ · " .. weight(s.density),
			drawtype = "liquid",
			tiles = { "matter_liquid.png^[multiply:" .. tint },
			use_texture_alpha = "blend",
			paramtype = "light",
			light_source = hot and 10 or 0,
			walkable = false,
			pointable = true,
			buildable_to = false,
			liquid_move_physics = true,
			drowning = 1,
			damage_per_second = hot and 6 or 0,
			post_effect_color = { a = 160, r = 120, g = 120, b = 130 },
			groups = groups(s.state == "liquid"),
			_matter = { matter = s.id, share = 1, state = "liquid", form = true },
			_matter_generated = true,
		}
		core.register_node(":core:" .. s.id .. "_liquid", def)
		local puddle = table.copy(def)
		puddle.description = "Liquid " .. s.name:lower()
		puddle.drawtype = "nodebox"
		puddle.node_box = LEVELLED
		puddle.paramtype2 = "leveled"
		puddle.leveled = 64
		puddle.groups = groups(false)
		puddle.groups.matter_puddle = 1 -- sprint 81: liquid is a fill now; these become one on load
		puddle.drop = ""
		puddle.liquid_move_physics = false -- waded through (Luanti swims by the block, not by how deep it is)
		puddle.drowning = 0
		puddle.post_effect_color = nil
		core.register_node(":core:" .. s.id .. "_puddle", puddle)
	end
	-- worlds saved while these were tf_matter's
	for _, form in ipairs({ "", "_powder", "_liquid", "_puddle" }) do
		core.register_alias("tf_matter:" .. s.id .. form, "core:" .. s.id .. form)
	end
end

