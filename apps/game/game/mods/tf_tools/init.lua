-- Things in the hand (sprint 130). The engine makes a small thing's text and
-- moves it (core.physics, src/workshop/small); here's what a person can do
-- with one. On the ground it's a tf_world:piece; on the belt it's an item that
-- keeps the same text, so it comes back as it went.

local REACH = 3      -- nodes
local MOST_HELD = 25 -- kg: past this it isn't picked up, it's dragged
local STONES_A_CHUNK, STICKS_A_CHUNK = 3, 2

local STRIKE_GAP = 0.25 -- s between blows
local EDGES = { "rough", "fine" }
-- How neatly each breaks (knapping): what's not here doesn't flake at all.
local CLEAN = { ["tf_world:flint"] = 1, ["tf_world:stone"] = 0.35 }

local SIZES = {
	{ kind = "stone", node = "tf_world:stone", half = vector.new(4, 3, 4) },    -- a pebble that fits the fist
	{ kind = "stone", node = "tf_world:flint", half = vector.new(6, 5, 7) },    -- a good knapping stone
	{ kind = "stone", node = "tf_world:stone", half = vector.new(6, 5, 7) },
	{ kind = "stone", node = "tf_world:stone", half = vector.new(12, 9, 11) },  -- two hands
	{ kind = "stick", node = "tf_world:timber", radius = 2, length = 50 },
	{ kind = "stick", node = "tf_world:timber", radius = 3, length = 90 }, -- a handle
}

local function text_of(size, seed)
	if size.kind == "stone" then
		return core.physics.small_stone(size.node, size.half, seed)
	end
	return core.physics.small_stick(size.node, size.radius, size.length, seed)
end

local function put_down(text, pos)
	-- staticdata is the text, which is what the entity carries (tf_world:piece)
	return core.add_entity(pos, "tf_world:piece", text)
end

local function describe(text, kg)
	local edge, node = core.physics.edge(text)
	local words = ("%s, %.2f kg"):format(core.registered_nodes[node].description, kg)
	return EDGES[edge] and (words .. ", Edge: " .. EDGES[edge]) or words
end

-- What it shows on the belt, by what it's made of and whether it has an edge.
-- Its own shape's in its metadata: the icon only says what kind of thing it is.
local KINDS = {
	["tf_world:stone"] = { plain = "stone", edged = "stone_edged" },
	["tf_world:flint"] = { plain = "flint", edged = "flint_edged" },
	["tf_world:timber"] = { plain = "stick", edged = "stick" },
}
local THING = {
	stack_max = 1,
	on_place = function(stack) return stack end, -- it isn't a block: F and Q
	on_drop = function(stack, player)
		local at = vector.add(player:get_pos(), vector.new(0, player:get_properties().eye_height, 0))
		at = vector.add(at, vector.multiply(player:get_look_dir(), 0.7))
		local text = core.decode_base64(stack:get_meta():get_string("piece"))
		if not text then
			error("tf_tools: an item whose thing can't be read")
		end
		put_down(text, at)
		return ItemStack("")
	end,
}
local IS_THING = {}
for _, kinds in pairs(KINDS) do
	for _, kind in pairs(kinds) do
		local name = "tf_tools:" .. kind
		if not IS_THING[name] then
			IS_THING[name] = true
			local def = table.copy(THING)
			def.description = (kind:gsub("_", " "):gsub("^%l", string.upper))
			def.inventory_image = "tf_tools_" .. kind .. ".png"
			core.register_craftitem(name, def)
		end
	end
end
core.register_alias("tf_tools:thing", "tf_tools:stone") -- from before kinds

