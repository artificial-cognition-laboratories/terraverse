--
-- This file contains built-in stuff in Luanti implemented in Lua.
--
-- It is always loaded and executed after registration of the C API,
-- before loading and running any mods.
--

-- Initialize some very basic things
function core.error_handler(err, level)
	return debug.traceback(tostring(err), level)
end
do
	local function concat_args(...)
		local n, t = select("#", ...), {...}
		for i = 1, n do
			t[i] = tostring(t[i])
		end
		return table.concat(t, "\t")
	end
	function core.debug(...) core.log(concat_args(...)) end
	if core.print then
		local core_print = core.print
		-- Override native print and use
		-- terminal if that's turned on
		function print(...) core_print(concat_args(...)) end
		core.print = nil -- don't pollute our namespace
	end
end

do
	-- Note that PUC Lua just calls srand() which is already initialized by C++,
	-- but we don't want to rely on this implementation detail.
	local seed = 1048576 * (os.time() % 1048576)
	seed = seed + core.get_us_time() % 1048576
	math.randomseed(seed)
end

minetest = core

-- Load other files
local scriptdir = core.get_builtin_path()
local commonpath = scriptdir .. "common" .. DIR_DELIM
local asyncpath = scriptdir .. "async" .. DIR_DELIM

dofile(commonpath .. "math.lua")
dofile(commonpath .. "vector.lua")
dofile(commonpath .. "vector2.lua")
dofile(commonpath .. "strict.lua")
dofile(commonpath .. "serialize.lua")
dofile(commonpath .. "misc_helpers.lua")

if INIT == "game" then
	dofile(scriptdir .. "game" .. DIR_DELIM .. "init.lua")
	assert(not core.get_http_api)
elseif INIT == "async_game" then
	dofile(commonpath .. "metatable.lua")
	dofile(asyncpath .. "game.lua")
elseif INIT == "test" then
	-- TestScriptApi: the common helpers above, and nothing that needs a server
elseif INIT == "emerge" then
	dofile(scriptdir .. "emerge" .. DIR_DELIM .. "init.lua")
else
	error(("Unrecognized builtin initialization type %s!"):format(tostring(INIT)))
end
