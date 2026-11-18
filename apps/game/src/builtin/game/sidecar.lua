-- Terra Firma: TypeScript mods' blocks, registered as Lua's own (sprint 119).
--
-- TEMP(2026-11-18): while the tf_* mods are Lua, a block a TypeScript mod declares goes
-- through core.register_node, so their systems see it as one of theirs: tf_matter hands its
-- matter to the world and its weight and strength to machines, the creative catalogue offers
-- it. The engine calls this between the mods' scripts and their on_mods_loaded. Stage 7
-- registers them natively, and this goes with Lua.

local clear -- substance id → true where it's see-through: its block is drawn as glass

local function transparent(matter)
	if not clear then
		clear = {}
		for _, s in ipairs(core.matter.substances()) do
			if s.transparent then clear[s.id] = true end
		end
	end
	return clear[matter] == true
end

function core.register_mod_block(b)
	local glass = transparent(b.matter)
	core.register_node(":" .. b.name, {
		description = b.description,
		tiles = b.tiles,
		drawtype = glass and "glasslike" or nil,
		use_texture_alpha = glass and "blend" or nil,
		paramtype = glass and "light" or nil,
		sunlight_propagates = glass or nil,
		-- TEMP(2026-11-18): every block digs like stone until its matter decides how (items)
		groups = { cracky = 3 },
		_matter = { matter = b.matter, share = b.share },
	})
end
