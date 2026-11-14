-- The rivers as the weather fills them (sprint 115), by hand: /river reads
-- the one you stand in, /rivers brings a storm or a drought over a region
-- so you can watch it answer. The weather drives them; this only stands in
-- for it for a while (core.terra.force_rivers).

tf_weather.STORM = 8 -- mm/h a storm brings: a day of it is a big one (about 200 mm)

-- What a river at pos is doing, as a line, or nil if there's none there.
function tf_weather.river_line(pos)
	local r = core.terra.river(pos)
	if not r then return nil end
	if r.lake then return "a lake: it stands at its level" end
	return string.format("%.0f m³/s (%.1f in its channel here) · ×%.2f its usual · %+.2f blocks · %.2f blocks/s · %.1f days to the sea",
		r.discharge, r["local"], r.ratio, r.rise, r.speed, r.to_sea)
end

core.register_chatcommand("river", {
	description = "What the river you stand in is doing: its flow, against its usual, its level and speed",
	func = function(name)
		local player = core.get_player_by_name(name)
		if not player then return false, "not in the world" end
		return true, tf_weather.river_line(player:get_pos()) or "no river or lake here"
	end,
})

local KINDS = { storm = tf_weather.STORM, drought = 0 }

core.register_chatcommand("rivers", {
	params = "storm|drought [<hours> [<km>]] | clear",
	description = "Bring a storm or a drought over the rivers round you, in place of the weather (default 24 h, 200 km)",
	privs = { server = true },
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then return false, "not in the world" end
		local kind, hours, km = param:match("^(%a+)%s*(%S*)%s*(%S*)$")
		if kind == "clear" then
			core.terra.force_rivers(player:get_pos(), 0, 0, 0)
			return true, "the weather has the rivers again"
		end
		if not kind or not KINDS[kind] then return false, "/rivers storm|drought [hours [km]] or /rivers clear" end
		hours, km = tonumber(hours) or 24, tonumber(km) or 200
		if hours <= 0 or km <= 0 then return false, "hours and km must be over 0" end
		core.terra.force_rivers(player:get_pos(), km, hours, KINDS[kind])
		return true, string.format("a %s over %g km for %g h: the rivers answer at the weather's next step (3 h)",
			kind, km, hours)
	end,
})
