-- An atlas's seams, crossed (scripts/atlas-seam-check.sh; layout v2:
-- planet.paper.md, section 3). Courses of stone are laid along real paths
-- on the planet (core.planet.atlas_node), so each is whole across the seam
-- it tests; then, over each:
--   - a walker crosses at the floor's height, never in seam, never jumping
--     on the planet, still going, and ends up past the seam;
--   - water poured before the seam flows past it.
-- And on a segment's seam, sand dropped above lands below.
-- Prints ATLAS lines; the script reads them. ATLAS_TRACE=1 also prints what's
-- round a walker that stopped, the water along the cap course, and the sand's
-- fall and what it hit.

local atlas = assert(core.planet.atlas(), "atlas_seam_probe: not an atlas world")
local R = atlas.radius
local FLOOR = 120  -- the courses' layer: above the terrain, below the top
local SPEED = 3    -- m/s
local HALF = 2     -- blocks each side of a course's line
local STONE, SAND = "core:stone", "core:sand"
local WATER, FLOWING = "core:water_source", "core:water_flowing"

local results = { pass = 0, fail = 0 }
-- the server's steps: how long, and how many
local steps, longest = 0, 0
core.register_globalstep(function(dtime)
	steps = steps + 1
	longest = math.max(longest, dtime)
end)
local function check(ok, what)
	if ok then
		results.pass = results.pass + 1
	else
		results.fail = results.fail + 1
		if results.fail <= 30 then print("ATLAS miss " .. what) end
	end
end

local function node(lat, lon, layer)
	return core.planet.atlas_node(lat, lon, layer)
end
-- Keep a node's block active (transient, no limit: the engine's default of
-- 16 blocks refuses the rest).
local function keep(pos)
	if not core.forceload_block(pos, true, -1) then
		error("atlas_seam_probe: couldn't keep " .. core.pos_to_string(pos) .. " loaded")
	end
end
local function point(pos)
	return core.planet.atlas_point(pos)
end
local function distance(a, b)
	return math.sqrt((a.x - b.x) ^ 2 + (a.y - b.y) ^ 2 + (a.z - b.z) ^ 2)
end
-- two nodes one row (or layer) apart in the true geometry, stored a plain
-- step apart, are in the same grid; otherwise a seam is between them
-- the node a mover is in: a centimetre above its feet (planet::Atlas::moverNode)
local function moverNode(pos)
	return { x = math.floor(pos.x + 0.5), y = math.floor(pos.y + 0.51), z = math.floor(pos.z + 0.5) }
end
local function plain(a, b, axis)
	local d = vector.subtract(b, a)
	local want = { x = 0, y = 0, z = 0 }
	want[axis] = 1
	return d.x == want.x and d.y == want.y and d.z == want.z
end

-- Seams, found from geometry: segments' meridians, every 360°/segments
-- from longitude 0; the caps' circles.
local segments = 0
for _, r in ipairs(atlas.tiles) do
	if r.kind == "segment" then segments = segments + 1 end
end
local WIDTH = 2 * math.pi / segments
local function capEdge(lon)
	local lat = math.rad(80)
	while lat < math.rad(89) do
		local c = core.planet.atlas_cell(node(lat, lon, FLOOR))
		if c and c.region == "cap" then return lat end
		lat = lat + 1 / (3 * R)
	end
	error("atlas_seam_probe: no cap")
end
-- A course: s along it (blocks, the seam at 0), w across it.
local function eastward(lat, lon0)
	return function(s, w) return lat + w / R, lon0 + s / (R * math.cos(lat)) end
end
local function northward(lat0, lon)
	return function(s, w) return lat0 + s / R, lon + w / (R * math.cos(lat0 + s / R)) end
end

