-- tf_craft's Lab scenario (data in lab.json): an axe made from what's found,
-- each step through the same offers the page shows.

tf_lab.load()

local FOUND = { "tf_craft:stone_flint 2", "tf_craft:stone_stone", "tf_craft:stick_wood", "tf_craft:fibre_fibre 3" }
local STEPS = { "flake_flint", "axe_head_flint", "handle_wood", "cord_fibre", "axe_flint" }

tf_lab.behaviour("making.axe", {
	build = function(area)
		local c = area:centre()
		area:fill({ x = 0, y = 0, z = 0 }, { x = 4, y = 0, z = 4 }, "tf_world:stone")
		area:set({ x = c.x, y = 1, z = c.z }, "tf_craft:bench_wood")
	end,
	expect = {
		after = 0,
		check = function(area)
			local c = area:centre()
			local station = core.registered_nodes[area:get({ x = c.x, y = 1, z = c.z })]._station
			if not station then return false, "no workbench at the centre" end
			local inv = core.create_detached_inventory("tf_craft_lab", {})
			inv:set_size("main", 16)
			for _, s in ipairs(FOUND) do inv:add_item("main", s) end
			-- granite's offered greyed, for the reason it can't be knapped
			local granite
			for _, o in ipairs(tf_craft.offers(inv:get_list("main"), station)) do
				if o.id == "axe_head_stone" then granite = o end
			end
			if not granite or granite.ok or not granite.reason:find("breaks clean") then
				return false, "a granite axe head isn't refused for not breaking clean"
			end
			for _, step in ipairs(STEPS) do
				local made, why = tf_craft.make({ { inv, "main" } }, station, step, { inv, "main" })
				if not made then return false, step .. ": " .. why end
			end
			core.remove_detached_inventory("tf_craft_lab")
			return true, "made a flint axe"
		end,
	},
})
