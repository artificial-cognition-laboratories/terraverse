-- The weather each player stands in. The atmosphere moves on its own in the
-- engine (src/fluid: wind, rain, storms over the whole world); this reads it
-- where each player is for the page (set_ui "weather"). Nothing of it is
-- drawn: the stock clouds and the rain, snow and ash that fell round the
-- player went with the stock sky (sprint 60); the air's drawing comes back
-- with the atmosphere. Worlds terra didn't generate have no atmosphere.

dofile(core.get_modpath("tf_weather") .. "/world.lua") -- snow cover and ice, from the weather

if not core.terra.enabled() then
	return
end

dofile(core.get_modpath("tf_weather") .. "/rivers.lua") -- /river and /rivers

local PERIOD = 2.0 -- seconds between readings

local space = core.terra.world().boundary -- this high above the sea, space: no weather

local function update(player)
	local pos = player:get_pos()
	if pos.y >= space then
		player:set_ui("weather", nil)
		return
	end
	-- (none on a seam, before a player's placed; nor while it settles as the world opens)
	local w = not core.terra.seam(pos) and core.terra.weather(pos)
	if not w then
		player:set_ui("weather", nil)
		return
	end
	-- events go only when there are any (an empty Lua table has no JSON shape)
	player:set_ui("weather", {
		temperature = w.temperature, humidity = w.humidity, precipitation = w.precipitation,
		snow = w.snow, wind = w.wind, pressure = w.pressure,
		events = #w.events > 0 and w.events or nil,
	})
end

core.register_on_joinplayer(update)

local elapsed = 0
core.register_globalstep(function(dtime)
	elapsed = elapsed + dtime
	if elapsed < PERIOD then
		return
	end
	elapsed = 0
	for _, player in ipairs(core.get_connected_players()) do
		update(player)
	end
end)
