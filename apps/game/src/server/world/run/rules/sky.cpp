// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

/*
	Each player's sky, kept told by the engine (it was tf_sky's, in Lua, until
	sprint 119: a mod never runs per player, per tick). The sky follows the real
	sun for where they stand and the day of the year (terraSun); the engine
	places the sun, moon and stars and lights the world from it.

	On a planet (an atlas world) the stock sky is off (sprint 60): no dome, sun,
	moon, stars or clouds. The client draws space there itself, the system seen
	from where the player is (planetarium/sky), and the far view the planet. The
	real sun is still sent: it lights the world. Elsewhere on a terra world the
	stock sky's blue fades to black as the air above thins, and the stars come
	out by day. Worlds terra didn't make keep the stock sky.
*/

#include "src/server/world/terra.h"
#include "src/server/app/server.h"
#include "src/server/world/load/emerge.h"
#include "src/server/world/generate/mapgen.h"
#include "library/space/orbit/flight.h"
#include "src/shared/objects/remoteplayer.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "src/shared/environment/skyparams.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {

const float PERIOD = 0.5f;     // s between looks
const double CHANGE = 0.02;    // degrees of latitude or declination worth sending
const double CLOCK = 0.0001;   // turns of a body's clock worth sending
const double THIN = 0.01;      // a change in how thin the air is worth sending
const double CLOUDS = 12000;   // m above the sea: above them, they're gone
const video::SColor SPACE(255, 0, 0, 4);

// The stock sky's colour, taken toward space's black as the air thins (k: 1 the sea's air, 0 none).
video::SColor thinned(video::SColor c, double k)
{
	auto mix = [k](u32 from, u32 to) { return (u32)std::floor(from * k + to * (1 - k)); };
	return video::SColor(255, mix(c.getRed(), SPACE.getRed()), mix(c.getGreen(), SPACE.getGreen()),
			mix(c.getBlue(), SPACE.getBlue()));
}

bool moved(const ServerTerra::SkySent &a, const ServerTerra::SkySent &b)
{
	return std::abs(a.latitude - b.latitude) >= CHANGE || std::abs(a.declination - b.declination) >= CHANGE ||
			std::abs(a.thin - b.thin) >= THIN || a.turns != b.turns || std::abs(a.at - b.at) >= CLOCK;
}

void send(Server *server, RemotePlayer *player, const TerraSun &sun, double thin, double altitude)
{
	SkyboxParams sky = player->getSkyParams();
	sky.solar.on = true;
	sky.solar.latitude = (float)sun.latitude;
	sky.solar.declination = (float)sun.declination;
	sky.solar.turns = (float)sun.turns;
	sky.solar.at = (float)sun.at;
	if (server->atlas()) {
		sky.type = "plain";
		sky.bgcolor = video::SColor(255, 0, 0, 0);
		sky.clouds = false;
		server->setSky(player, sky);
		SunParams sun_params = player->getSunParams();
		sun_params.visible = false;
		sun_params.sunrise_visible = false;
		server->setSun(player, sun_params);
		MoonParams moon = player->getMoonParams();
		moon.visible = false;
		server->setMoon(player, moon);
		StarParams stars = player->getStarParams();
		stars.visible = false;
		server->setStars(player, stars);
		return;
	}
	const double blue = std::sqrt(thin); // the sky darkens early: a third of the air is already a deep blue
	const SkyColor stock = SkyboxDefaults::getSkyColorDefaults();
	sky.type = "regular";
	sky.clouds = altitude < CLOUDS;
	sky.fog_tint_type = "default";
	sky.sky_color.day_sky = thinned(stock.day_sky, blue);
	sky.sky_color.day_horizon = thinned(stock.day_horizon, blue);
	sky.sky_color.dawn_sky = thinned(stock.dawn_sky, blue);
	sky.sky_color.dawn_horizon = thinned(stock.dawn_horizon, blue);
	sky.sky_color.night_sky = thinned(stock.night_sky, blue);
	sky.sky_color.night_horizon = thinned(stock.night_horizon, blue);
	server->setSky(player, sky);
	StarParams stars = player->getStarParams();
	stars.day_opacity = (f32)(1 - blue);
	server->setStars(player, stars);
}

} // namespace

void terraStepSkies(Server *server, float dtime)
{
	ServerTerra &t = server->terra();
	const MapgenParams *params = server->getEmergeManager()->mgparams;
	if (!params || params->mgtype != MAPGEN_TERRA)
		return;
	t.skies_timer -= dtime;
	const bool look = t.skies_timer <= 0;
	if (look)
		t.skies_timer = PERIOD;
	const orbit::Flight &flight = server->flight();
	std::unordered_set<std::string> here;
	for (RemotePlayer *player : server->getEnv().getPlayers()) {
		PlayerSAO *sao = player->getPlayerSAO();
		if (!sao || player->getPeerId() == PEER_ID_INEXISTENT)
			continue;
		const std::string name = player->getName();
		here.insert(name);
		auto last = t.skies.find(name);
		if (!look && last != t.skies.end())
			continue; // someone new is told at once; the rest every PERIOD
		const v3f pos = sao->getBasePosition() / BS;
		const std::optional<TerraSun> sun = terraSun(server, pos);
		if (!sun)
			continue; // on seam, before they're placed: the next look
		const double altitude = pos.Y - params->water_level;
		// the air left above, against the sea's: its sight of the sky
		const double thin = flight.density(std::max(altitude, 0.0)) / flight.density(0);
		const ServerTerra::SkySent now{ sun->latitude, sun->declination, thin, sun->turns, sun->at };
		if (last != t.skies.end() && !moved(last->second, now))
			continue;
		send(server, player, *sun, thin, altitude);
		t.skies[name] = now;
	}
	for (auto it = t.skies.begin(); it != t.skies.end();)
		it = here.count(it->first) ? std::next(it) : t.skies.erase(it);
}
