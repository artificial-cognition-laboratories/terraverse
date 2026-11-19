// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

/*
	A player's weight on the planet: gravity weakens as they climb, by the
	inverse square, R² / (R + h)², so high enough they hardly fall. It was
	tf_flight's, in Lua, until sprint 119: physics is the engine's, and a mod
	never runs per player, per tick. Only the gravity is the engine's to set;
	speed and jump are left to whatever tunes them (sprinting, a heavy load).
	Worlds terra didn't make have no planet to climb.
*/

#include "server/terra.h"
#include "server.h"
#include "emerge.h"
#include "mapgen/mapgen.h"
#include "orbit/flight.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include "serverenvironment.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {

const double RETUNE = 0.01; // the share gravity changes by before the body is told again

} // namespace

void terraStepGravity(Server *server)
{
	ServerTerra &t = server->terra();
	const MapgenParams *params = server->getEmergeManager()->mgparams;
	if (!params || params->mgtype != MAPGEN_TERRA)
		return;
	const double radius = server->flight().planet().radius;
	std::unordered_set<std::string> here;
	for (RemotePlayer *player : server->getEnv().getPlayers()) {
		PlayerSAO *sao = player->getPlayerSAO();
		if (!sao || player->getPeerId() == PEER_ID_INEXISTENT)
			continue;
		const std::string name = player->getName();
		here.insert(name);
		// travelling, they're held still: the landing gives them their weight back
		if (t.travelling.count(name))
			continue;
		const double altitude = std::max(sao->getBasePosition().Y / BS - params->water_level, 0.0f);
		const double g = std::pow(radius / (radius + altitude), 2);
		auto worn = t.gravity.find(name);
		if (worn != t.gravity.end() && std::abs(worn->second - g) <= RETUNE * std::max(g, RETUNE))
			continue;
		t.gravity[name] = g;
		player->physics_override.gravity = (float)g;
		sao->m_physics_override_sent = false;
	}
	for (auto it = t.gravity.begin(); it != t.gravity.end();)
		it = here.count(it->first) ? std::next(it) : t.gravity.erase(it);
}
