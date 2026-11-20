// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

/*
	Standing on the planet (an atlas world): where players start, getting out
	of the seam, and travel between its bodies. They were tf_planet's, in Lua,
	until sprint 119: each is the planet's own mechanism, and a mod only
	decides when to use one (the /travel command is policy; landing isn't).

	The seam is every node that is no cell, between the planet's regions: a
	wall nothing passes, never drawn (builtin/game/planet.lua registers it).
	Nobody belongs there, so one found in it (saved there, or a client's own
	position sent before the server placed it) starts again at the spawn,
	before anything else asks the planet where they are.
*/

#include "src/server/world/terra.h"
#include "src/server/app/server.h"
#include "src/server/world/load/emerge.h"
#include "src/shared/platform/system/log.h"
#include "src/server/world/state/servermap.h"
#include "src/shared/map/block/mapblock.h"
#include "src/server/world/generate/mapgen.h"
#include "library/world/planet/atlas.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "library/world/terragen/src/system/system.h"
#include "library/world/terragen/src/code/worlds.h"
#include <json/json.h>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {

const float HOLD = 1.5f; // s on the travel screen after arriving, for the blocks round them to reach the client
const s16 REACH = 32;    // nodes round a landing made before it

// The node of the cell at a place on a body, `layer` up; none above its atlas.
std::optional<v3s16> nodeAt(Server *server, int body, double lat, double lon, s32 layer)
{
	const terragen::Atlas &atlas = server->atlas()->body(body);
	if (layer >= atlas.params().height)
		return std::nullopt;
	const double r = atlas.params().radius + layer + 0.5;
	if (r <= 0)
		return std::nullopt;
	const planet::Atlas::Cell c(body, atlas.locate({ r * std::cos(lat) * std::cos(lon), r * std::sin(lat),
			r * std::cos(lat) * std::sin(lon) }));
	const planet::Atlas::Node n = server->planetNode(c); // its tile put in the window
	return v3s16(n.x, n.y, n.z);
}

// Where one stands at a place (degrees): none where no one spawns (the sea, a lake, a river).
std::optional<v3s16> standAt(Server *server, int body, double lat, double lon)
{
	const s16 level = server->getEmergeManager()->getSpawnLevelAt(body, lat * M_PI / 180, lon * M_PI / 180);
	if (level >= MAX_MAP_GENERATION_LIMIT)
		return std::nullopt;
	return nodeAt(server, body, lat * M_PI / 180, lon * M_PI / 180, level + 1);
}

// On land, nearest the equator, from `west` east to `east`. Searched on the half degree:
// whole degrees put it on the equator (the tiles' edge row) and on every 15° (the
// segments' edges, whose columns narrow with depth).
std::optional<v3s16> search(Server *server, int body, double west, double east)
{
	for (int band = 0; band <= 60; band += 5) {
		const std::vector<double> lats = band == 0 ? std::vector<double>{ 0.5 } :
				std::vector<double>{ band + 0.5, -band - 0.5 };
		for (double lat : lats)
			for (double lon = west; lon <= east; lon += 2)
				if (auto n = standAt(server, body, lat, lon))
					return n;
	}
	return std::nullopt;
}

PlayerPhysicsOverride held(PlayerPhysicsOverride p)
{
	p.speed = 0;
	p.jump = 0;
	p.gravity = 0;
	return p;
}

} // namespace

int terraBody(Server *server, const std::string &id)
{
	if (!server->atlas())
		return -1;
	const std::vector<terragen::BodyWorld> &worlds = server->bodyWorlds();
	for (size_t b = 0; b < worlds.size(); b++)
		if (worlds[b].id == id)
			return (int)b;
	return -1;
}

std::optional<v3s16> terraLanding(Server *server, int body, std::optional<std::pair<double, double>> near)
{
	if (!near) {
		// a body that faces its star starts on the ring between day and night, on either
		// side (longitude 0 is under the star), if the ring has land at all
		if (server->bodyWorlds().at(body).code.processes.facing_star) {
			if (auto n = search(server, body, 72.5, 90.5))
				return n;
			if (auto n = search(server, body, 270.5, 288.5))
				return n;
		}
		return search(server, body, 0.5, 359.5);
	}
	// the place itself, else the nearest in rings round it, a degree apart, out to 30°
	const auto [lat, lon] = *near;
	for (int ring = 0; ring <= 30; ring++) {
		for (int step = 0; step < std::max(1, ring * 8); step++) {
			const double a = ring == 0 ? 0 : (double)step / (ring * 8) * 2 * M_PI;
			const double la = std::clamp(lat + ring * std::sin(a), -85.0, 85.0);
			if (auto n = standAt(server, body, la, lon + ring * std::cos(a)))
				return n;
		}
	}
	return std::nullopt;
}

v3f terraSpawn(Server *server)
{
	const std::optional<v3s16> n = terraLanding(server, planet::Atlas::HOME, std::nullopt);
	if (!n)
		throw BaseException("the planet has no land within 60° of the equator to spawn on");
	return intToFloat(*n, BS);
}

bool terraSeam(Server *server, const v3f &nodes)
{
	const planet::Atlas *atlas = server->atlas();
	if (!atlas)
		return false;
	const std::optional<planet::Atlas::Node> from = atlas->charted(planet::Atlas::moverNode(nodes.X, nodes.Y, nodes.Z));
	return !from || !atlas->point(*from, nodes.X, nodes.Y, nodes.Z);
}

