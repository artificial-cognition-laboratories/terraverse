// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

/*
	The weather on the world. The atmosphere moves on its own (src/fluid: wind,
	rain and storms over the whole world); this is where it meets players and
	blocks. It was tf_weather's, in Lua, until sprint 119: it runs per player and
	across many blocks, which is the engine's (AGENTS.md: core mechanics).

	- Each player is told the weather they stand in, for the page (set_ui
	  "weather"), every PERIOD and as they join. Nothing of it is drawn: the
	  air's drawing comes back with the atmosphere.
	- Snow settles where it falls and melts when it warms; open water freezes
	  over in the cold and thaws again. All from the weather at the spot, so the
	  storm a player sees falling is the one that whitens the ground. It runs on
	  the blocks round players (active block modifiers).

	The blocks are the game's, found by alias as the generator finds rock:
	weather_snow, weather_ice and mapgen_water_source. Worlds terra didn't make
	have no atmosphere.
*/

#include "src/server/world/terra.h"
#include "src/server/app/server.h"
#include "src/server/world/load/emerge.h"
#include "src/shared/platform/system/log.h"
#include "src/server/world/generate/mapgen.h"
#include "src/shared/game/content/nodedef.h"
#include "src/shared/platform/system/porting.h"
#include "src/shared/game/objects/remoteplayer.h"
#include "src/server/world/state/blockmodifier.h"
#include "src/server/players/player_sao.h"
#include "src/server/world/state/environment/serverenvironment.h"
#include "library/world/terragen/src/system/planet.h"
#include <json/json.h>
#include <cmath>
#include <unordered_set>

namespace {

const float PERIOD = 2.0f;        // s between telling each player
const double CACHE_S = 10;        // s a column's weather is read for: one storm is wide
const double FREEZE = -3, THAW = 1, MELT = 1; // °C
const double SNOWING = 0.05;      // mm/h of falling snow that settles

const char *const SNOW = "weather_snow", *const ICE = "weather_ice", *const WATER = "mapgen_water_source";

// The weather at a node, read at most once per 16-node column per CACHE_S.
std::optional<fluid::Sample> weatherAt(Server *server, v3s16 pos)
{
	ServerTerra &t = server->terra();
	const double now = porting::getTimeMs() / 1000.0;
	if (now - t.weather_cached_at > CACHE_S) {
		t.weather_cache.clear();
		t.weather_cached_at = now;
	}
	const s64 key = ((s64)(pos.X >> 4) << 32) | (u32)(pos.Z >> 4);
	auto it = t.weather_cache.find(key);
	if (it == t.weather_cache.end()) {
		const std::optional<TerraWeather> w = terraWeather(server, intToFloat(pos, 1));
		// (none yet: it settles as the world opens; nor on seam) not cached, asked again
		if (!w)
			return std::nullopt;
		it = t.weather_cache.emplace(key, w->sample).first;
	}
	return it->second;
}

content_t id(const NodeDefManager *ndef, const char *name)
{
	const content_t c = ndef->getId(name);
	if (c == CONTENT_IGNORE)
		throw BaseException(std::string("the weather needs the ") + name + " alias: the game names its block");
	return c;
}

// What the weather does to a node at the top of the ground or water.
std::string actionFor(Server *server, MapNode n, const fluid::Sample &w)
{
	const NodeDefManager *ndef = server->getNodeDefManager();
	const double c = w.temperature - 273.15;
	const content_t here = n.getContent();
	if (here == id(ndef, SNOW))
		return c > MELT ? "melt" : "";
	if (here == id(ndef, ICE))
		return c > THAW ? "thaw" : "";
	if (here == id(ndef, WATER))
		return c < FREEZE ? "freeze" : "";
	const ContentFeatures &f = ndef->get(n);
	if (f.walkable && (f.drawtype == NDT_NORMAL || f.drawtype == NDT_ALLFACES_OPTIONAL) &&
			w.snow && w.precipitation > SNOWING)
		return "snow";
	return "";
}

// Open to the sky: air over it, and the full sun there at noon.
bool openSky(ServerEnvironment *env, v3s16 pos)
{
	const NodeDefManager *ndef = env->getGameDef()->ndef();
	const MapNode above = env->getMap().getNode(pos + v3s16(0, 1, 0));
	return above.getContent() == CONTENT_AIR && above.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(above)) >= LIGHT_SUN;
}

void act(Server *server, v3s16 pos, const std::string &action)
{
	ServerEnvironment &env = server->getEnv();
	const NodeDefManager *ndef = server->getNodeDefManager();
	if (action == "snow")
		env.setNode(pos + v3s16(0, 1, 0), MapNode(id(ndef, SNOW)));
	else if (action == "melt")
		env.removeNode(pos);
	else if (action == "freeze")
		env.setNode(pos, MapNode(id(ndef, ICE)));
	else if (action == "thaw")
		env.setNode(pos, MapNode(id(ndef, WATER)));
}

