// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

/*
	What a player is given, how they run, and what the page is told of where
	they are. It was tf_player's, in Lua, until sprint 119: each runs per player,
	and none of it is a game's rule.

	- Joining: a jointed body (client/figure.h), the inventory laid out as the
	  page draws it (a 2×2 crafting grid, nine columns), and the page told
	  whether they have the creative catalogue and the wiki.
	- Creative is the world's (creative_mode): there, a player can fly, run fast
	  and pass through walls (Server::getPlayerEffectivePrivs), digs anything at
	  once (Game::handleDigging), can't be hurt, and takes from the catalogue.
	- Sprinting, in every mode: hold Aux1, or double-tap forward; it lasts while
	  they keep moving forward.
	- The map: the body they're on, what they've explored of it (kept in their
	  metadata, a bit per cell of a 256 × 256 grid), and where they are; in
	  creative, the map's Go there.
*/

#include "server/terra.h"
#include "server.h"
#include "emerge.h"
#include "hud_element.h"
#include "inventory.h"
#include "itemdef.h"
#include "log.h"
#include "mapgen/mapgen.h"
#include "orbit/flight.h"
#include "planet/atlas.h"
#include "filesys.h"
#include "porting.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include "serverenvironment.h"
#include "settings.h"
#include "terragen/worlds.h"
#include "util/base64.h"
#include <json/json.h>
#include <cmath>
#include <unordered_set>

namespace {

const float SPRINT = 1.6f;        // × walking speed
const double DOUBLE_TAP = 0.3;    // s between two presses of forward that start a sprint
const float MAP_PERIOD = 2;       // s between telling each player where they are
const int GRID = 256;             // the explored map's cells, round and pole to pole
const size_t GRID_BYTES = GRID * GRID / 8;
const double REVEALED = 160;      // blocks round a player that count as explored

std::string json(const Json::Value &v)
{
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	return Json::writeString(writer, v);
}

bool creative()
{
	return g_settings->getBool("creative_mode");
}

// The home's explored map under the key it always had; another body's under its id.
std::string exploredKey(Server *server, int body)
{
	return body == planet::Atlas::HOME ? "tf_explored" : "tf_explored@" + server->bodyWorlds().at(body).id;
}

std::string loadExplored(Server *server, PlayerSAO *sao, int body)
{
	const std::string stored = sao->getMeta().getString(exploredKey(server, body));
	if (stored.empty())
		return std::string(GRID_BYTES, '\0');
	std::string explored = base64_decode(stored);
	if (explored.size() != GRID_BYTES)
		throw BaseException("player: " + sao->getPlayer()->getName() + "'s explored map of body " +
				std::to_string(body) + " isn't " + std::to_string(GRID_BYTES) + " bytes");
	return explored;
}

// The body a player's on, its size round (blocks) and radius.
struct BodyOn
{
	int index;
	int size;
	double radius;
};

BodyOn bodyOn(Server *server, int body, const TerraWorldFacts &facts)
{
	if (server->atlas()) {
		const terragen::BodyWorld &w = server->bodyWorlds().at(body);
		return { body, (int)w.code.size, w.atlas.radius };
	}
	return { body, facts.size, server->flight().planet().radius };
}

void sendMap(Server *server, RemotePlayer *player, const ServerTerra::MapState &state, const TerraWorldFacts &facts)
{
	const BodyOn on = bodyOn(server, state.body, facts);
	Json::Value v;
	v["code"] = facts.code;
	if (server->atlas()) {
		v["body"] = server->bodyWorlds().at(state.body).id;
		v["name"] = terraBodyName(server, state.body);
		for (size_t b = 0; b < server->bodyWorlds().size(); b++) {
			Json::Value listed;
			listed["id"] = server->bodyWorlds()[b].id;
			listed["name"] = terraBodyName(server, (int)b);
			v["bodies"].append(listed);
		}
	}
	v["size"] = on.size;
	v["radius"] = on.radius;
	v["sea"] = server->getEmergeManager()->mgparams->water_level;
	v["star"] = facts.star;
	v["tilt"] = facts.tilt;
	v["mode"] = creative() ? "creative" : "survival";
	v["explored"] = base64_encode(state.explored);
	v["grid"] = GRID;
	server->setUi(player, "map", json(v));
}

// Cells within `radius` of (cx, cz) marked explored: whether any weren't.
bool reveal(std::string &mask, int cx, int cz, int radius)
{
	bool changed = false;
	for (int dz = -radius; dz <= radius; dz++) {
		const int z = cz + dz;
		if (z < 0 || z >= GRID)
			continue;
		for (int dx = -radius; dx <= radius; dx++) {
			if (dx * dx + dz * dz > radius * radius)
				continue;
			const int bit = z * GRID + ((cx + dx) % GRID + GRID) % GRID;
			const u8 m = 1u << (bit % 8);
			if (!((u8)mask[bit / 8] & m)) {
				mask[bit / 8] = (char)((u8)mask[bit / 8] | m);
				changed = true;
			}
		}
	}
	return changed;
}

// Where a player is, for the map: the body, the cells round them explored, and their place.
void mapAt(Server *server, RemotePlayer *player, PlayerSAO *sao, ServerTerra::MapState &state, const TerraWorldFacts &facts)
{
	const v3f pos = sao->getBasePosition() / BS;
	// in seam there's no column to place them by: as they were till they're out of it
	const std::optional<TerraPlace> d = terraPlace(server, pos);
	if (!d)
		return;
	if (server->atlas()) {
		const std::optional<int> body = server->atlas()->bodyAt(planet::Atlas::moverNode(pos.X, pos.Y, pos.Z));
		if (body && *body != state.body) {
			state.body = *body;
			state.explored = loadExplored(server, sao, *body);
			state.cell = std::nullopt;
			Json::Value v;
			v["id"] = server->bodyWorlds().at(*body).id;
			v["name"] = terraBodyName(server, *body);
			v["home"] = *body == planet::Atlas::HOME;
			server->setUi(player, "body", json(v));
			if (state.ready)
				sendMap(server, player, state, facts);
		}
	}
	const int size = bodyOn(server, state.body, facts).size;
	// longitude east across `size`, latitude north across size / 2
	const double x = std::atan2(d->z, d->x) / (2 * M_PI) * size;
	const double z = std::atan2(d->y, std::hypot(d->x, d->z)) / M_PI * size;
	const int cx = ((int)std::floor((x + size / 2.0) * GRID / size) % GRID + GRID) % GRID;
	const int cz = std::clamp((int)std::floor((size / 2.0 - z) * GRID / size), 0, GRID - 1);
	if (!state.cell || *state.cell != std::pair{ cx, cz }) {
		state.cell = std::pair{ cx, cz };
		const int radius = std::max(2, (int)std::ceil(REVEALED * GRID / size));
		if (reveal(state.explored, cx, cz, radius)) {
			sao->getMeta().setString(exploredKey(server, state.body), base64_encode(state.explored));
			// in creative the whole map shows: nothing new to send
			if (state.ready && !creative())
				sendMap(server, player, state, facts);
		}
	}
	Json::Value v;
	v["x"] = x;
	v["z"] = z;
	if (server->atlas())
		v["body"] = server->bodyWorlds().at(state.body).id;
	v["date"] = facts.date;
	server->setUi(player, "map_position", json(v));
}

// The 2×2 crafting grid the page draws; a world from before had a 3×3: what was in it goes to main.
void layOut(Server *server, RemotePlayer *player, PlayerSAO *sao)
{
	Inventory *inv = sao->getInventory();
	InventoryList *main = inv->getList("main");
	if (!main)
		main = inv->addList("main", 36);
	if (main->getSize() < 36)
		main->setSize(36);
	main->setWidth(9);
	if (InventoryList *craft = inv->getList("craft")) {
		for (u32 i = 0; i < craft->getSize(); i++) {
			ItemStack stack = craft->takeItem(i, craft->getItem(i).count);
			if (stack.empty())
				continue;
			ItemStack left = main->addItem(stack);
			// a full inventory grows a row rather than lose it
			if (!left.empty()) {
				main->setSize(main->getSize() + 9);
				main->addItem(left);
			}
		}
	}
	InventoryList *craft = inv->getList("craft") ? inv->getList("craft") : inv->addList("craft", 4);
	craft->setSize(4);
	craft->setWidth(2);
	// the creative hand tf_player kept here (sprint 119): creative digs at once without one now
	if (InventoryList *hand = inv->getList("hand"))
		hand->clearItems();
	server->SendInventory(player, false);
}

} // namespace

