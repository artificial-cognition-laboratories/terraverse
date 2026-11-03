-- The far view draws each column as core.terra.surface says the generator
-- makes it. Generate some areas for real and compare, column by column:
-- the ground (its block and height) and any water over it. Prints FAR
-- lines; scripts/far-check.sh reads them.

-- one area by the middle of each of six regions, found by place (whatever
-- the storage): five segments' middles on the equator, and the north pole;
-- each a degree north-east of the middle, so it runs north-east within one
-- tile (a grid's middle is where its tiles meet)
local SIZE = 96
local BODY = (os.getenv("FAR_BODY") ~= "" and os.getenv("FAR_BODY")) or nil -- nil: the home; else another body's id, its own land
local AREAS = {}
local atlas = core.planet.atlas() or error("far check: the world isn't a planet")
for _, lon in ipairs({ 7.5, 82.5, 157.5, 232.5, 307.5 }) do
	local n = core.planet.atlas_node(math.rad(1), math.rad(lon + 1), 0, BODY)
	AREAS[#AREAS + 1] = { n.x, n.z }
end
local pole = core.planet.atlas_node(math.rad(89.5), math.rad(45), 0, BODY)
AREAS[#AREAS + 1] = { pole.x, pole.z }
assert(#AREAS == 6, "far check: only " .. #AREAS .. " regions to probe")

local function kind(name)
	local def = core.registered_nodes[name]
	if name == "air" or name == "tf_planet:seam" or not def then return "air" end -- (seam: above the tile's top)
	local g = def.groups or {}
	if (g.tree or 0) > 0 or (g.leaves or 0) > 0 then return "tree" end
	if def.drawtype == "plantlike" or def.drawtype == "nodebox" then return "plant" end -- planted after: tufts, cacti
	-- (the world's water is the matter's since sprint 80: group liquid, no Luanti liquidtype)
	if def.liquidtype ~= "none" or (g.liquid or 0) > 0 or name == "tf_world:ice" then return "liquid" end -- a frozen sea's top is its water
	return "solid"
end

-- the real column, top down
local function real(data, area, x, z, top, bottom)
	local out = {}
	for y = top, bottom, -1 do
		local name = core.get_name_from_content_id(data[area:index(x, y, z)])
		local k = kind(name)
		if k == "tree" and not out.tree then out.tree = { y = y, name = name } end
		if k == "liquid" and not out.water then out.water = { y = y, name = name } end
		if k == "solid" then out.ground = { y = y, name = name } return out end
	end
	return out
end

local totals = { columns = 0, ground = 0, water = 0, rooted = 0, seam = 0 }
local shown = 0
local function miss(what, x, z, want, got)
	if shown < 12 then
		print(("FAR miss %s at %d %d: far %s, world %s"):format(what, x, z, want, got))
		shown = shown + 1
	end
end

local pending = #AREAS
core.after(0, function()
	-- facing its star: a new player starts where it's livable, under a sun that stays
	if core.terra.world().facing_star[""] then
		local at = tf_planet.spawn_position()
		local sun, climate = core.terra.sun(at), core.terra.climate(at)
		print(("FAR held turns %g elevation %.1f daylight %g temperature %.1f"):format(sun.turns, sun.elevation, sun.daylight, climate.temperature))
		assert(sun.turns == 0 and sun.elevation > -30 and sun.elevation < 45, "the sun where a new player starts")
		assert(climate.temperature > -15 and climate.temperature < 45, "the climate where a new player starts")
	end
	for _, a in ipairs(AREAS) do
		-- (another body's land isn't near the home's sea level: round its own ground there)
		local middle = BODY and core.terra.surface({ x = a[1], y = 0, z = a[2] }).ground
		local minp = { x = a[1], y = BODY and middle - 200 or -200, z = a[2] }
		local maxp = { x = a[1] + SIZE - 1, y = BODY and middle + 200 or 255, z = a[2] + SIZE - 1 }
		core.emerge_area(minp, maxp, function(_, _, remaining)
			if remaining > 0 then return end
			local vm = core.get_voxel_manip(minp, maxp)
			local e1, e2 = vm:read_from_map(minp, maxp)
			local area, data = VoxelArea(e1, e2), vm:get_data()
			-- the edges' trees lean in from chunks outside the area: skip a margin
			for z = minp.z + 12, maxp.z - 12 do
				for x = minp.x + 12, maxp.x - 12 do
					-- a column that's no place on the planet (seam between
					-- storage, cut away) has no land to compare
					if not core.planet.atlas_place({ x = x, y = 0, z = z }) then
						totals.seam = totals.seam + 1
					else
					local s = core.terra.surface({ x = x, y = 0, z = z })
					local r = real(data, area, x, z, maxp.y, minp.y)
					-- a tree's trunk stands in its column's top block: on a planet
					-- surface plants no trees, so that column can't be told
					local rooted = kind(core.get_name_from_content_id(data[area:index(x, s.ground, z)])) == "tree"
					if rooted then
						totals.rooted = totals.rooted + 1
					else
					totals.columns = totals.columns + 1
					if r.ground and r.ground.y == s.ground and r.ground.name == s.top then
						totals.ground = totals.ground + 1
					else
						local around = {}
						for y = s.ground + 2, s.ground - 1, -1 do
							around[#around + 1] = y .. "=" .. core.get_name_from_content_id(data[area:index(x, y, z)])
						end
						miss("ground", x, z, s.ground .. " " .. s.top, (r.ground and (r.ground.y .. " " .. r.ground.name) or "none")
							.. " [" .. table.concat(around, " ") .. "]")
					end
					local sw = s.water and (s.water .. " " .. s.liquid) or "none"
					local rw = r.water and (r.water.y .. " " .. r.water.name) or "none"
					-- water that has flowed since it was generated is the world's doing, not the generator's
					if r.water and r.water.name:find("_flowing$") then
						totals.water = totals.water + 1
					elseif sw == rw then totals.water = totals.water + 1 else miss("water", x, z, sw, rw) end
					end
					end
				end
			end
			pending = pending - 1
			if pending == 0 then
				print(("FAR columns %d ground %d water %d rooted %d seam %d"):format(totals.columns, totals.ground, totals.water, totals.rooted, totals.seam))
				core.request_shutdown()
			end
		end)
	end
end)