core.register_on_ui_event(function(player, name, id)
	if name ~= "tf_tools:take" then return end
	local entity = core.luaentities[id]
	if not entity or entity.name ~= "tf_world:piece" then return end -- gone since they pressed it
	local object = entity.object
	local at = object:get_pos()
	if not at then return end -- taken already: its entity lingers till the step's end (F pressed twice)
	if vector.distance(at, player:get_pos()) > REACH + 2 then return end
	local kg = core.physics.mass(object)
	if not kg then return end -- it isn't in the physics world yet
	if kg > MOST_HELD then
		core.chat_send_player(player:get_player_name(), ("Too heavy to pick up (%.0f kg)"):format(kg))
		return
	end
	local text = object:get_properties().piece
	local edge, node = core.physics.edge(text)
	local kinds = KINDS[node]
	if not kinds then
		error("tf_tools: no kind of thing made of " .. node)
	end
	local stack = ItemStack("tf_tools:" .. (edge > 0 and kinds.edged or kinds.plain))
	local meta = stack:get_meta()
	meta:set_string("piece", core.encode_base64(text))
	meta:set_float("kg", kg)
	meta:set_string("description", describe(object:get_properties().piece, kg))
	local left = player:get_inventory():add_item("main", stack)
	if not left:is_empty() then
		core.chat_send_player(player:get_player_name(), "Nowhere to put it")
		return
	end
	object:remove()
	local told = player:get_meta()
	if told:get_int("tf_tools:told") == 0 then
		told:set_int("tf_tools:told", 1)
		core.chat_send_player(player:get_player_name(), "Try striking another stone")
	end
end)

local last_blow = {}
core.register_on_leaveplayer(function(player) last_blow[player:get_player_name()] = nil end)

core.register_on_ui_event(function(player, name, payload)
	if name ~= "tf_tools:strike" then return end
	local who = player:get_player_name()
	local now = core.get_us_time() / 1e6
	if now - (last_blow[who] or 0) < STRIKE_GAP then return end
	local held = player:get_wielded_item()
	local kg = held:get_meta():get_float("kg")
	if not IS_THING[held:get_name()] or kg <= 0 then return end
	local entity = core.luaentities[payload.object]
	if not entity or entity.name ~= "tf_world:piece" then return end
	local object = entity.object
	local at = object:get_pos()
	if not at or vector.distance(at, player:get_pos()) > REACH + 2 then return end
	last_blow[who] = now

	local text = object:get_properties().piece
	local had_edge, node = core.physics.edge(text)
	local clean = CLEAN[node]
	local cell, into = payload.cell, payload["in"]
	local struck = clean and core.physics.knap(text, vector.new(cell[1], cell[2], cell[3]),
		vector.new(into[1], into[2], into[3]), kg, clean, math.random(0, 2147483647))
	if not struck or struck.removed == 0 then
		core.add_particle({ pos = at, expirationtime = 0.2, size = 0.5, node = { name = node } }) -- a thud
		return
	end
	core.add_particlespawner({ amount = 8, time = 0.05, node = { name = node }, minsize = 0.3, maxsize = 0.8,
		pos = { min = vector.subtract(at, 0.05), max = vector.add(at, 0.05) },
		vel = { min = vector.new(-1.5, 0.5, -1.5), max = vector.new(1.5, 2.5, 1.5) },
		acc = vector.new(0, -9.8, 0), exptime = 0.6 })
	if #struck.left == 0 then
		object:remove()
		return
	end
	object:set_properties({ piece = struck.left[1] })
	local turned = object:get_rotation()
	for i = 2, #struck.left do
		put_down(struck.left[i], at):set_rotation(turned)
	end
	local edge = core.physics.edge(struck.left[1])
	if edge ~= had_edge and EDGES[edge] then
		core.chat_send_player(who, "Edge: " .. EDGES[edge])
	end
end)

-- Lying about on what's generated: on open ground, with nothing over it.
local function scatter(minp, maxp, seed, count, pick_from, pick_to)
	local random = PcgRandom(seed)
	local vm, emin, emax = core.get_mapgen_object("voxelmanip")
	local area = VoxelArea(emin, emax)
	local data = vm:get_data()
	local air = core.CONTENT_AIR
	for _ = 1, count do
		local x, z = random:next(minp.x, maxp.x), random:next(minp.z, maxp.z)
		for y = maxp.y, minp.y, -1 do
			local here = data[area:index(x, y, z)]
			if here ~= air then
				local def = core.registered_nodes[core.get_name_from_content_id(here)]
				if def.walkable and y < maxp.y then
					local size = SIZES[random:next(pick_from, pick_to)]
					put_down(text_of(size, random:next(0, 2147483647)), vector.new(x, y + 1, z))
				end
				break
			end
		end
	end
end

core.register_on_generated(function(minp, maxp, blockseed)
	scatter(minp, maxp, blockseed, STONES_A_CHUNK, 1, 4)
	scatter(minp, maxp, blockseed + 1, STICKS_A_CHUNK, 5, 6)
end)
