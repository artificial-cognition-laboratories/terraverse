-- What the player may do and use: how they move (below), and whether they
-- have the wiki (at the end).
--
-- How the player moves. Luanti gives singleplayer every privilege except
-- fly, fast and noclip: games grant those, and ours grants them in creative
-- worlds (double-tap jump to fly, as Minecraft). Sprinting is ours in every
-- mode, as Minecraft's: hold Shift (aux1), or double-tap forward; it lasts
-- until you stop moving forward. A run, not Luanti's fast mode.

-- Building in creative: the engine asks for this privilege (the workshop's
-- tools, and how hard a player carries: as hard as they like). Nothing else
-- registers it; build mode grants it with every other privilege.
core.register_privilege("creative", {
	description = "Can build in creative (the workshop's tools, carrying anything)",
	give_to_singleplayer = false,
	give_to_admin = false,
})

local SPRINT = 1.6 -- × walking speed
-- Creative grants every privilege; survival takes back what creative granted,
-- and these whoever granted them (survival never moves this way)
local MOVEMENT = { fly = true, fast = true, noclip = true }
local creative = core.settings:get_bool("creative_mode", false)
local damage = core.settings:get_bool("enable_damage", true)
local modes = {} -- connected player name → build mode
local original_is_creative_enabled = core.is_creative_enabled

function core.is_creative_enabled(name)
	if modes[name] ~= nil then return modes[name] end
	return original_is_creative_enabled(name)
end

-- In creative the hand breaks anything at once (the player's "hand" list
-- overrides the hand item, and the client times digging by it too).
local DIG = { "crumbly", "choppy", "cracky", "snappy", "oddly_breakable_by_hand", "dig_immediate" }
local instant = {}
for _, group in ipairs(DIG) do
	instant[group] = { times = { [1] = 0, [2] = 0, [3] = 0 }, uses = 0, maxlevel = 3 }
end
core.register_item("tf_player:creative_hand", {
	type = "none",
	wield_image = "blank.png",
	groups = { not_in_creative_inventory = 1 },
	tool_capabilities = {
		full_punch_interval = 0.9,
		max_drop_level = 3,
		groupcaps = instant,
		damage_groups = { fleshy = 1 },
	},
})

local function set_hand(player, enabled)
	local inv = player:get_inventory()
	inv:set_size("hand", 1)
	inv:set_stack("hand", 1, enabled and "tf_player:creative_hand" or "")
end

local function set_mode(player, enabled, switched)
	local name = player:get_player_name()
	modes[name] = enabled
	local meta = player:get_meta()
	meta:set_string("tf_build_mode", enabled and "creative" or "survival")
	player:set_ui("creative", enabled)
	local privs = core.get_player_privs(name)
	for priv in pairs(core.registered_privileges) do
		if enabled and not privs[priv] then
			privs[priv] = true
			meta:set_string("tf_granted_" .. priv, "1")
		elseif not enabled and (meta:get_string("tf_granted_" .. priv) == "1" or (switched and MOVEMENT[priv])) then
			privs[priv] = nil
			meta:set_string("tf_granted_" .. priv, "")
		end
	end
	core.set_player_privs(name, privs)
	local before = meta:get_string("tf_survival_armor")
	if enabled then
		if before == "" then meta:set_string("tf_survival_armor", core.serialize(player:get_armor_groups())) end
		local groups = player:get_armor_groups()
		groups.immortal = 1
		player:set_armor_groups(groups)
	elseif before ~= "" then
		if damage then player:set_armor_groups(core.deserialize(before)) end
		meta:set_string("tf_survival_armor", "")
	end
	player:hud_set_flags({ healthbar = damage and not enabled, breathbar = damage and not enabled })
	set_hand(player, enabled)
end

-- Match the player inventory's nine-column layout. Existing worlds may have
-- items in the old 3×3 craft list, so move them before resizing it.
-- What a player looks like (sprint 58): a jointed body the engine poses by
-- how they move (client/figure.h), not a flat picture.
core.register_on_joinplayer(function(player)
	player:set_properties({ visual = "body", textures = {} })
end)

