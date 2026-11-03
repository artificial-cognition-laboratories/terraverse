-- tf_world's Lab scenarios (data in lab.json): the engine's liquids and
-- falling blocks.

local WATER = { ["tf_world:water_source"] = true, ["tf_world:water_puddle"] = true }

tf_lab.load()

tf_lab.behaviour("water.spill", {
	build = function(area, values)
		area:fill({ x = 0, y = 0, z = 0 }, { x = 6, y = 1, z = 6 }, "tf_world:stone")
		area:fill({ x = 1, y = 0, z = 1 }, { x = 5, y = 1, z = 5 }, "tf_world:water_source")
		-- the gap, facing south (where the camera starts), centred
		local left = 3 - math.floor((values.gap - 1) / 2)
		area:fill({ x = left, y = 0, z = 0 }, { x = left + values.gap - 1, y = 1, z = 0 }, "air")
		-- built, not dug: the world's matter (which runs water, sprint 80) is told the gap opened
		for x = left, left + values.gap - 1 do
			for y = 0, 1 do core.matter.disturb(area:pos({ x = x, y = y, z = 0 })) end
		end
	end,
	expect = {
		after = 5,
		check = function(area)
			local outside = area:get({ x = 3, y = 0, z = -2 })
			-- (sprint 81: spilled water lies on the ground as a fill)
			local fill = core.matter.get_fill(area:pos({ x = 3, y = 0, z = -2 }))
			return WATER[outside] or (fill and fill.id == "water"), "two blocks past the gap is " .. outside
				.. (fill and (" holding " .. fill.id) or "")
		end,
	},
})

tf_lab.behaviour("falling.blocks", {
	build = function(area, values)
		local c = area:centre()
		area:fill({ x = c.x, y = 4, z = c.z }, { x = c.x, y = 6, z = c.z }, values.material)
		area:set({ x = c.x - 1, y = 5, z = c.z }, "tf_world:stone") -- stays: stone doesn't fall
		core.check_for_falling(area:pos({ x = c.x, y = 4, z = c.z }))
	end,
	expect = {
		after = 4,
		check = function(area, values)
			local c = area:centre()
			local bottom, top = area:get({ x = c.x, y = 0, z = c.z }), area:get({ x = c.x, y = 6, z = c.z })
			local stone = area:get({ x = c.x - 1, y = 5, z = c.z })
			return bottom == values.material and top == "air" and stone == "tf_world:stone",
				"floor " .. bottom .. ", top " .. top .. ", stone " .. stone
		end,
	},
})

tf_lab.behaviour("moves.walls", {
	build = function(area)
		area:fill({ x = 0, y = 0, z = 0 }, { x = 18, y = 0, z = 14 }, "tf_world:stone")
		-- a row of three heights, thin in front and thick behind
		for i, high in ipairs({ 1, 2, 3 }) do
			local x = (i - 1) * 6 + 1
			area:fill({ x = x, y = 1, z = 4 }, { x = x + 4, y = high, z = 4 }, "tf_world:stone")
			area:fill({ x = x, y = 1, z = 9 }, { x = x + 4, y = high, z = 12 }, "tf_world:stone")
		end
	end,
	expect = {
		after = 0,
		check = function(area)
			local top = area:get({ x = 13, y = 3, z = 4 })
			local over = area:get({ x = 13, y = 4, z = 4 })
			return top == "tf_world:stone" and over == "air", "the tall wall's top is " .. top .. ", above it " .. over
		end,
	},
})
