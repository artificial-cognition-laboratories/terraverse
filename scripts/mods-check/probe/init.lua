-- Types the fixture's /check from the console (player ""), once before the sidecar is
-- crashed and once after it restarts: its commands must answer both times.
local function typed(param)
	local def = core.registered_chatcommands.check
	if not def then
		core.log("action", "PROBE no /check registered")
		return
	end
	local ok, message = def.func("", param)
	core.log("action", "PROBE /check " .. param .. " → " .. tostring(ok) .. (message and (" " .. message) or ""))
end
core.after(1, function() typed("41 words after it") typed("x y") end)
core.after(5, function() typed("1 again") end)
