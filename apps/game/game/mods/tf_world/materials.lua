-- Building materials (sprint 74): what machines and buildings are made
-- of, as against the ground's dirt and stone. Each is a substance (its
-- weight is its matter's density by the share of the block it fills: sawn
-- timber is wood with air in its grain) and looks like what it is. Plain
-- for now: in survival they'll be worked from raw stock (a log sawn, an
-- ingot rolled), not found. `_machine.friction`: how it grips (a tyre's
-- tread is its rim's), where it isn't most things' 0.6.
local function look(base, tint)
	return base .. "^[multiply:" .. tint
end
local metal = "tf_matter_substance.png"

local MATERIALS = {
	-- flint breaks clean (tf_tools' knapping): a hard, brittle silica
	{ id = "flint", tension = 5e7, compression = 5e8, toughness = 50, name = "Flint", matter = "silica", share = 1, groups = { cracky = 2 },
		tiles = { look(metal, "#4a4640") } },
	{ id = "timber", tension = 4e7, compression = 3e7, toughness = 1e4, name = "Timber", matter = "wood", share = 0.4, groups = { choppy = 2 },
		tiles = { "tf_goodvibes_oak_log.png^[transformR90^[colorize:#c49a62:90" } },
	{ id = "aluminium", tension = 2e8, compression = 2e8, toughness = 1e5, name = "Aluminium", matter = "aluminium", share = 1, groups = { cracky = 2 },
		tiles = { look(metal, "#d4d8de") }, friction = 0.5 },
	{ id = "steel", tension = 4e8, compression = 4e8, toughness = 2e5, name = "Steel", matter = "steel", share = 1, groups = { cracky = 1 },
		tiles = { look(metal, "#7f8a96") }, friction = 0.5 },
	{ id = "rubber", tension = 1.5e7, compression = 1.5e7, toughness = 1e4, name = "Rubber (a tyre's tread: it grips)", matter = "rubber", share = 1, groups = { choppy = 3 },
		tiles = { look(metal, "#2a2b2e") }, friction = 1.0 },
	{ id = "canvas", tension = 3e7, compression = 1e5, toughness = 5e3, name = "Canvas", matter = "wood", share = 0.33, groups = { snappy = 3 },
		tiles = { "tf_goodvibes_sandstone_top.png^[colorize:#ece4cf:200" } },
	{ id = "glass", tension = 3e7, compression = 5e8, toughness = 10, name = "Glass", matter = "glass", share = 1, groups = { cracky = 3 },
		tiles = { look(metal, "#cfeef4") }, drawtype = "glasslike", alpha = "blend" },
}

for _, m in ipairs(MATERIALS) do
	core.register_node("tf_world:" .. m.id, {
		description = m.name,
		tiles = m.tiles,
		drawtype = m.drawtype,
		use_texture_alpha = m.alpha,
		paramtype = m.drawtype and "light" or nil,
		sunlight_propagates = m.drawtype and true or nil,
		groups = m.groups,
		_matter = { matter = m.matter, share = m.share },
		_machine = { friction = m.friction, tension = m.tension, compression = m.compression, toughness = m.toughness },
	})
end
