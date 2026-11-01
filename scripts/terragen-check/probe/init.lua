-- Emerges the surface node at each place listed in the world's points.txt
-- (a direction per line, 2^30 a unit: terragen::Place), logs each column's
-- (looking past small plants and cactus, which stand on the ground) water
-- surface and solid surface and what core.terra says there (with the clock
-- it read), then stops the server. Lines are keyed by the place's number.
local points = {}
for line in io.lines(core.get_worldpath() .. "/points.txt") do
	local x, y, z = line:match("(-?%d+) (-?%d+) (-?%d+)")
	points[#points + 1] = { x = tonumber(x), y = tonumber(y), z = tonumber(z) }
end
local remaining = #points

local WATER = { ["tf_world:water_source"] = true, ["tf_world:water_flowing"] = true, ["tf_world:ice"] = true,
	["tf_world:lava_source"] = true, ["tf_world:lava_flowing"] = true }

-- from the atlas's top layer down (above it is seam)
local TOP = (core.planet.atlas() or error("terragen check: the world isn't a planet")).top - 1

local function column(p)
	local water = -9999
	for y = TOP, -120, -1 do
		local name = core.get_node({ x = p.x, y = y, z = p.z }).name
		if WATER[name] then
			if water == -9999 then water = y end
		elseif name ~= "air" and name ~= "ignore" and core.get_item_group(name, "plant") == 0 and name ~= "tf_world:cactus" then
			return y, name, water
		end
	end
	return nil, "none", water
end

-- The surface node at a place (core.planet.atlas_node). Its column's own
-- place is near it, not on it (a node is a block across): that's what's
-- logged as MAP (core.terra.place), and the prediction is made for it
-- (predict --columns).
local function onMap(d)
	local lat, lon = math.atan2(d.y, math.sqrt(d.x * d.x + d.z * d.z)), math.atan2(d.z, d.x)
	local n = core.planet.atlas_node(lat, lon, 0)
		or error(string.format("terragen check: no surface node at %.4f %.4f", lat, lon))
	local at = core.terra.place(n)
	return { x = n.x, z = n.z, place = at }
end

core.register_on_mods_loaded(function()
	core.after(1, function()
		for id, wp in ipairs(points) do
			local p = onMap(wp)
			core.log("action", string.format("MAP %d %d %d %d", id, p.place.x, p.place.y, p.place.z))
			core.emerge_area({ x = p.x, y = -160, z = p.z }, { x = p.x, y = 260, z = p.z }, function(_, _, left)
				if left > 0 then return end
				local y, name, water = column(p)
				-- and the block 20 below the surface: rock, ore or cave
				local deep = y and core.get_node({ x = p.x, y = y - 20, z = p.z }).name or "none"
				core.log("action", string.format("PROBE %d %s %s %d %s", id, tostring(y), name, water, deep))
				local pos = { x = p.x, y = 0, z = p.z }
				local sun, climate = core.terra.sun(pos), core.terra.climate(pos)
				local calendar = core.terra.calendar()
				local geology = core.terra.geology(pos)
				core.log("action", string.format("TERRA %d %d %.17g %d %d %.6f %.6f %.6f %.6f %.6f %s %s %.6f %s",
					id, core.get_day_count(), core.get_timeofday(), calendar.year, calendar.day, calendar.date,
					sun.latitude, sun.declination, sun.elevation, sun.daylight, sun.season,
					climate.biome, climate.temperature, geology.province:gsub(" ", "_")))
				local w = core.terra.weather({ x = p.x, y = math.max(y or 0, 1), z = p.z }) -- at the ground, where the climate is
				core.log("action", string.format("WEATHER %d %.4f %.4f %.4f %.4f %.4f %d %d %.4f",
					id, w.temperature, climate.temperature, w.precipitation, w.pressure, w.cloud, #w.events, core.get_day_count(), core.get_timeofday()))
				-- and what that weather does to the top of the column (snow, ice), applied
				local top = water ~= -9999 and { x = p.x, y = water, z = p.z } or (y and { x = p.x, y = y, z = p.z })
				if top then
					local node = core.get_node(top)
					local here = core.terra.weather(top)
					local action = tf_weather.act(top, node) or "none"
					local now = core.get_node(action == "snow" and vector.offset(top, 0, 1, 0) or top).name
					core.log("action", string.format("ACT %d %s %s %s %.3f %s %.4f", id, node.name, action, now,
						here.temperature, tostring(here.snow), here.precipitation))
				end
				remaining = remaining - 1
				if remaining == 0 then core.request_shutdown("probe done") end
			end)
		end
	end)
end)
