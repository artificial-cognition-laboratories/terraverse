-- The debug sphere, generated (scripts/atlas-debug-check.sh, sprint 30):
-- boxes where things meet (the equator from the surface down, a segment's
-- seam near 60°, the caps' edges), every node checked:
--   - above the ground (layer ≥ 0), air in full sun;
--   - the ground (layer < 0), painted (core:atlas_debug_*);
--   - storage that is no cell, seam;
-- and every paint is somewhere. Prints ATLAS lines; the script reads them.

local atlas = assert(core.planet.atlas(), "atlas_debug_probe: not an atlas world")
local results = { pass = 0, fail = 0, paints = {} }
local function check(ok, what)
	if ok then
		results.pass = results.pass + 1
	else
		results.fail = results.fail + 1
		if results.fail <= 20 then print("ATLAS miss " .. what) end
	end
end
local function at(latDeg, lonDeg, layer)
	return assert(core.planet.atlas_node(math.rad(latDeg), math.rad(lonDeg), layer))
end
local function around(p, name, dy)
	return { name = name, min = { x = p.x - 16, y = p.y - dy, z = p.z - 16 }, max = { x = p.x + 15, y = p.y + dy, z = p.z + 15 } }
end
local boxes = {
	around(at(0, 3, 0), "the equator, its surface and below", 24),
	around(at(60, 15, 0), "a segment's seam near 60°", 6),
	around(at(85.2, 30, 0), "the north cap's edge", 6),
	around(at(-85.2, 30, 0), "the south cap's edge", 6),
}

local function inspect(box)
	for z = box.min.z, box.max.z do
		for y = box.min.y, box.max.y do
			for x = box.min.x, box.max.x do
				local pos = { x = x, y = y, z = z }
				local cell = core.planet.atlas_cell(pos)
				local name = core.get_node(pos).name
				if not cell then
					check(name == "core:seam", box.name .. ": no cell, but " .. name)
				elseif cell.k >= 0 then
					local light = core.get_node_light(pos, 0.5)
					check(name == "air" and light == 15,
						("%s: layer %d at %s is %s lit %s"):format(box.name, cell.k, core.pos_to_string(pos), name, tostring(light)))
				else
					local paint = name:match("^core:atlas_debug_(.+)$")
					check(paint ~= nil, ("%s: ground at %s is %s"):format(box.name, core.pos_to_string(pos), name))
					if paint then results.paints[paint] = (results.paints[paint] or 0) + 1 end
				end
			end
		end
	end
end

core.after(0, function()
	local left, started = #boxes, core.get_us_time()
	for _, box in ipairs(boxes) do
		core.emerge_area(box.min, box.max, function(_, _, remaining)
			if remaining > 0 then return end
			inspect(box)
			left = left - 1
			if left > 0 then return end
			print(("ATLAS generated %d boxes in %.1f s"):format(#boxes, (core.get_us_time() - started) / 1e6))
			for _, p in ipairs({ "segment_a", "segment_b", "cap", "segment_seam", "cap_edge", "cut", "line" }) do
				print(("ATLAS paint %s %d"):format(p, results.paints[p] or 0))
				check((results.paints[p] or 0) > 0, "no " .. p .. " painted")
			end
			local spawn = core.planet.spawn_position()
			local cell = core.planet.atlas_cell(spawn)
			check(cell ~= nil and cell.k == 0, ("the spawn %s is at layer %s, not 0 (spawn level %s)"):format(
				core.pos_to_string(spawn), tostring(cell and cell.k), tostring(core.get_spawn_level(spawn.x, spawn.z))))
			print(("ATLAS pass %d fail %d"):format(results.pass, results.fail))
			core.request_shutdown()
		end)
	end
end)