// One of the weather's modifiers: its nodes, how often, and whether it needs the sky open.
class Weathering : public ActiveBlockModifier
{
public:
	Weathering(Server *server, std::vector<std::string> nodes, std::vector<std::string> neighbors,
			u32 chance, bool sky, bool settles) :
		m_server(server), m_nodes(std::move(nodes)), m_neighbors(std::move(neighbors)),
		m_chance(chance), m_sky(sky), m_settles(settles)
	{}

	const std::vector<std::string> &getTriggerContents() const override { return m_nodes; }
	const std::vector<std::string> &getRequiredNeighbors() const override { return m_neighbors; }
	const std::vector<std::string> &getWithoutNeighbors() const override { return m_none; }
	float getTriggerInterval() override { return 10; }
	u32 getTriggerChance() override { return m_chance; }
	bool getSimpleCatchUp() override { return false; }
	s16 getMinY() override { return -MAX_MAP_GENERATION_LIMIT; }
	s16 getMaxY() override { return MAX_MAP_GENERATION_LIMIT; }

	void trigger(ServerEnvironment *env, v3s16 p, MapNode n, u32, u32) override
	{
		if (m_settles) {
			// snow settles on the ground, never on snow or ice
			const NodeDefManager *ndef = m_server->getNodeDefManager();
			if (n.getContent() == id(ndef, SNOW) || n.getContent() == id(ndef, ICE))
				return;
		}
		if (m_sky && !openSky(env, p))
			return;
		terraWeatherAct(m_server, p);
	}

private:
	Server *m_server;
	std::vector<std::string> m_nodes, m_neighbors, m_none;
	u32 m_chance;
	bool m_sky, m_settles;
};

} // namespace

bool terraWorld(Server *server)
{
	const MapgenParams *params = server->getEmergeManager()->mgparams;
	return params && params->mgtype == MAPGEN_TERRA && server->atlas();
}

std::string terraWeatherAct(Server *server, v3s16 pos)
{
	const std::optional<fluid::Sample> w = weatherAt(server, pos);
	if (!w)
		return ""; // (no weather yet: it settles as the world opens)
	const std::string action = actionFor(server, server->getEnv().getMap().getNode(pos), *w);
	if (!action.empty())
		act(server, pos, action);
	return action;
}

void terraAddWeatherModifiers(Server *server)
{
	if (!terraWorld(server))
		return; // no atmosphere outside terra worlds
	ServerEnvironment &env = server->getEnv();
	env.addActiveBlockModifier(new Weathering(server, { "group:crumbly", "group:cracky", "group:leaves" },
			{ "air" }, 60, true, true));
	env.addActiveBlockModifier(new Weathering(server, { SNOW, ICE }, {}, 20, false, false));
	env.addActiveBlockModifier(new Weathering(server, { WATER }, { "air" }, 30, true, false));
}

void terraStepWeather(Server *server, float dtime)
{
	if (!terraWorld(server))
		return;
	ServerTerra &t = server->terra();
	t.weather_timer -= dtime;
	const bool look = t.weather_timer <= 0;
	if (look)
		t.weather_timer = PERIOD;
	const s16 sea = server->getEmergeManager()->mgparams->water_level;
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "";
	std::unordered_set<std::string> here;
	for (RemotePlayer *player : server->getEnv().getPlayers()) {
		PlayerSAO *sao = player->getPlayerSAO();
		if (!sao || player->getPeerId() == PEER_ID_INEXISTENT)
			continue;
		const std::string name = player->getName();
		here.insert(name);
		auto told = t.weather_told.find(name);
		if (!look && told != t.weather_told.end())
			continue; // someone new is told at once; the rest every PERIOD
		const v3f pos = sao->getBasePosition() / BS;
		std::optional<TerraWeather> w;
		// above the air, in space, there's no weather; none on seam, or while it settles
		if (pos.Y - sea < terragen::SPACE_BOUNDARY)
			w = terraWeather(server, pos);
		Json::Value v; // null: no weather to show
		if (w) {
			const fluid::Sample &s = w->sample;
			v["temperature"] = s.temperature - 273.15;
			v["humidity"] = s.humidity;
			v["precipitation"] = s.precipitation;
			v["snow"] = s.snow;
			v["wind"]["x"] = s.wind_u;
			v["wind"]["z"] = s.wind_v;
			v["pressure"] = s.pressure / 100;
			// events go only when there are any
			for (int e = 0; e < (int)fluid::Event::Count; e++)
				if (s.events & (1u << e))
					v["events"].append(fluid::EVENT_NAMES[e]);
		}
		const std::string json = Json::writeString(writer, v);
		if (told != t.weather_told.end() && told->second == json)
			continue;
		server->setUi(player, "weather", json);
		t.weather_told[name] = json;
	}
	for (auto it = t.weather_told.begin(); it != t.weather_told.end();)
		it = here.count(it->first) ? std::next(it) : t.weather_told.erase(it);
}