void terraJoinPlayer(Server *server, PlayerSAO *sao)
{
	RemotePlayer *player = sao->getPlayer();
	// a jointed body the client poses by how they move
	ObjectProperties *props = sao->accessObjectProperties();
	props->visual = OBJECTVISUAL_BODY;
	props->textures.clear();
	sao->notifyObjectPropertiesModified();

	layOut(server, player, sao);
	server->hudSetHotbarItemcount(player, 9);
	// health and breath that can't change get no bars
	if (creative() || !g_settings->getBool("enable_damage"))
		server->hudSetFlags(player, 0, HUD_FLAG_HEALTHBAR_VISIBLE | HUD_FLAG_BREATHBAR_VISIBLE);
	if (creative()) {
		ItemGroupList armor = sao->getArmorGroups();
		armor["immortal"] = 1;
		sao->setArmorGroups(armor);
	}
	server->setUi(player, "creative", creative() ? "true" : "false");

	// the wiki (key G), where the world's rules allow it (tf_wiki in world.mt, chosen on
	// the create screen's Rules); a world from before the rule keeps it
	Settings rules;
	rules.readConfigFile((server->getWorldPath() + DIR_DELIM "world.mt").c_str());
	if (!rules.exists("tf_wiki") || rules.getBool("tf_wiki"))
		server->setUi(player, "wiki", "true");

	std::optional<TerraWorldFacts> facts = terraWorldFacts(server);
	if (!facts)
		return;
	ServerTerra::MapState &state = server->terra().maps[player->getName()];
	state = {};
	state.explored = loadExplored(server, sao, planet::Atlas::HOME);
	mapAt(server, player, sao, state, *facts);
	sendMap(server, player, state, *facts);
	state.ready = true;
}

