-- The water meets the shore (sprint 81). Loose ground beside water is
-- smoothed into slopes, so the top block at the water's edge is partly
-- open; the generator only places water in whole blocks, which left a gap
-- along every beach and bank. Water lies in that open space as a fill
-- (core.matter.set_fill), full to the block's top, asleep like the sea.

local WATER = core.get_content_id("tf_world:water_source")
-- the sides and the corners: a shore's corner block meets the sea only
-- diagonally (sprint 82; the generator's own finding, "shore", looks at them too)
local SIDES = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } }

local loose = {}
core.register_on_mods_loaded(function()
	for name, def in pairs(core.registered_nodes) do
		if def.groups and (def.groups.loose or 0) > 0 then loose[core.get_content_id(name)] = true end
	end
end)

core.register_on_generated(function()
	local filled = core.get_mapgen_object("shore")
	-- after the generator writes its blocks: a fill is kept beside them
	if #filled > 0 then
		core.after(0, function()
			-- only where it's open (smoothed): a whole block of sand under the sea holds none
			for _, p in ipairs(filled) do
				if core.matter.open(p) > 0 then core.matter.set_fill(p, "water", 1) end
			end
		end)
	end
end)

-- Shores generated before this (sprint 82): once per block, each block of
-- water looks at the loose ground beside it and under it
local function wet(p)
	if not loose[core.get_content_id(core.get_node(p).name)] then return false end
	local above = core.get_node(vector.offset(p, 0, 1, 0)).name
	return not loose[core.get_content_id(above)] and not core.matter.get_fill(p) and core.matter.open(p) > 0
end

core.register_lbm({
	name = "tf_world:shore_fills_2", -- (the first missed corners and the sea floor)
	nodenames = { "tf_world:water_source" },
	run_at_every_load = false,
	bulk_action = function(list)
		for _, p in ipairs(list) do
			local floor = vector.offset(p, 0, -1, 0)
			if wet(floor) then core.matter.set_fill(floor, "water", 1) end
			if core.get_node(vector.offset(p, 0, 1, 0)).name ~= "tf_world:water_source" then
				for _, d in ipairs(SIDES) do
					local q = vector.offset(p, d[1], 0, d[2])
					if wet(q) then core.matter.set_fill(q, "water", 1) end
				end
			end
		end
	end,
})