core.register_on_joinplayer(function(player)
	local inv = player:get_inventory()
	local craft = inv:get_list("craft") or {}
	for i, stack in ipairs(craft) do
		if not stack:is_empty() then
			local leftover = inv:add_item("main", stack)
			if not leftover:is_empty() then
				core.add_item(player:get_pos(), leftover)
			end
			inv:set_stack("craft", i, "")
		end
	end
	inv:set_size("craft", 4)
	inv:set_width("craft", 2)
	inv:set_size("main", math.max(inv:get_size("main"), 36))
	inv:set_width("main", 9)
	player:hud_set_hotbar_itemcount(9)
	-- with no damage (creative worlds) health and breath can't change: no bars
	if not damage then
		player:hud_set_flags({ healthbar = false, breathbar = false })
	end
	local saved = player:get_meta():get_string("tf_build_mode")
	set_mode(player, saved == "creative" or (saved == "" and creative))
end)

-- The catalogue is only a view. A server event checks the world's mode and
-- the registered definition before replacing the chosen hotbar slot.
core.register_on_ui_event(function(player, name, payload)
	if name ~= "creative.take" and name ~= "creative.clear" then return end
	if not modes[player:get_player_name()] or type(payload) ~= "table" then return end
	local slot = payload.slot
	local inv = player:get_inventory()
	if type(slot) ~= "number" or slot % 1 ~= 0 or slot < 0 or slot >= inv:get_size("main") then return end
	if name == "creative.clear" then
		inv:set_stack("main", slot + 1, "")
		return
	end
	local item, count = payload.item, payload.count
	if type(item) ~= "string" or type(count) ~= "number" then return end
	if count % 1 ~= 0 or count < 1 then return end
	local def = core.registered_items[item]
	if not def or item == "" or item:match("_flowing$") or
			def.description == "" or (def.groups and def.groups.not_in_creative_inventory) then return end
	local stack = ItemStack(item)
	stack:set_count(math.min(count, stack:get_stack_max()))
	inv:set_stack("main", slot + 1, stack)
end)

local DOUBLE_TAP = 0.3 -- s between two presses of forward that start a sprint

local sprinting = {} -- player name → whether the override is on
local forward = {}   -- player name → { down = was forward held, tapped = when it was last pressed, latched = sprinting from a double tap }

core.register_globalstep(function()
	local now = core.get_us_time() / 1e6
	for _, player in ipairs(core.get_connected_players()) do
		local name = player:get_player_name()
		local control = player:get_player_control()
		local f = forward[name] or { down = false, tapped = -1, latched = false }
		if control.up and not f.down then -- forward pressed now
			f.latched = now - f.tapped < DOUBLE_TAP
			f.tapped = now
		end
		if not control.up then f.latched = false end
		f.down = control.up
		forward[name] = f
		local on = control.up and (control.aux1 or f.latched)
		if on ~= (sprinting[name] or false) then
			player:set_physics_override({ speed = on and SPRINT or 1 })
			sprinting[name] = on
		end
	end
end)

core.register_on_leaveplayer(function(player)
	sprinting[player:get_player_name()] = nil
	forward[player:get_player_name()] = nil
	modes[player:get_player_name()] = nil
end)

dofile(core.get_modpath("tf_player") .. "/widgets.lua")(modes)
dofile(core.get_modpath("tf_player") .. "/controls.lua")(modes, set_mode, damage)

-- The wiki (key G): offered where the world's rules allow it (tf_wiki in
-- world.mt, chosen on the create screen's Rules). Worlds made before the
-- rule existed have no tf_wiki and keep it, as Classic does.
local rules = Settings(core.get_worldpath() .. "/world.mt")
local wiki = rules:get_bool("tf_wiki", true)

core.register_on_joinplayer(function(player)
	if wiki then
		player:set_ui("wiki", true)
	end
end)
