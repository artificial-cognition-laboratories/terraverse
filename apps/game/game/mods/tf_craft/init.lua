-- Making (AGENTS.md: Making and moving). A hand-sized thing is a form of a
-- material: one item for each the data allows, named tf_craft:<form>_<material>,
-- derived, never listed. Processes make forms at a station or in the hand; the
-- page shows what's offered (set_ui "making") and asks for one (making.make),
-- and it's made after its seconds, checked again then.
local dir = core.get_modpath("tf_craft")
local M = dofile(dir .. "/materials.lua")
local P = dofile(dir .. "/processes.lua")
local FORMS = dofile(dir .. "/forms.lua")
local BY_ID = {}
for _, form in ipairs(FORMS) do BY_ID[form.id] = form end
local offers_of = assert(loadfile(dir .. "/offers.lua"))(M, P, FORMS, BY_ID)
dofile(dir .. "/carry.lua")

local REACH = 5 -- nodes from a station to work at it
local worktop, shown_at -- (below: a station's callbacks use them)

-- A held form's tool capabilities: its group times, quicker and lasting
-- longer in a material with a better edge.
local function tool_of(form, material)
	local groupcaps = {}
	for group, times in pairs(form.digs) do
		local t = {}
		for level, s in ipairs(times) do t[level] = s / material.edge end
		groupcaps[group] = { times = t, uses = math.floor(form.uses * material.edge), maxlevel = 3 }
	end
	return { full_punch_interval = 0.9, max_drop_level = 0, groupcaps = groupcaps,
		damage_groups = { fleshy = math.floor((form.hits or 2) * material.edge + 0.5) } }
end

-- What each form comes in: found ones as listed, made ones in every material
-- of a way's first input that its process can work (`way_of`: the way, by material).
local ITEMS = {}
-- kg each: found by size, made by what went in (mass is conserved; a form's
-- inputs come before it)
local MASS = {}
local function mass_of(form, m)
	if not form.made then return form.litres / 1000 * M[m].density end
	local way = form.way_of[m]
	local total = 0
	for _, input in ipairs(way.from) do
		-- in this material if it comes in it, else its own (an axe's handle is wood)
		local of = BY_ID[input[1]].materials
		local name = "tf_craft:" .. input[1] .. "_" .. (table.indexof(of, m) > 0 and m or of[1])
		total = total + assert(MASS[name], "tf_craft: " .. name .. " isn't weighed before " .. form.id) * input[2]
	end
	return total / (way.count or 1)
end
for _, form in ipairs(FORMS) do
	if form.made then
		form.materials, form.way_of = {}, {}
		for _, way in ipairs(form.made) do
			local process = P[way.by]
			if not process then error("tf_craft: " .. form.id .. " is made by an unknown process") end
			local first = BY_ID[way.from[1][1]]
			if not first or not first.materials then error("tf_craft: " .. form.id .. " is made from what isn't a form yet") end
			for _, m in ipairs(first.materials) do
				local works = not form.way_of[m]
				for prop in pairs(process.needs or {}) do works = works and M[m][prop] end
				if works then
					form.materials[#form.materials + 1] = m
					form.way_of[m] = way
				end
			end
		end
	end
	for _, m in ipairs(form.materials) do
		if not M[m] then error("tf_craft: " .. form.id .. " in unknown material " .. m) end
		local name = "tf_craft:" .. form.id .. "_" .. m
		local label = (form.name:gsub("%%s", M[m].name))
		ITEMS[name] = { form = form.id, material = m, label = label }
		MASS[name] = mass_of(form, m)
		local icon = "tf_craft_" .. form.id .. ".png^[multiply:" .. M[m].tint
		if form.station then
			core.register_node(name, {
				description = label,
				tiles = { "tf_goodvibes_oak_log_top.png", "tf_goodvibes_oak_log.png" },
				drawtype = "nodebox",
				paramtype = "light",
				node_box = { type = "fixed", fixed = {
					{ -0.5, 0.25, -0.5, 0.5, 0.5, 0.5 }, -- its top
					{ -0.45, -0.5, -0.45, -0.3, 0.25, -0.3 }, { 0.3, -0.5, -0.45, 0.45, 0.25, -0.3 },
					{ -0.45, -0.5, 0.3, -0.3, 0.25, 0.45 }, { 0.3, -0.5, 0.3, 0.45, 0.25, 0.45 },
				} },
				groups = { choppy = 3, station = 1 },
				stack_max = form.stack,
				_matter = { matter = M[m].matter, share = 0.3 },
				_station = form.station,
				_mass = MASS[name],
				on_construct = function(pos) worktop(pos) end,
				-- what's on it is in the way of taking it up
				can_dig = function(pos) return worktop(pos):is_empty("worktop") end,
				on_metadata_inventory_put = function(pos) shown_at(pos) end,
				on_metadata_inventory_take = function(pos) shown_at(pos) end,
				on_metadata_inventory_move = function(pos) shown_at(pos) end,
			})
		elseif form.lies then
			core.register_node(name, {
				description = label,
				drawtype = "nodebox",
				tiles = { M[m].tile },
				inventory_image = icon,
				wield_image = icon,
				node_box = { type = "fixed", fixed = form.lies },
				paramtype = "light",
				paramtype2 = "facedir",
				sunlight_propagates = true,
				walkable = false,
				groups = { dig_immediate = 3, attached_node = 1, lying = 1 },
				stack_max = form.stack,
				_matter = { matter = M[m].matter, share = 0.02 },
				_mass = MASS[name],
			})
		elseif form.digs then -- a tool: it wears
			core.register_tool(name, { description = label, inventory_image = icon, wield_image = icon,
				tool_capabilities = tool_of(form, M[m]), _mass = MASS[name] })
		else
			core.register_craftitem(name, { description = label, inventory_image = icon, wield_image = icon,
				stack_max = form.stack, _mass = MASS[name] })
		end
	end
end

tf_craft = {}
-- What's offered with these stacks in reach, at a station's definition
-- (`_station`) or in the hand (nil); and the stacks each make takes, by id.
function tf_craft.offers(stacks, station) return offers_of(stacks, ITEMS, station) end
-- Every stack in these sources ({ inv, list } each, in the order they're
-- drawn on): a station's worktop, then the maker's pockets.
local function stacks_of(sources)
	local all = {}
	for _, src in ipairs(sources) do
		for _, stack in ipairs(src[1]:get_list(src[2])) do all[#all + 1] = stack end
	end
	return all
end
-- Makes an offer now from what's in the sources, if it can be made, into
-- `into` ({ inv, list }): true, or false and why not; and what didn't fit.
function tf_craft.make(sources, station, id, into)
	local offers, takes = offers_of(stacks_of(sources), ITEMS, station)
	for _, o in ipairs(offers) do
		if o.id == id then
			if not o.ok then return false, o.reason end
			for _, t in ipairs(takes[id]) do
				local need = t.count
				for _, src in ipairs(sources) do
					if need > 0 then need = need - src[1]:remove_item(src[2], t.name .. " " .. need):get_count() end
				end
			end
			return true, nil, into[1]:add_item(into[2], o.item .. " " .. o.count)
		end
	end
	return false, "nothing to make that from here"
end

-- Who's making, where, and what's under way: { pos, station, job }.
local at = {}

local function station_at(pos)
	return pos and core.registered_nodes[core.get_node(pos).name]._station or nil
end

-- What's on a station's worktop: the page's slots, as it shows the inventory
local WORKTOP = { width = 3, size = 9 }
function worktop(pos)
	local inv = core.get_meta(pos):get_inventory()
	if inv:get_size("worktop") ~= WORKTOP.size then inv:set_size("worktop", WORKTOP.size) end -- (benches from before)
	return inv
end

-- What a player making draws on: the worktop, if at a station, then their pockets
local function sources(player, here)
	local own = { player:get_inventory(), "main" }
	if here.pos then return { { worktop(here.pos), "worktop" }, own } end
	return { own }
end

local function show(player)
	local here = at[player:get_player_name()]
	if not here then
		player:set_ui("making", nil)
		return
	end
	local offers = offers_of(stacks_of(sources(player, here)), ITEMS, here.station)
	for _, o in ipairs(offers) do o.form = nil end
	local top
	if here.pos then
		top = { width = WORKTOP.width, stacks = {} }
		for i, stack in ipairs(worktop(here.pos):get_list("worktop")) do
			-- (a list, never a gap: an empty Lua table would be sent as null)
			top.stacks[i] = stack:is_empty() and false or { item = stack:get_name(), count = stack:get_count(), wear = stack:get_wear() }
		end
	end
	player:set_ui("making", {
		station = here.station and here.station.name or nil,
		at = here.pos,
		worktop = top,
		offers = offers,
		working = here.job and { id = here.job.id, seconds = here.job.seconds } or nil,
	})
end

-- everyone working at this station sees it change
function shown_at(pos)
	for who, here in pairs(at) do
		if here.pos and vector.equals(here.pos, pos) then show(core.get_player_by_name(who)) end
	end
end

local function open(player, pos)
	at[player:get_player_name()] = { pos = pos, station = station_at(pos) }
	show(player)
end

core.register_on_ui_event(function(player, name, payload)
	local who = player:get_player_name()
	if name == "making.use" then
		local pos = vector.new(payload.x, payload.y, payload.z)
		if not station_at(pos) or vector.distance(pos, player:get_pos()) > REACH then return end
		open(player, pos)
	elseif name == "making.hand" then
		open(player, nil)
	elseif name == "making.leave" then
		at[who] = nil
		show(player)
	elseif name == "making.make" then
		local here = at[who]
		if not here or here.job or type(payload.offer) ~= "string" then return end
		local offers = offers_of(stacks_of(sources(player, here)), ITEMS, here.station)
		local chosen
		for _, o in ipairs(offers) do
			if o.id == payload.offer and o.ok then chosen = o end
		end
		if not chosen then return show(player) end -- changed since it was shown
		local job = { id = chosen.id, seconds = chosen.seconds }
		here.job = job
		show(player)
		core.after(chosen.seconds, function()
			local now = at[who]
			if not now or now.job ~= job then return end -- left, or another since
			now.job = nil
			local p = core.get_player_by_name(who)
			if now.pos and not station_at(now.pos) then -- the bench went while they worked
				at[who] = nil
				return show(p)
			end
			local made, why, left = tf_craft.make(sources(p, now), now.station, job.id, { p:get_inventory(), "main" })
			if not made then
				core.chat_send_player(who, why)
			elseif not left:is_empty() then
				core.add_item(p:get_pos(), left) -- no room: it's put down
			end
			if now.pos then shown_at(now.pos) else show(p) end
		end)
	end
end)

core.register_on_player_inventory_action(function(player)
	if at[player:get_player_name()] then show(player) end
end)
core.register_on_leaveplayer(function(player) at[player:get_player_name()] = nil end)

-- Where found things come from, for now: sticks from leaves, fibre from small
-- plants, stones and flint from gravel. TEMP(2026-11-06): until breaking is
-- tool × material → output.
local function also(name, extra)
	local def = core.registered_nodes[name]
	if type(def.drop) == "table" then error("tf_craft: " .. name .. " already drops a table") end
	local items = { { items = { def.drop or name } } }
	for _, e in ipairs(extra) do items[#items + 1] = e end
	core.override_item(name, { drop = { max_items = #items, items = items } })
end
for name, def in pairs(core.registered_nodes) do
	local groups = def.groups or {}
	if groups.leaves then
		also(name, { { items = { "tf_craft:stick_wood" }, rarity = 2 } })
	elseif groups.plant then
		also(name, { { items = { "tf_craft:fibre_fibre 2" }, rarity = 2 } })
	end
end
also("tf_world:gravel", { { items = { "tf_craft:stone_stone" }, rarity = 2 }, { items = { "tf_craft:stone_flint" }, rarity = 4 } })

-- Lying about where land's generated: on open ground, not on leaves, in water or out on the ice.
-- tries a chunk (80 × 80): a few land in water or on leaves
local LYING = { { "tf_craft:stone_stone", 24 }, { "tf_craft:stick_wood", 16 }, { "tf_craft:stone_flint", 4 } }
local GROUND = {}
core.register_on_mods_loaded(function()
	for name, def in pairs(core.registered_nodes) do
		local g = def.groups or {}
		if def.walkable ~= false and def.drawtype == "normal" and not g.leaves and not g.liquid and not g.slippery then
			GROUND[core.get_content_id(name)] = true
		end
	end
end)
core.register_on_generated(function(minp, maxp, blockseed)
	-- (node by node down the few columns it tries: the whole chunk read into
	-- tables was most of what this cost, for some forty columns)
	local vm = core.get_mapgen_object("voxelmanip")
	local random = PcgRandom(blockseed)
	local placed = false
	for _, lying in ipairs(LYING) do
		for _ = 1, lying[2] do
			local x, z = random:next(minp.x, maxp.x), random:next(minp.z, maxp.z)
			for y = maxp.y - 1, minp.y, -1 do
				local here = vm:get_node_at({ x = x, y = y, z = z })
				if here.name ~= "air" then
					local above = { x = x, y = y + 1, z = z }
					if GROUND[core.get_content_id(here.name)] and vm:get_node_at(above).name == "air" then
						vm:set_node_at(above, { name = lying[1], param2 = random:next(0, 3) })
						placed = true
					end
					break
				end
			end
		end
	end
	if placed then
		vm:write_to_map()
	end
end)

-- what was tf_tools' (sprint 130), in worlds from before
for old, new in pairs({ stone = "stone_stone", stone_edged = "flake_flint", flint = "stone_flint",
		flint_edged = "flake_flint", stick = "stick_wood", thing = "stone_stone" }) do
	core.register_alias("tf_tools:" .. old, "tf_craft:" .. new)
end

dofile(dir .. "/lab.lua")