void terraOffSeam(Server *server, PlayerSAO *sao, const char *how)
{
	if (!server->atlas())
		return;
	const v3f pos = sao->getBasePosition() / BS;
	if (!terraSeam(server, pos))
		return;
	warningstream << "planet: " << sao->getPlayer()->getName() << " " << how << " in seam at " <<
			floatToInt(sao->getBasePosition(), BS) << ": moved to the spawn" << std::endl;
	sao->setPos(terraSpawn(server));
}

std::string terraTravel(Server *server, RemotePlayer *player, const std::string &id,
		std::optional<std::pair<double, double>> near)
{
	ServerTerra &t = server->terra();
	PlayerSAO *sao = player->getPlayerSAO();
	if (!sao)
		return "not in the world";
	if (t.travelling.count(player->getName()))
		return "already on the way";
	if (!server->atlas())
		return "this world has no other bodies";
	const int body = terraBody(server, id);
	const std::vector<terragen::BodyWorld> &worlds = server->bodyWorlds();
	if (body < 0) {
		// every body at the home's scale: on a small world the others are too small to stand on
		if (worlds.size() == 1)
			return "no body called " + id + ": this world is too small for any other (a moon needs a planet 10k round or more)";
		std::string ids;
		for (const terragen::BodyWorld &w : worlds)
			ids += (ids.empty() ? "" : ", ") + w.id;
		return "no body called " + id + "; this world has: " + ids;
	}
	const std::string name = terraBodyName(server, body);
	const std::optional<v3s16> node = terraLanding(server, body, near);
	if (!node)
		return "no land on " + name + " within 30° of there";

	ServerTerra::Travel travel;
	travel.had = player->physics_override;
	travel.node = *node;
	travel.body = id;
	t.travelling[player->getName()] = travel;
	player->physics_override = held(player->physics_override);
	sao->m_physics_override_sent = false;
	Json::Value ui;
	ui["body"] = id;
	ui["name"] = name;
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	server->setUi(player, "travel", Json::writeString(writer, ui));
	return "";
}

std::optional<v3s16> terraGround(Server *server, int body, double lat, double lon)
{
	if (!server->atlas())
		return std::nullopt;
	const s16 level = server->getEmergeManager()->getSpawnLevelAt(body, lat, lon);
	// over water, its surface
	const s32 layer = level < MAX_MAP_GENERATION_LIMIT ? level + 1 :
			server->getEmergeManager()->mgparams->water_level + 1;
	return nodeAt(server, body, lat, lon, layer);
}

std::string terraBodyName(Server *server, int body)
{
	const std::vector<terragen::BodyWorld> &worlds = server->bodyWorlds();
	const terragen::System system(terragen::SystemSpec::forWorld(worlds.at(planet::Atlas::HOME).code));
	return system.spec().bodies.at(worlds.at(body).system).name;
}

std::vector<std::string> terraBodyIds(Server *server)
{
	std::vector<std::string> ids;
	if (server->atlas())
		for (const terragen::BodyWorld &w : server->bodyWorlds())
			ids.push_back(w.id);
	return ids;
}

void terraStepPlanet(Server *server, float dtime)
{
	if (!server->atlas())
		return;
	ServerTerra &t = server->terra();
	ServerMap &map = server->getEnv().getServerMap();
	EmergeManager *emerge = server->getEmergeManager();
	std::unordered_set<std::string> here;
	for (RemotePlayer *player : server->getEnv().getPlayers()) {
		PlayerSAO *sao = player->getPlayerSAO();
		if (!sao || player->getPeerId() == PEER_ID_INEXISTENT)
			continue;
		const std::string &name = player->getName();
		here.insert(name);
		auto it = t.travelling.find(name);
		if (it == t.travelling.end()) {
			terraOffSeam(server, sao, "was");
			continue;
		}
		ServerTerra::Travel &travel = it->second;
		if (travel.hold < 0) {
			// held on the screen while the land there is made, then put on it
			const v3s16 lo = getNodeBlockPos(travel.node - v3s16(REACH, REACH, REACH));
			const v3s16 hi = getNodeBlockPos(travel.node + v3s16(REACH, REACH, REACH));
			bool made = true;
			for (s16 x = lo.X; x <= hi.X; x++)
			for (s16 y = lo.Y; y <= hi.Y; y++)
			for (s16 z = lo.Z; z <= hi.Z; z++) {
				const v3s16 b(x, y, z);
				MapBlock *block = map.getBlockNoCreateNoEx(b);
				if (block && block->isGenerated())
					continue;
				made = false;
				if (!emerge->isBlockInQueue(b))
					emerge->enqueueBlockEmerge(PEER_ID_INEXISTENT, b, true);
			}
			if (!made)
				continue;
			sao->setPos(intToFloat(travel.node, BS));
			actionstream << "planet: " << name << " travelled to " << travel.body << " at " << travel.node << std::endl;
			travel.hold = HOLD;
			continue;
		}
		if ((travel.hold -= dtime) > 0)
			continue;
		player->physics_override = travel.had;
		sao->m_physics_override_sent = false;
		server->setUi(player, "travel", "null");
		t.travelling.erase(it);
	}
	for (auto it = t.travelling.begin(); it != t.travelling.end();)
		it = here.count(it->first) ? std::next(it) : t.travelling.erase(it);
}
