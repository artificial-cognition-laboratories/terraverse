-- What the generator places is terra::AtlasLand's, node for node
-- (scripts/atlas-land-check.sh, planning/sprint-32-atlas-lod.md): areas
-- emerged where the atlas changes (a segment's middle, its edge and the seam
-- beside it, a cap and its circle, down by the core, the core itself), and
-- every node compared with core.terra.land, which the far view draws from.
-- The world may differ only by what came after the terrain: the game's
-- planting (trees, plants, snow) and liquid that has flowed. Prints LAND lines.

local atlas = assert(core.planet.atlas(), "atlas land probe: not an atlas world")
local SEAM = "core:seam"
local function at(latDeg, lonDeg, layer)
	return assert(core.planet.atlas_node(math.rad(latDeg), math.rad(lonDeg), layer), "atlas land probe: no node there")
end
local function box(name, p, below, above, half)
	half = half or 20
	return { name = name, min = { x = p.x - half, y = p.y - below, z = p.z - half },
		max = { x = p.x + half - 1, y = p.y + above, z = p.z + half - 1 } }
end
local segment, inner
for _, r in ipairs(atlas.tiles) do
	if r.kind == "segment" and not segment then segment = r end
	if r.kind == "core" then inner = r end
end
assert(segment and inner, "atlas land probe: no segment or core")
local boxes = {
	box("a segment's middle, the surface", at(0, 7.5, 0), 40, 60),
	box("a segment's east edge and the seam", { x = segment.x + segment.width, y = 0, z = segment.z + math.floor(segment.depth / 2) }, 30, 40),
	box("a cap's middle", at(89.5, 0, 0), 40, 60),
	box("across the cap circle", at(85, 40, 0), 30, 40),
	box("a segment down by the core", { x = segment.x + math.floor(segment.width / 2), y = segment.y0 + 10, z = segment.z + math.floor(segment.depth / 2) }, 10, 20),
	box("the core", { x = inner.x + math.floor(inner.width / 2), y = inner.y0 + math.floor((inner.y1 - inner.y0) / 2), z = inner.z + math.floor(inner.depth / 2) }, 16, 16, 16),
}

-- what the game does after the terrain
local function later(name)
	if name:find("_flowing$") then return "flowed" end
	local def = core.registered_nodes[name]
	if not def then return nil end
	local g = def.groups or {}
	if (g.tree or 0) > 0 or (g.leaves or 0) > 0 or (g.plant or 0) > 0 or (g.flora or 0) > 0 or (g.sapling or 0) > 0 then
		return "planted"
	end
	if name:find("snow") or def.drawtype == "plantlike" or def.drawtype == "nodebox" then return "planted" end
	return nil
end

local totals = { nodes = 0, same = 0, planted = 0, flowed = 0, wrong = 0 }
local pairs_wrong, shown = {}, 0
local pending = #boxes
core.after(0, function()
	for _, b in ipairs(boxes) do
		core.emerge_area(b.min, b.max, function(_, _, remaining)
			if remaining > 0 then return end
			local vm = core.get_voxel_manip(b.min, b.max)
			local e1, e2 = vm:read_from_map(b.min, b.max)
			local area, data = VoxelArea(e1, e2), vm:get_data()
			local land = core.terra.land(b.min, b.max)
			local mine = VoxelArea(b.min, b.max)
			local n = 0
			for z = b.min.z, b.max.z do
				for y = b.min.y, b.max.y do
					for x = b.min.x, b.max.x do
						local world = core.get_name_from_content_id(data[area:index(x, y, z)])
						local want = land[mine:index(x, y, z)]
						want = want == false and SEAM or want
						n = n + 1
						if world == want then
							totals.same = totals.same + 1
						else
							local why = later(world)
							if why then
								totals[why] = totals[why] + 1
							else
								totals.wrong = totals.wrong + 1
								local key = want .. " -> " .. world
								pairs_wrong[key] = (pairs_wrong[key] or 0) + 1
								if shown < 10 then
									print(("LAND wrong in %s at %d %d %d: land %s, world %s"):format(b.name, x, y, z, want, world))
									shown = shown + 1
								end
							end
						end
					end
				end
			end
			totals.nodes = totals.nodes + n
			print(("LAND box %s: %d nodes"):format(b.name, n))
			pending = pending - 1
			if pending == 0 then
				for k, v in pairs(pairs_wrong) do print(("LAND pair %s: %d"):format(k, v)) end
				print(("LAND nodes %d same %d planted %d flowed %d wrong %d"):format(
					totals.nodes, totals.same, totals.planted, totals.flowed, totals.wrong))
				core.request_shutdown()
			end
		end)
	end
end)
