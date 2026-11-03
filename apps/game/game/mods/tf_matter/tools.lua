-- Making fire and feeding it. Neither knows anything about fire: the drill
-- heats what it's used on, the bellows pushes air; the world's physics
-- decides what happens.

-- The drill's friction makes an ember hot enough to light tinder, but not
-- enough heat to warm anything with more than a little mass: whether it
-- catches is the block's heat capacity, not a list of burnable blocks.
local EMBER = 750      -- K
local MOST_MASS = 5e4  -- J/K: kindling and leaves are under it; a log is ~20× over

tf_matter = {} -- what other files of this mod share (the probe)

core.register_tool("tf_matter:fire_drill", {
	description = "Fire drill\nSpin it on tinder (leaves, kindling) to make an ember",
	inventory_image = "tf_matter_fire_drill.png",
	on_use = function(stack, user, pointed)
		if pointed.type ~= "node" then return end
		local pos, name = pointed.under, user:get_player_name()
		local capacity = core.matter.capacity(pos)
		if not capacity then
			core.chat_send_player(name, "Nothing there to light.")
		elseif capacity > MOST_MASS then
			core.chat_send_player(name, "Too much to warm with a drill: an ember on tinder beside it might.")
		else
			core.matter.heat(pos, EMBER)
		end
		stack:add_wear(65535 / 40)
		return stack
	end,
})

-- One push: about a litre of the room's air per click is far too little; a
-- real bellows moves tens of litres a stroke. 2 moles ≈ 50 litres.
local PUSH = 2

core.register_node("tf_matter:bellows", {
	description = "Bellows\nPump (right-click) to push air out of the front",
	tiles = { "tf_matter_bellows_side.png", "tf_matter_bellows_side.png", "tf_matter_bellows_side.png",
		"tf_matter_bellows_side.png", "tf_matter_bellows_side.png", "tf_matter_bellows_front.png" },
	paramtype2 = "facedir",
	groups = { choppy = 2 },
	on_place = core.rotate_node,
	on_rightclick = function(pos, node)
		-- facedir's front face points along −dir: the bellows blows out of it
		local front = vector.subtract(pos, core.facedir_to_dir(node.param2))
		core.matter.blow(front, PUSH)
	end,
})

core.register_craft({ output = "tf_matter:fire_drill", recipe = { { "group:tree" }, { "group:leaves" } } })
core.register_craft({ output = "tf_matter:bellows", recipe = { { "group:tree", "group:tree" }, { "group:leaves", "group:leaves" } } })

-- ---- creative lab tools -----------------------------------------------------

-- The heat gun: left-click heats the block you point at by 100 K, right-click
-- (on a block, or in the air pointing at one) chills it by 100 K. It sets the
-- temperature outright, whatever the block's mass: a tool for finding out.
local STEP = 100

local function aim(user, pointed)
	if pointed and pointed.type == "node" then return pointed.under end
	local look = core.raycast(vector.offset(user:get_pos(), 0, user:get_properties().eye_height, 0),
		vector.add(vector.offset(user:get_pos(), 0, user:get_properties().eye_height, 0), vector.multiply(user:get_look_dir(), 8)), false, false)
	for thing in look do
		if thing.type == "node" then return thing.under end
	end
end

local function nudge(user, pointed, by)
	local pos = aim(user, pointed)
	if not pos or not core.matter.capacity(pos) then return end
	core.matter.touch(pos, math.max(20, core.matter.temperature(pos) + by))
	tf_matter.probe(user, pos)
end

core.register_tool("tf_matter:heat_gun", {
	description = "Heat gun\nLeft-click: +100 K · right-click: −100 K",
	inventory_image = "tf_matter_heat_gun.png",
	groups = { creative_functional = 1 },
	on_use = function(stack, user, pointed) nudge(user, pointed, STEP) end,
	on_place = function(stack, user, pointed) nudge(user, pointed, -STEP) end,
	on_secondary_use = function(stack, user, pointed) nudge(user, pointed, -STEP) end,
})

-- The blowtorch (sprint 79): a real flame, not a thermostat. Held on a
-- face, it feeds propane and air (Shift: pure oxygen) into the air just in
-- front of it and lights it; the propane burning there is the heat, as hot
-- as propane burns in what it's given (~2000 °C in air, hotter in oxygen),
-- and what's in front warms by conduction. Big things take long: a cubic
-- metre of sand is two and a half tonnes. A crucible's cupful melts fast.
local TORCH = {
	rate = 0.01,   -- mol/s of propane: about 20 kW, a big torch
	air = { oxygen = 5, nitrogen = 18.8 }, -- per propane, burnt clean
	oxy = { oxygen = 5 },
	light = 900,   -- K: its pilot's spark
	every = 0.1,   -- s
}

local torches = {} -- player name -> seconds since it last fed

local function torch(user, dtime)
	local name = user:get_player_name()
	torches[name] = (torches[name] or 0) + dtime
	if torches[name] < TORCH.every then return end
	local dt = torches[name]
	torches[name] = 0
	local eye = vector.offset(user:get_pos(), 0, user:get_properties().eye_height, 0)
	for thing in core.raycast(eye, vector.add(eye, vector.multiply(user:get_look_dir(), 4)), false, false) do
		if thing.type == "node" then
			-- the flame's in the air before the face, touching it
			local at = thing.above
			if not core.matter.capacity(at) then return end
			local fuel = TORCH.rate * dt
			core.matter.add(at, "propane", fuel)
			for gas, per in pairs(user:get_player_control().aux1 and TORCH.oxy or TORCH.air) do
				core.matter.add(at, gas, fuel * per)
			end
			-- lit once; then its own heat keeps it lit (a pilot every tick
			-- would heat the whole block's air for nothing)
			if core.matter.temperature(at) < TORCH.light then
				core.matter.heat(at, TORCH.light)
			end
			return
		end
	end
end

core.register_tool("tf_matter:blowtorch", {
	description = "Blowtorch\nHold left on a block: a propane flame against it · Shift: with oxygen, hotter",
	inventory_image = "tf_matter_heat_gun.png^[colorize:#2060ff:90",
	groups = { creative_functional = 1 },
	on_use = function() end, -- held, not clicked: the step below feeds it
})

core.register_globalstep(function(dtime)
	for _, user in ipairs(core.get_connected_players()) do
		local control = user:get_player_control()
		if (control.dig or control.LMB) and user:get_wielded_item():get_name() == "tf_matter:blowtorch" then
			torch(user, dtime)
		else
			torches[user:get_player_name()] = nil
		end
	end
end)

-- The probe: what a block is made of, how hot, in what state, and what's
-- happening in it, shown on the HUD (set_ui "probe").
function tf_matter.probe(user, pos)
	local r = core.matter.inspect(pos)
	if not r then
		user:set_ui("probe", { node = core.get_node(pos).name, empty = true })
		return
	end
	local node = core.get_node(pos)
	local def = core.registered_nodes[node.name]
	-- a family's block: the composition it is, by name ("Loam (sand 40% · silt 40% · clay 20%)")
	r.node = def and def._point_name and def._point_name(node.param2) or (def and def.description ~= "" and def.description) or node.name
	user:set_ui("probe", r)
end

core.register_tool("tf_matter:probe", {
	description = "Probe\nPoint and click: what a block is made of, its temperature and state",
	inventory_image = "tf_matter_probe.png",
	groups = { creative_functional = 1 },
	on_use = function(stack, user, pointed)
		local pos = aim(user, pointed)
		if pos then tf_matter.probe(user, pos) end
	end,
})
