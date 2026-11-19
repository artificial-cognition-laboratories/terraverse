-- A fire pit, built and lit on a live server, run for simulated minutes:
-- clay floor and walls, leaves (tinder) under a log, the leaves lit by an
-- ember. Logs what the world's matter does, then stops the server.
local P = { x = 0, y = 10, z = 0 }
local function at(dx, dy, dz) return vector.offset(P, dx, dy, dz) end

core.register_on_mods_loaded(function()
	core.after(1, function()
		core.emerge_area(at(-3, -2, -3), at(3, 6, 3), function(_, _, left)
			if left > 0 then return end
			for x = -1, 1 do
				for z = -1, 1 do
					core.set_node(at(x, 0, z), { name = "core:clay" })
					for y = 1, 2 do
						if x ~= 0 or z ~= 0 then core.set_node(at(x, y, z), { name = "core:clay" }) end
					end
				end
			end
			core.set_node(at(0, 1, 0), { name = "core:leaves" })
			core.set_node(at(0, 2, 0), { name = "core:log" })
			local capacity = core.matter.capacity(at(0, 1, 0))
			core.log("action", string.format("MATTER capacity tinder %.0f log %.0f", capacity, core.matter.capacity(at(0, 2, 0))))
			core.matter.heat(at(0, 1, 0), 750) -- an ember
			local hottest, seen = 0, {}
			for second = 1, 1800 do
				core.matter.step(1)
				for _, a in ipairs(core.matter.awake(at(-3, -2, -3), at(3, 8, 3))) do
					hottest = math.max(hottest, a.temperature)
				end
				for _, h in ipairs(core.matter.happened()) do seen[h.reaction] = true end
				if second % 300 == 0 then
					core.log("action", string.format("MATTER t %d tinder %s log %s log_T %.0f awake %d hottest %.0f",
						second, core.get_node(at(0, 1, 0)).name, core.get_node(at(0, 2, 0)).name,
						core.matter.temperature(at(0, 2, 0)), core.matter.count(), hottest))
				end
			end
			local ids = {}
			for id in pairs(seen) do ids[#ids + 1] = id end
			table.sort(ids)
			core.log("action", "MATTER seen " .. table.concat(ids, ","))
			core.log("action", string.format("MATTER end tinder %s log %s hottest %.0f awake %d",
				core.get_node(at(0, 1, 0)).name, core.get_node(at(0, 2, 0)).name, hottest, core.matter.count()))
			-- a family block: peat (soil, its most organic point), heated, burns its organic part
			local peat = 0
			for i, p in ipairs(core.terra.soil_points()) do
				if p.organic >= 0.3 and p.clay < 0.25 then peat = i - 1 break end
			end
			local Q = at(6, 0, 0)
			core.set_node(Q, { name = "core:soil", param2 = peat })
			local before = core.matter.inspect(Q)
			core.matter.heat(Q, 900)
			local burned = false
			for _ = 1, 300 do
				core.matter.step(1)
				for _, h in ipairs(core.matter.happened()) do
					if vector.equals(h.pos, Q) and (h.reaction == "wood_burns" or h.reaction == "wood_chars") then burned = true end
				end
			end
			core.log("action", string.format("MATTER soil %s fill %.2f burned %s", before.substance or "none", before.fill, tostring(burned)))
			core.matter.save()
			core.request_shutdown("probe done")
		end)
	end)
end)
