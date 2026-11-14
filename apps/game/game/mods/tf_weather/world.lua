-- The weather acting on the world: snow settles where it falls and melts
-- when it warms; open water freezes over in the cold and thaws again. All
-- from core.terra.weather at the spot, so the same storm that the player
-- sees falling is the one that whitens the ground. Runs on the blocks
-- around players (active block modifiers); `tf_weather.weather_on(pos)` is
-- the one rule, shared with the headless check.

local SNOW, ICE, WATER = "tf_weather:snow", "tf_weather:ice", "tf_world:water_source"

core.register_node(SNOW, {
	description = "Snow cover",
	tiles = { "tf_goodvibes_snow.png" },
	drawtype = "nodebox",
	paramtype = "light",
	node_box = { type = "fixed", fixed = { -0.5, -0.5, -0.5, 0.5, -0.375, 0.5 } },
	buildable_to = true,
	floodable = true,
	groups = { crumbly = 3, falling_node = 1 },
	drop = "",
	_matter = { matter = "water", share = 0.1 }, -- fresh snow: a tenth ice
})

-- weather-made ice: unlike the polar wall's, it thaws
core.register_node(ICE, {
	description = "Lake ice",
	tiles = { "tf_goodvibes_packed_ice.png^[opacity:220" },
	use_texture_alpha = "blend",
	groups = { cracky = 3, slippery = 3 },
	drop = "",
	_matter = { matter = "water", share = 1, form = true }, -- the form water freezes into
})

tf_weather = {}

-- The weather read at most once per 16-block column per period: one storm
-- is wide, and every node an ABM touches would otherwise ask again.
local PERIOD = 10
local cache, cached_at = {}, -PERIOD

local function weather_at(pos)
	local now = core.get_us_time() / 1e6
	if now - cached_at > PERIOD then
		cache, cached_at = {}, now
	end
	local key = math.floor(pos.x / 16) .. ":" .. math.floor(pos.z / 16)
	local w = cache[key]
	if not w then
		w = core.terra.weather(pos)
		cache[key] = w
	end
	return w
end

local FREEZE, THAW, MELT = -3, 1, 1 -- °C

-- What the weather does to the node at `pos` (the top of the ground or of
-- the water, open to the sky above): "snow", "melt", "freeze", "thaw" or nil.
-- `w` is the weather there (core.terra.weather).
function tf_weather.weather_on(pos, name, w)
	if name == SNOW then
		return w.temperature > MELT and "melt" or nil
	elseif name == ICE then
		return w.temperature > THAW and "thaw" or nil
	elseif name == WATER then
		return w.temperature < FREEZE and "freeze" or nil
	end
	local def = core.registered_nodes[name]
	if def and def.walkable and (def.drawtype == "normal" or def.drawtype == "allfaces_optional")
			and w.snow and w.precipitation > 0.05 then
		return "snow"
	end
	return nil
end

local function open_sky(pos)
	local above = vector.offset(pos, 0, 1, 0)
	return core.get_node(above).name == "air" and (core.get_node_light(above, 0.5) or 0) >= 15
end

local ACT = {
	snow = function(pos) core.set_node(vector.offset(pos, 0, 1, 0), { name = SNOW }) end,
	melt = function(pos) core.remove_node(pos) end,
	freeze = function(pos) core.set_node(pos, { name = ICE }) end,
	thaw = function(pos) core.set_node(pos, { name = WATER }) end,
}

function tf_weather.act(pos, node)
	local w = weather_at(pos)
	if not w then return nil end -- (no weather yet: it settles as the world opens)
	local action = tf_weather.weather_on(pos, node.name, w)
	if action then ACT[action](pos) end
	return action
end

if not core.terra.enabled() then
	return -- no atmosphere outside terra worlds
end

core.register_abm({
	label = "snow settles",
	nodenames = { "group:crumbly", "group:cracky", "group:leaves" },
	neighbors = { "air" },
	interval = 10,
	chance = 60,
	catch_up = false,
	action = function(pos, node)
		if node.name ~= SNOW and node.name ~= ICE and open_sky(pos) then tf_weather.act(pos, node) end
	end,
})

core.register_abm({
	label = "snow melts, ice thaws",
	nodenames = { SNOW, ICE },
	interval = 10,
	chance = 20,
	catch_up = false,
	action = function(pos, node) tf_weather.act(pos, node) end,
})

core.register_abm({
	label = "water freezes",
	nodenames = { WATER },
	neighbors = { "air" },
	interval = 10,
	chance = 30,
	catch_up = false,
	action = function(pos, node)
		if open_sky(pos) then tf_weather.act(pos, node) end
	end,
})
