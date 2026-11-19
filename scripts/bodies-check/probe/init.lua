-- A world's bodies, end to end (scripts/bodies-check.sh, sprint 40): the
-- home and its moon in one window; a mark on each, kept as a place; every
-- tile of both pushed out of the map (past the pager's hold), and each mark
-- found again by its place. Prints BODIES lines; the last is "BODIES pass"
-- or "BODIES fail".
local function say(...) print("BODIES " .. string.format(...)) end
local MARK = "core:seam" -- a node the ground never has
local ok = true
local function check(cond, ...)
	if not cond then
		ok = false
		say("FAILED: " .. string.format(...))
	end
	return cond
end
local function finish()
	say("%s", ok and "pass" or "fail")
	core.request_shutdown("bodies probe done", false, 0)
end

-- A column of a body made, layers −40 to 120.
local function emerge_column(lat, lon, body, then_)
	local low = core.planet.atlas_node(lat, lon, -40, body)
	local high = core.planet.atlas_node(lat, lon, 120, body)
	local a = vector.new(math.min(low.x, high.x), low.y, math.min(low.z, high.z))
	local b = vector.new(math.max(low.x, high.x), high.y, math.max(low.z, high.z))
	core.emerge_area(vector.subtract(a, 16), vector.add(b, 16), function(_, _, left)
		if left == 0 then then_() end
	end)
end

-- The first air over walkable ground in a made column; nil over the sea.
local function ground(lat, lon, body)
	for layer = -40, 119 do
		local here = core.planet.atlas_node(lat, lon, layer, body)
		local above = core.planet.atlas_node(lat, lon, layer + 1, body)
		local def = core.registered_nodes[core.get_node(here).name]
		if core.get_node(above).name == "air" and def and def.walkable then
			return above
		end
	end
end

local bodies = assert(core.planet.bodies(), "bodies_probe: not an atlas world")
local names = {}
for _, b in ipairs(bodies) do names[#names + 1] = string.format("%s %.0f", b.id, b.radius) end
say("bodies: %s", table.concat(names, ", "))
local home = bodies[1].id
local has_moon = false
for _, b in ipairs(bodies) do has_moon = has_moon or b.id == "moon" end
check(has_moon, "the moon among the bodies")

local marks = {} -- body → { place, pos }

-- Both tiles pushed out, past the hold, and each mark found by its place.
local function come_back()
	local left = 0
	for _ in pairs(marks) do left = left + 1 end
	for body, m in pairs(marks) do
		local pos = vector.round(core.planet.pos(m.place))
		check(core.planet.body_at(pos) == body, "%s's place is back on %s", body, body)
		core.emerge_area(vector.subtract(pos, 16), vector.add(pos, 16), function(_, _, n)
			if n ~= 0 then return end
			local name = core.get_node(pos).name
			check(name == MARK, "%s's mark kept (found %s)", body, name)
			say("%s's mark at %s now: %s", body, core.pos_to_string(pos), name == MARK and "kept" or "LOST")
			left = left - 1
			if left == 0 then finish() end
		end)
	end
end

local function push_out()
	for n = 0, 400 do
		local la = math.rad(-60 + (n % 23) * 5)
		local lo = math.rad(180 + math.floor(n / 23) * 10)
		pcall(core.planet.atlas_node, la, lo, 0)
	end
	for body, m in pairs(marks) do
		local now = core.planet.place(m.pos)
		check(not now or now.body ~= body or now.i ~= m.place.i or now.j ~= m.place.j,
				"%s's mark's tile left the map", body)
	end
	local moons = 0
	for _, t in ipairs(core.planet.atlas().tiles) do
		if t.body == "moon" then moons = moons + 1 end
	end
	say("after filling: %d tiles, %d of the moon", #core.planet.atlas().tiles, moons)
	core.after(31, come_back)
end

local function mark(body, pos)
	core.set_node(pos, { name = MARK })
	local place = core.planet.place(pos)
	check(place and place.body == body, "%s's mark has a place on it", body)
	-- while its tile is still in, a place is its position
	check(place and vector.equals(vector.round(core.planet.pos(place)), pos), "%s's place is its position", body)
	marks[body] = { place = place, pos = pos }
end

local function on_the_moon(on, spawn)
	say("the moon's ground at %s: %s", core.pos_to_string(on), core.get_node(vector.offset(on, 0, -1, 0)).name)
	-- its sky, its land and its air, its own
	local sun = core.terra.sun(on)
	local surface = core.terra.surface(on)
	local w = core.terra.weather(on)
	check(sun.latitude ~= nil, "the moon's sun")
	check(surface.ground ~= nil, "the moon's surface")
	check(w.atmosphere == false, "the moon's air isn't simulated")
	say("the moon's sun %.1f° up, its ground %d, %.1f °C", sun.elevation, surface.ground, w.temperature)
	-- where /travel lands on the moon: on its land, by its own spawn levels
	local landing = core.planet.spawn_position("moon")
	check(core.planet.body_at(landing) == "moon", "the moon's landing is on the moon")
	say("the moon's landing: %s", core.pos_to_string(landing))
	mark(home, vector.offset(spawn, 0, 3, 0))
	mark("moon", vector.offset(on, 0, 2, 0))
	say("tiles in the window: %d", #core.planet.atlas().tiles)
	core.after(32, push_out)
end

-- Land on the moon: the first of a few places that isn't sea.
local SPOTS = { { 0.3, 1.0 }, { -0.4, 2.0 }, { 0.6, -1.5 }, { -0.2, -2.5 }, { 0.1, 0.2 }, { 0.8, 3.0 } }
local function find_land(n, spawn)
	if n > #SPOTS then
		check(false, "land on the moon")
		return finish()
	end
	local lat, lon = SPOTS[n][1], SPOTS[n][2]
	check(core.planet.body_at(core.planet.atlas_node(lat, lon, 0, "moon")) == "moon", "a moon node is the moon's")
	emerge_column(lat, lon, "moon", function()
		local on = ground(lat, lon, "moon")
		if not on then
			say("sea at %.1f, %.1f on the moon", lat, lon)
			return find_land(n + 1, spawn)
		end
		on_the_moon(on, spawn)
	end)
end

core.after(0.5, function()
	local spawn = vector.round(core.planet.spawn_position())
	check(core.planet.body_at(spawn) == home, "the spawn on the home")
	core.emerge_area(vector.subtract(spawn, 16), vector.add(spawn, 16), function(_, _, left)
		if left == 0 then find_land(1, spawn) end
	end)
end)
