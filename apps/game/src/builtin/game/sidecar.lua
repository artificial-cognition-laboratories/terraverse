-- Terra Firma: TypeScript mods' blocks, registered as Lua's own (sprint 119).
--
-- TEMP(2026-11-18): while the tf_* mods are Lua, a block a TypeScript mod declares goes
-- through core.register_node, so their systems see it as one of theirs: the engine hands its
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

-- TypeScript mods' commands, as chat commands: each line typed under one goes to the
-- sidecar's next window, which parses it, checks its permission and answers in chat.
-- The engine calls this with the whole set, whenever it changes. A mod's command may
-- shadow one of builtin's (/msg, /kick): the TypeScript one answers while it's there.
local ours = {}     -- name → true: the chat commands this registered
local shadowed = {} -- name → the definition ours replaced

function core.set_mod_commands(commands)
	local now = {}
	for _, c in ipairs(commands) do now[c.name] = c end
	for name in pairs(ours) do
		if not now[name] then
			core.registered_chatcommands[name] = shadowed[name]
			ours[name], shadowed[name] = nil, nil
		end
	end
	for name, c in pairs(now) do
		if not ours[name] then shadowed[name] = core.registered_chatcommands[name] end
		ours[name] = true
		local command = name
		core.registered_chatcommands[name] = nil
		core.register_chatcommand(name, {
			params = c.params,
			description = c.description,
			func = function(player, param)
				if not core.sidecar_command(player, command, param) then
					return false, "The mods are off, so nothing can answer /" .. command .. "."
				end
				return true -- the mod answers, in the next window
			end,
		})
	end
end
