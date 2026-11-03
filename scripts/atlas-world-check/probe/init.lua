-- An atlas world, generated (scripts/atlas-world-check.sh, sprint 30):
-- boxes where the atlas's storage changes (the equator from the surface
-- down, a segment's seam near 60°, both caps' edges, a region's edge and the
-- gap beside it, the core's sphere) emerged, then every node checked:
--   - a node that stores a cell is generated content, never seam or ignore;
--   - a node that stores no cell is seam, whatever ran after the terrain.
-- Prints ATLAS lines; the script reads them.

local atlas = assert(core.planet.atlas(), "atlas_probe: not an atlas world")
local SEAM = "tf_planet:seam"
local R = atlas.radius
local function at(latDeg, lonDeg, layer)
	return assert(core.planet.atlas_node(math.rad(latDeg), math.rad(lonDeg), layer), "atlas_probe: no node there")
end
local function around(p, name, extra)
	local box = { name = name, min = { x = p.x - 16, y = p.y - 8, z = p.z - 16 }, max = { x = p.x + 15, y = p.y + 8, z = p.z + 15 } }
	for k, v in pairs(extra or {}) do box[k] = v end
	return box
end

-- a segment's box's right edge at the surface: the gap beside it
local edge
for _, r in ipairs(atlas.tiles) do
	if r.kind == "segment" then
		edge = { x = r.x + r.width, y = 0, z = r.z + math.floor(r.depth / 2) }
		break
	end
end
-- the core's box: its middle
local inner
for _, r in ipairs(atlas.tiles) do
	if r.kind == "core" then inner = { x = r.x + math.floor(r.width / 2), y = 0, z = r.z + math.floor(r.depth / 2) } end
end
assert(inner, "atlas_probe: no core")
local capLat = 90 - 5 -- the cap's edge at the default 85°: a box straddling it
-- on the equator at a coast of TF16.dGVycmE's (129° east), so ground and air
-- both meet the box at sea level
local equator = at(0, 129, 0)
local boxes = {
	around(equator, "the equator, its surface and below", { terrain = true,
		min = { x = equator.x - 16, y = -24, z = equator.z - 16 }, max = { x = equator.x + 15, y = 24, z = equator.z + 15 } }),
	around(at(60, 15, 0), "a segment's seam near 60°"),
	around(at(capLat + 0.2, 30, 0), "the north cap's edge"),
	around(at(-capLat - 0.2, 30, 0), "the south cap's edge"),
	around(edge, "a region's edge and the gap"),
	around(at(45, 10, math.floor(atlas.core - atlas.radius) + 4), "a segment's floor at the core's sphere"),
	around(inner, "the core's own grid", { min = { x = inner.x - 16, y = -8, z = inner.z - 16 }, max = { x = inner.x + 15, y = 8, z = inner.z + 15 } }),
}

local results = { pass = 0, fail = 0, cells = {}, seam = 0, kinds = {} }
local function check(ok, what)
	if ok then
		results.pass = results.pass + 1
	else
		results.fail = results.fail + 1
		if results.fail <= 20 then print("ATLAS miss " .. what) end
	end
end

local function inspect(box)
	for z = box.min.z, box.max.z do
		for y = box.min.y, box.max.y do
			for x = box.min.x, box.max.x do
				local pos = { x = x, y = y, z = z }
				local cell = core.planet.atlas_cell(pos)
				local name = core.get_node(pos).name
				if cell then
					results.cells[cell.region] = (results.cells[cell.region] or 0) + 1
					if box.terrain then
						local kind = name == "air" and "air" or (core.get_item_group(name, "liquid") > 0 and "liquid" or "solid")
						results.kinds[kind] = (results.kinds[kind] or 0) + 1
					end
					check(name ~= SEAM and name ~= "ignore",
						("%s: cell %s at %s is %s"):format(box.name, cell.region, core.pos_to_string(pos), name))
				else
					results.seam = results.seam + 1
					check(name == SEAM, ("%s: no cell at %s, but %s"):format(box.name, core.pos_to_string(pos), name))
				end
			end
		end
	end
end

local spawn, finish2 -- after the boxes: the spawn, then the tally

local function finish()
	for region, n in pairs(results.cells) do print(("ATLAS cells %s %d"):format(region, n)) end
	print(("ATLAS cells seam %d"):format(results.seam))
	check((results.cells.segment or 0) > 0, "no segment cells seen")
	check((results.cells.cap or 0) > 0, "no cap cells seen")
	check((results.cells.core or 0) > 0, "no core cells seen")
	check(results.seam > 0, "no seam seen")
	-- the terrain is real: ground and air at the equator's sea level
	for _, kind in ipairs({ "solid", "air" }) do
		print(("ATLAS equator %s %d"):format(kind, results.kinds[kind] or 0))
		check((results.kinds[kind] or 0) > 0, "no " .. kind .. " at the equator's sea level")
	end
	-- and somewhere along the equator there's land to spawn on
	local land
	for lon = 0, 359, 3 do
		local p = at(0, lon, 0)
		local y = core.get_spawn_level(p.x, p.z)
		if y then land = { x = p.x, y = y, z = p.z } break end
	end
	print(("ATLAS land on the equator: %s"):format(land and core.pos_to_string(land) or "none"))
	check(land ~= nil, "no land anywhere on the equator")
	spawn(finish2)
end

-- A new player's spawn (tf_planet.spawn_position): a cell, open, over
-- ground: within three layers below it on the planet (terragen's spawn level
-- is a few above the top node, as on the cube; and through the atlas, as the
-- layers below can be another grid's at a seam) there's something to stand on.
spawn = function(done)
	local pos = tf_planet.spawn_position()
	local cell = core.planet.atlas_cell(pos)
	check(cell ~= nil, "spawn " .. core.pos_to_string(pos) .. " is seam")
	if not cell then return done() end
	local lat, lon = core.planet.atlas_place(pos)
	local below = {}
	for d = 1, 3 do below[d] = core.planet.atlas_node(lat, lon, cell.k - d) end
	local need, left = { pos, below[1], below[2], below[3] }, 4
	for _, p in ipairs(need) do
		core.emerge_area(p, p, function(_, _, l)
			if l > 0 then return end
			left = left - 1
			if left > 0 then return end
			local here = core.get_node(pos).name
			local under, ground = {}, false
			for d = 1, 3 do
				under[d] = core.get_node(below[d]).name
				ground = ground or core.registered_nodes[under[d]].walkable
			end
			print(("ATLAS spawn %s layer %d: %s over %s"):format(core.pos_to_string(pos), cell.k, here,
				table.concat(under, ", ")))
			check(not core.registered_nodes[here].walkable, "spawn is inside " .. here)
			check(ground, "nothing to stand on under the spawn")
			done()
		end)
	end
end

finish2 = function()
	print(("ATLAS pass %d fail %d"):format(results.pass, results.fail))
	core.request_shutdown()
end

core.after(0, function()
	local left = #boxes
	local started = core.get_us_time()
	for _, box in ipairs(boxes) do
		core.emerge_area(box.min, box.max, function(_, _, remaining)
			if remaining > 0 then return end
			inspect(box)
			left = left - 1
			if left == 0 then
				print(("ATLAS generated %d boxes in %.1f s"):format(#boxes, (core.get_us_time() - started) / 1e6))
				finish()
			end
		end)
	end
end)
