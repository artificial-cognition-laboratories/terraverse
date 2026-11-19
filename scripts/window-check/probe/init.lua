-- The planet's window, end to end (scripts/window-check.sh, sprint 39): a
-- mark made at the spawn survives its tile leaving the map (the window
-- filled with elsewhere, past the pager's hold) and coming back. Prints
-- WINDOW lines; the last is "WINDOW pass" or "WINDOW fail".
local function say(...) print("WINDOW " .. string.format(...)) end
local atlas = assert(core.planet.atlas(), "window_probe: not a planet")
local MARK = "core:seam" -- a node the ground never has
local function emerged(pos, then_)
	core.emerge_area(vector.subtract(pos, 16), vector.add(pos, 16), function(_, _, left)
		if left == 0 then then_() end
	end)
end
local function tileOf(pos)
	for _, t in ipairs(core.planet.atlas().tiles) do
		if pos.x >= t.x and pos.x < t.x + t.width and pos.z >= t.z and pos.z < t.z + t.depth then
			return t
		end
	end
end
local lat, lon
core.after(0.5, function()
	local spawn = vector.round(core.planet.spawn_position())
	local cell = assert(core.planet.atlas_cell(spawn))
	say("spawn %s, cell %s %d %d %d %d", core.pos_to_string(spawn), cell.region, cell.index, cell.i, cell.j, cell.k)
	emerged(spawn, function()
		lat, lon = core.planet.atlas_place(spawn)
		local above = vector.offset(spawn, 0, 3, 0)
		say("above the spawn: %s", core.get_node(above).name)
		core.set_node(above, { name = MARK })
		local before = tileOf(spawn)
		say("tiles in the window: %d; the spawn's at x %d z %d", #core.planet.atlas().tiles, before.x, before.z)
		-- past the pager's hold, fill the window with elsewhere
		core.after(32, function()
			local placed, gone = 0, false
			for n = 0, 400 do
				-- a quarter turn and more from the spawn, so never its tile
				local la = math.rad(-60 + (n % 23) * 5)
				local lo = lon + math.rad(90 + math.floor(n / 23) * 10)
				local ok, err = pcall(core.planet.atlas_node, la, lo, 0)
				if not ok then say("full, as it should be: %s", tostring(err):sub(1, 60)) end
				placed = placed + 1
				local now = core.planet.atlas_cell(spawn)
				if not now or now.i ~= cell.i or now.j ~= cell.j or now.region ~= cell.region or now.index ~= cell.index then
					gone = true
					break
				end
			end
			say("asked for %d places elsewhere; the spawn's tile %s; tiles in the window: %d", placed,
				gone and "left the map" or "STAYED", #core.planet.atlas().tiles)
			core.after(31, function()
			-- and back: the same place on the planet
			local again = assert(core.planet.atlas_node(lat, lon, spawn.y))
			local found = core.planet.atlas_cell(again)
			say("the spawn's place is at %s now: cell %s %d %d %d %d (%s)", core.pos_to_string(again), found.region, found.index,
				found.i, found.j, found.k, (found.i == cell.i and found.j == cell.j and found.k == cell.k) and "the same cell" or "ANOTHER CELL")
			emerged(again, function()
				local mark = core.get_node(vector.offset(again, 0, 3, 0)).name
				say("the mark: %s", mark == MARK and "kept" or ("LOST (" .. mark .. ")"))
				local same = found.i == cell.i and found.j == cell.j and found.k == cell.k
				say("%s", (gone and same and mark == MARK) and "pass" or "fail")
				core.request_shutdown("probe done")
			end)
			end)
		end)
	end)
end)