local courses = {}
local function course(name, path, axis, sign, past)
	courses[#courses + 1] = { name = name, path = path, axis = axis, sign = sign, past = past }
end
local cap0, cap45 = capEdge(0), capEdge(math.rad(45))
local seam1, seam5 = WIDTH, 5 * WIDTH
course("a segment's seam at longitude 0, east at the equator", eastward(0.4 / R, 0), "x", 1,
	function(lat, lon) return lon > 0 and lon < 6 / R end)
course("a segment's seam, east at the equator", eastward(0.4 / R, seam1), "x", 1,
	function(lat, lon) return lon > seam1 + 1 / R end)
course("a segment's seam, east near 45°", eastward(math.rad(45), seam5), "x", 1,
	function(lat, lon) return lon > seam5 + 1 / (R * math.cos(math.rad(45))) end)
course("a segment's seam, west near 30° south", function(s, w)
	return eastward(math.rad(-30), seam5)(-s, w)
end, "x", -1, function(lat, lon) return lon < seam5 - 1 / (R * math.cos(math.rad(30))) end)
course("into the cap at 0°", northward(cap0, 0), "z", 1,
	function(lat) return lat > cap0 + 1 / R end)
course("into the cap at 45°", northward(cap45, math.rad(45)), "z", 1,
	function(lat) return lat > cap45 + 1 / R end)

-- Every node a course needs: its floor, and air three layers over it; and
-- what's round it loaded (collision takes an unloaded node as solid, and
-- across a seam it looks into the far grid's storage, round its cells).
local function lay(c)
	c.floor, c.air, c.near = {}, {}, {}
	-- a tenth of a block apart: thin cut cells at the seam are cells too
	for s = -12, 12, 0.1 do
		for w = -HALF, HALF, 0.25 do
			local lat, lon = c.path(s, w)
			c.floor[#c.floor + 1] = node(lat, lon, FLOOR)
			for up = 1, 3 do c.air[#c.air + 1] = node(lat, lon, FLOOR + up) end
		end
	end
	for s = -14, 14, 1 do
		for w = -HALF - 3, HALF + 3, 1 do
			local lat, lon = c.path(s, w)
			for layer = FLOOR - 2, FLOOR + 5 do c.near[#c.near + 1] = node(lat, lon, layer) end
		end
	end
end

core.register_entity("atlas_seam_probe:walker", {
	initial_properties = {
		physical = true,
		collide_with_objects = false,
		collisionbox = { -0.3, 0, -0.3, 0.3, 1.7, 0.3 },
		visual = "sprite",
		textures = { "blank.png" },
		static_save = false,
	},
	on_step = function(self, dt, moveresult)
		local pos = self.object:get_pos()
		if not pos or not self.case then return end
		if os.getenv("ATLAS_TRACE") and moveresult and moveresult.collides then
			for _, info in ipairs(moveresult.collisions) do
				if info.axis ~= "y" and info.node_pos then
					local c = core.planet.atlas_cell(info.node_pos)
					print(("ATLAS trace %s hit %s at %s axis %s (%s), from %s"):format(self.case, core.get_node(info.node_pos).name,
						core.pos_to_string(info.node_pos), info.axis,
						c and (c.region .. " " .. c.index .. " " .. c.i .. "," .. c.j .. "," .. c.k) or "no cell",
						core.pos_to_string(pos, 2)))
				end
			end
		end
		local cell = core.planet.atlas_cell(moverNode(pos))
		check(cell ~= nil, self.case .. ": walker in seam at " .. core.pos_to_string(pos, 2))
		local here = point(pos)
		if here and self.last then
			local moved = distance(here, self.last)
			check(moved <= SPEED * dt + 1, (self.case .. ": jumped %.2f blocks in %.3f s"):format(moved, dt))
		end
		self.last = here
	end,
})

local function walk(done)
	local going = {}
	for _, c in ipairs(courses) do
		local lat, lon = c.path(-6, 0)
		local start = node(lat, lon, FLOOR + 1)
		local obj = core.add_entity({ x = start.x, y = start.y - 0.5, z = start.z }, "atlas_seam_probe:walker")
			or error("atlas_seam_probe: no walker")
		obj:set_acceleration({ x = 0, y = -9.81, z = 0 })
		local v = { x = 0, y = 0, z = 0 }
		v[c.axis] = SPEED * c.sign
		obj:set_velocity(v)
		obj:get_luaentity().case = c.name
		going[#going + 1] = { c = c, obj = obj }
	end
	core.after(4, function()
		for _, g in ipairs(going) do
			local p, v = g.obj:get_pos(), g.obj:get_velocity()
			local cell = core.planet.atlas_cell(moverNode(p))
			check(cell ~= nil and cell.k == FLOOR + 1,
				g.c.name .. ": walker at layer " .. tostring(cell and cell.k) .. ", not on the floor")
			local lat, lon = core.planet.atlas_place(moverNode(p))
			local past = lat ~= nil and g.c.past(lat, lon)
			check(past, g.c.name .. ": walker didn't get past the seam")
			if not past then
				local la0, lo0 = g.c.path(-6, 0)
				local la1, lo1 = g.c.path(6, 0)
				print(("ATLAS trace %s: from %.4f°, %.4f° toward %.4f°, %.4f°; at %s, %.4f°, %.4f°"):format(g.c.name,
					math.deg(la0), math.deg(lo0), math.deg(la1), math.deg(lo1), core.pos_to_string(p, 2),
					math.deg(lat or 0), math.deg(lon or 0)))
				local m = moverNode(p)
				for dy = 0, 1 do
					for d = -1, 2 do
						local q = { x = m.x, y = m.y + dy, z = m.z }
						q[g.c.axis] = q[g.c.axis] + d * g.c.sign
						local c = core.planet.atlas_cell(q)
						print(("ATLAS trace   %s+%d up %d: %s %s"):format(g.c.axis, d, dy, core.get_node(q).name,
							c and (c.region .. " " .. c.index .. " " .. c.i .. "," .. c.j .. "," .. c.k) or "no cell"))
					end
				end
				print(("ATLAS trace   velocity %s"):format(core.pos_to_string(v, 2)))
			end
			check(math.sqrt(v.x * v.x + v.z * v.z) > SPEED * 0.9, g.c.name .. ": walker stopped")
			if os.getenv("ATLAS_TRACE") and math.sqrt(v.x * v.x + v.z * v.z) < 1 then
				local m = moverNode(p)
				print(("ATLAS trace stopped at %.6f %.6f %.6f, node %s"):format(p.x, p.y, p.z, core.pos_to_string(m)))
				for dy = -1, 2 do
					for dz = -1, 1 do
						local q = { x = m.x, y = m.y + dy, z = m.z + dz }
						local c = core.planet.atlas_cell(q)
						print(("ATLAS trace   +(0,%d,%d) %s %s"):format(dy, dz, core.get_node(q).name,
							c and ("j" .. c.j .. " k" .. c.k .. " i" .. c.i) or "no cell"))
					end
				end
			end
			g.obj:remove()
		end
		done()
	end)
end

local function pour(done)
	for _, c in ipairs(courses) do
		local lat, lon = c.path(-1.5, 0)
		c.spring = node(lat, lon, FLOOR + 1)
		lat, lon = c.path(1.5, 0)
		c.beyond = node(lat, lon, FLOOR + 1)
		check(core.get_node(c.beyond).name == "air", c.name .. ": past the seam isn't dry to start")
		core.set_node(c.spring, { name = WATER })
	end
	core.after(8, function() -- into a cap's grid at 45° is three flows
		for _, c in ipairs(courses) do
			local n = core.get_node(c.beyond).name
			check(n == FLOWING or n == WATER, c.name .. ": past the seam is " .. n .. ", not water")
			if os.getenv("ATLAS_TRACE") and n ~= FLOWING and n ~= WATER then
				for s = -3, 4, 0.5 do
					local la, lo = c.path(s, 0)
					local q = node(la, lo, FLOOR + 1)
					local cell = core.planet.atlas_cell(q)
					print(("ATLAS trace water s=%.1f %s %s %s"):format(s, core.pos_to_string(q), core.get_node(q).name,
						cell and (cell.region .. " " .. cell.i .. "," .. cell.j) or "seam"))
				end
			end
		end
		done()
	end)
end

-- Sand on a segment's seam: dropped from six layers up, lands on stone at
-- the bottom of a pit across the seam.
if os.getenv("ATLAS_TRACE") then
	local def = core.registered_entities["__builtin:falling_node"]
	local step = def.on_step
	local place = def.try_place
	def.try_place = function(self, bcp, bcn)
		local ok = place(self, bcp, bcn)
		local c = core.planet.atlas_cell(bcp)
		local above = vector.offset(bcp, 0, 1, 0)
		print(("ATLAS trace sand placed %s on %s %s (%s); above it %s"):format(tostring(ok), core.pos_to_string(bcp),
			bcn.name, c and (c.region .. " " .. c.index .. " " .. c.i .. "," .. c.j .. "," .. c.k) or "seam",
			core.get_node(above).name))
		return ok
	end
	def.on_deactivate = function(self, removal)
		local pos = self.object:get_pos()
		print(("ATLAS trace falling node %s at %s"):format(removal and "removed" or "deactivated",
			core.pos_to_string(pos, 3)) .. "\n" .. debug.traceback())
	end
	def.on_step = function(self, dtime, moveresult)
		if moveresult and moveresult.collides then
			for _, info in ipairs(moveresult.collisions) do
				print(("ATLAS trace hit %s %s at %s (axis %s), ground %s"):format(info.type,
					info.node_pos and core.get_node(info.node_pos).name or "-",
					info.node_pos and core.pos_to_string(info.node_pos) or "-", tostring(info.axis),
					tostring(moveresult.touching_ground)))
			end
		end
		return step(self, dtime, moveresult)
	end
	local t = 0
	core.register_globalstep(function(dt)
		t = t + dt
		for _, o in pairs(core.object_refs) do
			local e = o:get_luaentity()
			if e and e.name == "__builtin:falling_node" then
				local pos = o:get_pos()
				local c = core.planet.atlas_cell(moverNode(pos))
				print(("ATLAS trace falling %.2f %s %s v%.2f"):format(t, core.pos_to_string(pos, 3),
					c and (c.region .. " k" .. c.k) or "seam", o:get_velocity().y))
			end
		end
	end)
end

local DROP_LAT, DROP_LON, DROP_K = math.rad(12), 13 * WIDTH, FLOOR - 10
local function drop(done)
	local lat, lon, k = DROP_LAT, DROP_LON, DROP_K
	local ground, top = node(lat, lon, k - 3), node(lat, lon, k + 4)
	-- a headless server has no players, so no active blocks, and a falling
	-- node (saved with the map) in an inactive block is put away mid-fall:
	-- keep the pit's blocks, both segments' storage, active (collision takes
	-- an unloaded node as solid ignore, and sand touching ignore is deleted)
	for dlat = -3, 3 do
		for dlon = -3, 3 do
			local la, lo = lat + dlat / R, lon + dlon / (R * math.cos(lat))
			for layer = k - 3, k + 6 do keep(node(la, lo, layer)) end
		end
	end
	-- an open pit, not a shaft: the ground there may be rock, and across a
	-- seam the next grid's columns are offset by up to half a block, so a
	-- shaft's walls would stick into the sand's way (as they should)
	for dlat = -3, 3 do
		for dlon = -3, 3 do
			local la, lo = lat + dlat / R, lon + dlon / (R * math.cos(lat))
			core.set_node(node(la, lo, k - 3), { name = STONE })
			for layer = k - 2, k + 6 do core.set_node(node(la, lo, layer), { name = "air" }) end
		end
	end
	core.set_node(top, { name = SAND })
	core.spawn_falling_node(top)
	core.after(4, function()
		-- the two segments' columns don't line up at the seam: it settles in
		-- the column it hits, within one of straight below
		local landed = false
		for dlat = -1, 1 do
			for dlon = -1.5, 1.5, 0.5 do
				local q = node(lat + dlat / R, lon + dlon / (R * math.cos(lat)), k - 2)
				landed = landed or core.get_node(q).name == SAND
			end
		end
		check(landed, ("sand on the seam at layer %d didn't land within a column of below"):format(k))
		if os.getenv("ATLAS_TRACE") then
			for layer = k - 3, k + 6 do
				print(("ATLAS trace sand column layer %d: %s"):format(layer, core.get_node(node(lat, lon, layer)).name))
			end
			for dlat = -3, 3 do
				for dlon = -3, 3, 0.5 do
					for layer = k - 3, k + 6 do
						local q = node(lat + dlat / R, lon + dlon / (R * math.cos(lat)), layer)
						if q and core.get_node(q).name == SAND then
							print(("ATLAS trace sand found %s layer %d (dlat %d dlon %.1f)"):format(core.pos_to_string(q), layer, dlat, dlon))
						end
					end
				end
			end
			for _, o in pairs(core.object_refs) do
				local e = o:get_luaentity()
				local pos = o:get_pos()
				local c = pos and core.planet.atlas_cell(moverNode(pos))
				print("ATLAS trace object " .. (e and e.name or "?") .. " at " .. core.pos_to_string(pos, 2) ..
					" " .. (c and (c.region .. " k" .. c.k) or "seam") .. " " .. (e and e.itemstring or ""))
			end
		end
		done()
	end)
end

core.after(1, function()
	local blocks, need = {}, {}
	local function want(p)
		local b = vector.floor(vector.divide(p, 16))
		local key = core.hash_node_position(b)
		if not blocks[key] then blocks[key] = true; need[#need + 1] = p end
	end
	for _, c in ipairs(courses) do
		lay(c)
		for _, p in ipairs(c.near) do want(p) end
	end
	for layer = DROP_K - 5, DROP_K + 8 do
		for dlat = -3, 3 do
			for dlon = -3, 3 do
				want(node(DROP_LAT + dlat / R, DROP_LON + dlon / R, layer))
			end
		end
	end
	print(("ATLAS %d courses over %d map blocks"):format(#courses, #need))
	local left = #need
	for _, p in ipairs(need) do
		core.emerge_area(p, p, function(_, _, remaining)
			if remaining > 0 then return end
			left = left - 1
			if left > 0 then return end
			for _, c in ipairs(courses) do
				for _, q in ipairs(c.air) do core.set_node(q, { name = "air" }) end
				for _, q in ipairs(c.floor) do core.set_node(q, { name = STONE }) end
				-- a headless server has no players, so no active blocks: the
				-- walkers and the water step only in blocks kept active
				for _, q in ipairs(c.near) do keep(q) end
			end
			walk(function()
				pour(function()
					drop(function()
						print(("ATLAS steps %d, longest %.2f s"):format(steps, longest))
						print(("ATLAS pass %d fail %d"):format(results.pass, results.fail))
						core.request_shutdown()
					end)
				end)
			end)
		end)
	end
end)
