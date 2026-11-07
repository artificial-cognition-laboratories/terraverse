-- Felling: a trunk cut through brings its tree down, logs and leaves, and
-- what it gives lands in stacks at the stump. A tree is the trunk joined to
-- the cut above it (diagonals too, for branches), no further out than a
-- branch reaches, with leaves on it, so a wall of placed logs stays a wall and
-- the next tree's crown stays up; crouched, only the one block's taken.
local MAX_LOGS = 256 -- the largest tree there is, and then some
local SPREAD = 4 -- nodes out from the cut a branch may reach
local REACH = 3 -- leaves this far from a log of the tree come down with it

local function neighbours(p)
	local out = {}
	for dy = 0, 1 do -- (never below: the stump and the roots stay)
		for dx = -1, 1 do
			for dz = -1, 1 do
				if dx ~= 0 or dy ~= 0 or dz ~= 0 then out[#out + 1] = { x = p.x + dx, y = p.y + dy, z = p.z + dz } end
			end
		end
	end
	return out
end

local function group(name, g) return core.get_item_group(name, g) > 0 end

-- The trunk above the cut, of the kind cut: positions (and a set of them by
-- hash), or nil past MAX_LOGS
local function trunk(cut, kind)
	local logs, mine, seen, queue = {}, {}, { [core.hash_node_position(cut)] = true }, { cut }
	while #queue > 0 do
		local p = table.remove(queue)
		for _, n in ipairs(neighbours(p)) do
			local h = core.hash_node_position(n)
			if not seen[h] and math.abs(n.x - cut.x) <= SPREAD and math.abs(n.z - cut.z) <= SPREAD then
				seen[h] = true
				if core.get_node(n).name == kind then
					logs[#logs + 1] = n
					mine[h] = true
					if #logs > MAX_LOGS then return nil end
					queue[#queue + 1] = n
				end
			end
		end
	end
	return logs, mine
end

-- The leaves about those logs, but not those as near another tree's
local function crown(logs, mine)
	local leaves, seen = {}, {}
	for _, p in ipairs(logs) do
		for _, l in ipairs(core.find_nodes_in_area(vector.subtract(p, REACH), vector.add(p, REACH), { "group:leaves" })) do
			local h = core.hash_node_position(l)
			if not seen[h] then
				seen[h] = true
				local other = false
				for _, t in ipairs(core.find_nodes_in_area(vector.subtract(l, REACH), vector.add(l, REACH), { "group:tree" })) do
					local th = core.hash_node_position(t)
					if not mine[th] and vector.distance(t, l) <= vector.distance(p, l) then other = true break end
				end
				if not other then leaves[#leaves + 1] = l end
			end
		end
	end
	return leaves
end

-- Drops as whole stacks, each as full as it'll go
local function stacked(drops)
	local total, order = {}, {}
	for _, d in ipairs(drops) do
		local s = ItemStack(d)
		if not total[s:get_name()] then order[#order + 1] = s:get_name() end
		total[s:get_name()] = (total[s:get_name()] or 0) + s:get_count()
	end
	local out = {}
	for _, name in ipairs(order) do
		local max = ItemStack(name):get_stack_max()
		local left = total[name]
		while left > 0 do
			out[#out + 1] = name .. " " .. math.min(left, max)
			left = left - max
		end
	end
	return out
end

core.register_on_dignode(function(pos, oldnode, digger)
	if not digger or not digger:is_player() or not group(oldnode.name, "tree") then return end
	if digger:get_player_control().sneak then return end
	local logs, mine = trunk(pos, oldnode.name)
	if not logs then return end
	mine[core.hash_node_position(pos)] = true
	-- a log by itself, a cut log or one in a wall: no leaves, no tree
	local leaves = crown(#logs > 0 and logs or { pos }, mine)
	if #leaves == 0 then return end
	local tool = digger:get_wielded_item()
	local drops = {}
	for _, list in ipairs({ logs, leaves }) do
		for _, p in ipairs(list) do
			local node = core.get_node(p)
			core.remove_node(p)
			for _, d in ipairs(core.get_node_drops(node, tool:get_name(), tool, digger, p)) do
				-- a crown gives what's in it (sticks), not armfuls of leaves
				if list == logs or ItemStack(d):get_name() ~= node.name then drops[#drops + 1] = d end
			end
		end
	end
	core.handle_node_drops(pos, stacked(drops), digger)
	-- the tool wears for every log it went through, as if each were cut
	local def = tool:get_definition()
	if def and def.tool_capabilities and #logs > 0 and not core.is_creative_enabled(digger:get_player_name()) then
		local dig = core.get_dig_params(core.registered_nodes[oldnode.name].groups, def.tool_capabilities, tool:get_wear())
		tool:add_wear(dig.wear * #logs)
		digger:set_wielded_item(tool)
	end
end)
