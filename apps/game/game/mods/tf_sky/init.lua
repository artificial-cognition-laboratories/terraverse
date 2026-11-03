-- Each player's sky follows the real sun for where they are and the day of
-- the year (core.terra.sun), and the air above them: blue going to black as
-- they climb out of it. The engine places the sun, moon and stars and
-- lights the world from it; this only keeps it told. Worlds terra didn't
-- generate (the Lab) have no real sun to follow and keep the stock sky; the
-- Lab shows the solar sky through its own scenarios (lab.lua).
--
-- On a planet (an atlas world) the stock sky is off (sprint 60): no dome,
-- sun, moon, stars or clouds. The client draws space there itself, the
-- system seen from where the player is (planetarium/sky), and the far view
-- the planet. The real sun is still sent: it lights the world. The air's
-- blue comes back with the atmosphere.

dofile(core.get_modpath("tf_sky") .. "/lab.lua") -- its Lab scenario

if not core.terra.enabled() then
	return
end

local PERIOD = 0.5  -- seconds between checks
local CHANGE = 0.02 -- degrees of latitude or declination worth sending
local CLOCK = 0.0001 -- turns of a body's clock worth sending
local THIN = 0.01   -- a change in how thin the air is worth sending

-- The stock sky's colours, which the air's thinning takes to black.
local BLUE = {
	day_sky = { 0x61, 0xb5, 0xf5 }, day_horizon = { 0x90, 0xd3, 0xf6 },
	dawn_sky = { 0xb4, 0xba, 0xfa }, dawn_horizon = { 0xba, 0xc1, 0xf0 },
	night_sky = { 0x00, 0x6b, 0xff }, night_horizon = { 0x40, 0x90, 0xff },
}
local SPACE = { 0, 0, 4 }
local CLOUDS = 12000 -- m above the sea: above them, they're gone

local PLANET = (core.get_mapgen_setting("planet_atlas") or "") ~= ""

local sent = {} -- player name → the sky they have: { latitude, declination, thin }

local function hex(c, k)
	return string.format("#%02x%02x%02x", math.floor(c[1] * k + SPACE[1] * (1 - k)),
		math.floor(c[2] * k + SPACE[2] * (1 - k)), math.floor(c[3] * k + SPACE[3] * (1 - k)))
end

-- The sky over where a player is: the real sun there, and the air's blue
-- fading to black as it thins, the stars coming out by day.
local SEA = tonumber(core.get_mapgen_setting("water_level")) or error("tf_sky: no water_level")
local function update(player)
	local name = player:get_player_name()
	local pos = player:get_pos()
	local at = { x = pos.x, z = pos.z, altitude = pos.y - SEA }
	local sun = core.terra.sun(pos) -- where the player stands (on a planet, layer 0 under them can be seam)
	-- the air left above, against the sea's (its sight of the sky)
	local thin = core.orbit.density(math.max(at.altitude, 0)) / core.orbit.density(0)
	local last = sent[name]
	if last and math.abs(last.latitude - sun.latitude) < CHANGE
			and math.abs(last.declination - sun.declination) < CHANGE and math.abs(last.thin - thin) < THIN
			and last.turns == sun.turns and math.abs(last.at - sun.at) < CLOCK then
		return
	end
	if PLANET then
		player:set_sky({
			type = "plain",
			base_color = "#000000",
			clouds = false,
			solar = { latitude = sun.latitude, declination = sun.declination, turns = sun.turns, at = sun.at },
		})
		player:set_sun({ visible = false, sunrise_visible = false })
		player:set_moon({ visible = false })
		player:set_stars({ visible = false })
	else
		local blue = math.sqrt(thin) -- the sky darkens early: a third of the air is already a deep blue
		local colours = {}
		for k, c in pairs(BLUE) do colours[k] = hex(c, blue) end
		colours.fog_tint_type = "default"
		player:set_sky({
			type = "regular",
			clouds = at.altitude < CLOUDS,
			solar = { latitude = sun.latitude, declination = sun.declination, turns = sun.turns, at = sun.at },
			sky_color = colours,
		})
		player:set_stars({ day_opacity = 1 - blue })
	end
	sent[name] = { latitude = sun.latitude, declination = sun.declination, thin = thin, turns = sun.turns, at = sun.at }
end

core.register_on_joinplayer(update)

core.register_on_leaveplayer(function(player)
	sent[player:get_player_name()] = nil
end)

local elapsed = 0
core.register_globalstep(function(dtime)
	elapsed = elapsed + dtime
	if elapsed < PERIOD then
		return
	end
	elapsed = 0
	for _, player in ipairs(core.get_connected_players()) do
		update(player)
	end
end)
