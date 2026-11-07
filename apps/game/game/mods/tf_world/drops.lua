-- What's broken falls where it was, as items on the ground to be picked up,
-- never straight into the inventory; items lying close merge into stacks
-- (builtin/game/item_entity.lua). tf_matter wraps this, after.
function core.handle_node_drops(pos, drops)
	for _, item in ipairs(drops) do
		local at = vector.offset(pos, math.random() * 0.4 - 0.2, 0.1, math.random() * 0.4 - 0.2)
		local obj = core.add_item(at, item)
		if obj then obj:set_velocity({ x = math.random() - 0.5, y = 1.5, z = math.random() - 0.5 }) end
	end
end