void terraStepPlayers(Server *server, float dtime)
{
	ServerTerra &t = server->terra();
	const double now = porting::getTimeMs() / 1000.0;
	t.maps_timer -= dtime;
	const bool look = t.maps_timer <= 0;
	if (look)
		t.maps_timer = MAP_PERIOD;
	const std::optional<TerraWorldFacts> facts = look ? terraWorldFacts(server) : std::nullopt;
	std::unordered_set<std::string> here;
	for (RemotePlayer *player : server->getEnv().getPlayers()) {
		PlayerSAO *sao = player->getPlayerSAO();
		if (!sao || player->getPeerId() == PEER_ID_INEXISTENT)
			continue;
		const std::string &name = player->getName();
		here.insert(name);

		// sprinting
		const PlayerControl &control = player->getPlayerControl();
		ServerTerra::Sprint &s = t.sprints[name];
		const bool forward = control.up > 0;
		if (forward && !s.down) {
			s.latched = now - s.tapped < DOUBLE_TAP;
			s.tapped = now;
		}
		if (!forward)
			s.latched = false;
		s.down = forward;
		const bool on = forward && (control.aux1 || s.latched);
		// travelling, they're held still (server/planet.cpp gives their speed back)
		if (on != s.on && !t.travelling.count(name)) {
			player->physics_override.speed = on ? SPRINT : 1;
			sao->m_physics_override_sent = false;
			s.on = on;
		}

		if (facts) {
			auto state = t.maps.find(name);
			if (state != t.maps.end() && state->second.ready)
				mapAt(server, player, sao, state->second, *facts);
		}
	}
	for (auto it = t.sprints.begin(); it != t.sprints.end();)
		it = here.count(it->first) ? std::next(it) : t.sprints.erase(it);
	for (auto it = t.maps.begin(); it != t.maps.end();)
		it = here.count(it->first) ? std::next(it) : t.maps.erase(it);
}

bool terraUiEvent(Server *server, PlayerSAO *sao, const std::string &name, const Json::Value &payload)
{
	RemotePlayer *player = sao->getPlayer();
	if (name == "creative.take" || name == "creative.clear") {
		// the catalogue is only a view: the world's mode and the item's definition decide
		if (!creative() || !payload.isObject() || !payload["slot"].isInt())
			return true;
		InventoryList *main = sao->getInventory()->getList("main");
		const int slot = payload["slot"].asInt();
		if (!main || slot < 0 || slot >= (int)main->getSize())
			return true;
		if (name == "creative.clear") {
			main->changeItem(slot, ItemStack());
		} else {
			if (!payload["item"].isString() || !payload["count"].isInt() || payload["count"].asInt() < 1)
				return true;
			const std::string item = payload["item"].asString();
			const IItemDefManager *idef = server->idef();
			if (item.empty() || !idef->isKnown(item))
				return true;
			const ItemDefinition &def = idef->get(item);
			if (def.description.empty() || itemgroup_get(def.groups, "not_in_creative_inventory") > 0 ||
					str_ends_with(item, "_flowing"))
				return true;
			ItemStack stack;
			stack.name = item;
			stack.count = (u16)std::min<int>(payload["count"].asInt(), def.stack_max);
			main->changeItem(slot, stack);
		}
		server->SendInventory(player, false);
		return true;
	}
	if (name == "map.teleport") {
		// the map's Go there, in creative: to the ground at a place (radians) on the body it shows
		const std::string who = player->getName();
		if (!server->atlas() || !creative()) {
			warningstream << "player: " << who << " asked to go somewhere on the map, out of creative or off a planet" << std::endl;
			return true;
		}
		const Json::Value &lat = payload["latitude"], &lon = payload["longitude"];
		if (!lat.isNumeric() || !lon.isNumeric() || !std::isfinite(lat.asDouble()) || !std::isfinite(lon.asDouble()) ||
				std::abs(lat.asDouble()) > M_PI / 2) {
			warningstream << "player: " << who << " sent a map teleport that isn't a place" << std::endl;
			return true;
		}
		int body = planet::Atlas::HOME;
		if (payload["body"].isString()) {
			body = terraBody(server, payload["body"].asString());
			if (body < 0) {
				warningstream << "player: " << who << " sent a map teleport to no body of this world" << std::endl;
				return true;
			}
		}
		const std::optional<v3s16> node = terraGround(server, body, lat.asDouble(), lon.asDouble());
		if (!node) {
			warningstream << "player: no ground at " << lat.asDouble() << ", " << lon.asDouble() << std::endl;
			return true;
		}
		sao->setPos(intToFloat(*node, BS));
		actionstream << "player: " << who << " went to " << *node << " by the map" << std::endl;
		return true;
	}
	return false;
}

void terraCreativePrivs(std::set<std::string> &privs)
{
	if (!creative())
		return;
	for (const char *p : { "core:fly", "core:fast", "core:noclip", "core:creative", "core:give" })
		privs.insert(p);
}
