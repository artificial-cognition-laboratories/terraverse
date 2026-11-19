-- Types the fixture's /check at the console, once before the sidecar is crashed and once
-- after it restarts: its commands must answer both times.
local function typed(param)
	local reply = core.console_command("check " .. param)
	core.log("action", "PROBE /check " .. param .. " → " .. reply)
end
core.after(1, function() typed("41 words after it") typed("x y") end)
core.after(5, function() typed("1 again") end)
