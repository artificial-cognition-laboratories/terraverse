// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/views/game/far/land/terrain.h"
#include "src/server/game/cut/cuts.h"
#include "src/server/game/dig/digs.h"
#include "src/server/world/generate/place/terra_species.h"
#include "library/matter/matter/matter.h"
#include "library/world/terragen/src/system/system.h"
#include "library/world/terragen/src/world.h"
#include "library/world/rivers/basin.h"
#include "library/world/rivers/rivers.h"
#include "library/world/weather/weather.h"
#include "src/shared/base/irr_v3d.h"
#include "src/shared/game/objects/player.h"
#include <set>
#include <string>
#include <json/json.h>
#include <future>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <array>
#include <vector>

/*
	What core.terra keeps for one server: the species the game registers, and
	what is built from the world on first use. The Server owns it, so a second
	world played in the same process starts from nothing, not from the first
	world's leftovers.

	Species are registered while mods load; the first use of them (resolved)
	closes registration, so nothing built from them can go stale.
*/
struct ServerTerra
{
	// cutting (sprint 123): what tools peel off blocks, and each player's chosen cut
	std::unique_ptr<cut::Cuts> cuts;
	std::unique_ptr<dig::Digs> digs; // the shovel (sprint 124)
	std::vector<terra::SpeciesDef> species; // registration order
	std::optional<std::vector<terra::Species>> resolved; // against the node definitions
	std::vector<std::unique_ptr<terragen::World>> worlds; // by body
	std::unique_ptr<matter::Matter> matter;
	// the rivers as the weather fills them (sprint 115): stepped by the
	// weather's own steps, made on its first, saved beside it each day
	struct RiverState
	{
		std::unique_ptr<rivers::Network> network;
		bool settled = false;
		double day = -1;
		u64 step = 0; // each step of them: what a block's flow was read at
		std::vector<rivers::Forcing> forcings; // storms and droughts put by hand (core.terra.force_rivers)
	};
	// none till the weather's open (it settles on a worker, and they with it)
	std::shared_ptr<RiverState> rivers;
	// map blocks with a river's held water near players (sprint 115): one of
	// its held nodes (none: it has none), and its flow as last read
	struct RiverBlock
	{
		std::optional<v3s16> held;
		u64 read_at = 0; // rivers_step (0: never)
		s16 rise = 0;    // sixteenths
		u8 speed = 0;    // nodes/s × 40
		u8 way = 0;      // the way it runs, 1..255 round from +x toward +z (0: none, a lake or unknown)
	};
	std::unordered_map<v3s16, RiverBlock> river_blocks;
	std::unordered_map<u16, std::unordered_map<v3s16, u32>> river_sent; // per peer: what it was sent
	// lakes a river was dammed into (sprint 115, phase 5: server/lakes.cpp)
	struct Lake
	{
		v3s16 seed;         // a node of the pool it rose from: its water's top
		s32 level0 = 0;     // that pool's level (the first open block over it)
		double level = 0;   // its surface now (blocks)
		std::optional<rivers::Basin> basin; // found once its ground's loaded
		s32 written = 0;    // its water is written into the map under this level
		std::unordered_map<s64, s32> wet; // columns it's written water in (x << 32 | z): the level written to
		std::unordered_set<s64> blocks; // the map block columns of its basin's columns
		std::unordered_set<s64> touch;  // its basin's columns and the ones round them (its dam's), its streams': an edit there looks again
		std::unordered_set<s64> cols;   // its basin's columns
		double inflow = 0;  // m³/s: the river's, into it
		double outflow = 0; // m³/s: what pours out over its rim (Basin::outflow)
		// its new streams (6b): what pours out over its main outlets, run down
		// over the land to water, or into a hollow it fills (a lake of its own)
		struct Stream
		{
			std::vector<std::array<s32, 3>> path; // columns from its outlet down (x, z, ground's top)
			double q = 0;                         // m³/s down it
			std::optional<v3s16> fills;           // the hollow it ends in, or the lake it runs into: that lake's seed
			s32 joins_cell = -1, joins_along = 0; // the river it runs into: its run's cell, and how far along it
		};
		std::vector<Stream> streams;
		bool routed = false;
		int generation = 0;              // a dammed river's: 0; a hollow a stream of it fills: 1, …
		double fed = 0, fed_next = 0;    // m³/s streams bring it (this step's, and gathered for the next)
		bool refind = false; // its ground changed: look again
		s32 risen = 0;       // the level its basin's been spread to over its spill (Basin::rise)
		// a flood (5d): a river over its banks, not dammed: it stands where its
		// river's flow puts it (target), spread over the dry ground beside it
		bool flood = false;
		double target = 0;
		u64 target_step = 0; // the rivers step its target was set at
		// the weather over it (5e): mm/h of rain on it and of evaporation off it
		double rain = 0, evaporation = 0;
		u64 weather_step = 0;
	};
	std::vector<Lake> lakes;
	// what lakes' streams bring the rivers they run into (6c): per run's cell,
	// (how far along it, m³/s): its channel carries it from there down
	std::unordered_map<s32, std::vector<std::pair<s32, double>>> river_joins;
	std::vector<v3s16> lake_edits; // nodes changed since the lakes last looked
	bool lakes_writing = false;    // (their own water going in: not an edit to look at)
	bool lakes_loaded = false;
	std::unordered_map<s64, s32> generated_tops; // columns' tops as terragen made them (basins over ground not loaded)
	float lakes_saved = 0;         // s since they were last saved
	std::unique_ptr<weather::Weather> weather; // after world, matter and rivers: it refers to them
	std::vector<std::unique_ptr<far::Generator>> surfaces; // by body
	std::unique_ptr<terragen::System> system; // the world's, for its bodies' clocks
	// each player's sky as last sent (server/sky.cpp), by name: a new one is sent
	// only when it's moved enough to see
	struct SkySent
	{
		double latitude, declination, thin, turns, at;
	};
	std::unordered_map<std::string, SkySent> skies;
	float skies_timer = 0; // s to the next look
	// each player's gravity as last set (server/gravity.cpp), by name
	std::unordered_map<std::string, double> gravity;
	// players travelling to another body (server/planet.cpp), by name: held still on the
	// travel screen while the land there is made, then put on it
	struct Travel
	{
		PlayerPhysicsOverride had; // given back on arriving
		v3s16 node;                // where they land
		std::string body;
		float hold = -1;           // s left on the screen once there (< 0: not there yet)
	};
	std::unordered_map<std::string, Travel> travelling;
	// each player's map (server/player.cpp), by name: the body they're on, what they've
	// explored of it, the cell they're in (none: not placed yet), and whether the page has it
	struct MapState
	{
		int body = 0;
		std::string explored;
		std::optional<std::pair<int, int>> cell;
		bool ready = false;
	};
	std::unordered_map<std::string, MapState> maps;
	float maps_timer = 0;
	// sprinting (server/player.cpp), by name: forward held, when it was last pressed, a
	// sprint latched by a double tap, and whether the faster speed is on
	struct Sprint
	{
		bool down = false, latched = false, on = false;
		double tapped = -1;
	};
	std::unordered_map<std::string, Sprint> sprints;
	// the weather on the world (server/weather.cpp): what each player was last told
	// (its JSON), by name, and the weather read per 16-node column, for a while
	std::unordered_map<std::string, std::string> weather_told;
	float weather_timer = 0;
	std::unordered_map<s64, fluid::Sample> weather_cache;
	double weather_cached_at = -1e9;

	// The weather being opened, and the rivers with it: a new world's settles
	// a year of it, seconds, on a worker so the server goes on meanwhile
	// (handed over whole: neither's seen half made). Last, so it's let go
	// (waited for) before the world and matter it reads.
	struct OpenedWeather
	{
		std::unique_ptr<weather::Weather> weather;
		std::shared_ptr<RiverState> rivers;
	};
	std::future<OpenedWeather> weather_opening;

	// The species against the node definitions. The first call closes registration.
	const std::vector<terra::Species> &resolve(const NodeDefManager *ndef)
	{
		if (!resolved)
			resolved = terra::resolve(species, ndef);
		return *resolved;
	}
};

class Server;
// Each player is sent the flow of the rivers in the map blocks round them
// that changed since they were last told (sprint 115: TOCLIENT_RIVERS)
void terraSendRivers(Server *server);
// m³/s of the river whose water is at `node` now, in the blocks (rivers::Flow::local;
// 0: none, a lake, or no rivers yet)
double terraRiverDischarge(Server *server, v3s16 node);
// The grid river whose water is at `node`: its run's cell and how far along
// it (blocks); (−1, 0) for none
std::pair<s32, s32> terraRiverRun(Server *server, v3s16 node);
// The first open block over a column's ground or water as generated
// (rivers::Basin::UNKNOWN: no place on the home world)
s32 terraGeneratedTop(Server *server, s32 x, s32 z);

// The real sun over a position (script/lua_api/l_terra.cpp, as core.terra.sun): degrees,
// its body's clock (the time of day there is at + turns × the engine's). None where
// terra didn't make the world, or on seam (no column of the planet).
struct TerraSun
{
	double latitude = 0, declination = 0, elevation = 0;
	double daylight = 0; // the share of the day the sun is up
	double turns = 1, at = 0;
	const char *season = "";
};
std::optional<TerraSun> terraSun(Server *server, const v3f &pos);

// A position's place on its body (script/lua_api/l_terra.cpp, as core.terra.place):
// the direction of its column from the body's centre. None on seam, or where terra
// didn't make the world.
struct TerraPlace
{
	double x, y, z;
};
std::optional<TerraPlace> terraPlace(Server *server, const v3f &pos);

// The world's code and what it says (as core.terra.world), and the date (0..1 through
// the year, as core.terra.calendar). None where terra didn't make the world.
struct TerraWorldFacts
{
	std::string code, star;
	int size;
	double tilt, date;
};
std::optional<TerraWorldFacts> terraWorldFacts(Server *server);

// What the world model says, for the checks' probes (src/probe; sprint 119: were
// core.terra's). Each throws BaseException on seam, or where terra didn't make the world.
struct TerraCalendar
{
	uint32_t year, day; // from 1
	double date;   // 0..1 through the year (0: the northern spring equinox)
	int year_days;
};
TerraCalendar terraCalendar(Server *server);
struct TerraClimate
{
	double temperature; // °C, now
	std::string biome;
};
TerraClimate terraClimate(Server *server, const v3f &pos);
// The geological province under a position.
std::string terraProvince(Server *server, const v3f &pos);
// A column as the generator makes it, without making it: its top solid block's y and
// name, and the surface of any water or lava over it. What the far view draws.
struct TerraSurface
{
	int ground;
	std::string top;
	std::optional<int> water;
	std::string liquid;
};
TerraSurface terraSurface(Server *server, v3s16 column);
// The block the generator places at each node of an area (x inner, then y, then z), before
// what's planted after; none where it stores no cell (seam). At most 2^24 nodes.
std::vector<std::optional<content_t>> terraLand(Server *server, v3s16 minp, v3s16 maxp);
// Whether a body (by its number) keeps one face to its star.
bool terraFacingStar(Server *server, int body);

// The weather at a position (script/lua_api/l_terra.cpp, as core.terra.weather):
// `moving`, whether it's src/fluid's moving air (else a still body's climate). None
// where terra didn't make the world, on seam, or while the weather settles as the
// world opens.
struct TerraWeather
{
	fluid::Sample sample;
	bool moving = false;
};
std::optional<TerraWeather> terraWeather(Server *server, const v3f &pos);

// The weather on the world (server/weather.cpp): each player told what they stand in,
// and snow, ice and thaw on the open ground round them.
void terraStepWeather(Server *server, float dtime);
// Whether terra generated this world, as a planet (a singlenode one, for checks, isn't).
bool terraWorld(Server *server);
// The river or lake at a position, as a line for chat; none where there's neither.
std::optional<std::string> terraRiverLine(Server *server, const v3f &pos);
// A storm or drought over the rivers round a position, in place of the weather, for a while
// (rain in mm/h); radius 0 hands them back to it. Throws std::invalid_argument, said to the player.
void terraForceRivers(Server *server, const v3f &pos, double radius_km, double hours, double rain);
void terraAddWeatherModifiers(Server *server);
// What the weather does to the node at `pos` (the top of the ground or water, open to
// the sky), done: "snow", "melt", "freeze", "thaw", or "" for nothing.
std::string terraWeatherAct(Server *server, v3s16 pos);

// Each player's sky (server/sky.cpp): the real sun where they stand, and on a world
// that isn't a planet, the air's blue thinning to black as they climb.
void terraStepSkies(Server *server, float dtime);

// A player's own (server/player.cpp): what they're given as they join, sprinting and
// the map each step, the page's events about them (true when handled: the catalogue,
// the map's Go there), and what creative grants.
class PlayerSAO;
void terraJoinPlayer(Server *server, PlayerSAO *sao);
void terraStepPlayers(Server *server, float dtime);
bool terraUiEvent(Server *server, PlayerSAO *sao, const std::string &name, const Json::Value &payload);
void terraCreativePrivs(std::set<std::string> &privs);

// A player's weight (server/gravity.cpp): the planet's pull at their height.
void terraStepGravity(Server *server);

// Standing on the planet (server/planet.cpp). All but terraBodyIds want an atlas world.
class RemotePlayer;
class PlayerSAO;
// A body's index by its id; −1 for none (or not an atlas world).
int terraBody(Server *server, const std::string &id);
// The bodies that can be stood on, by id, the home first; none on a world that isn't a planet.
std::vector<std::string> terraBodyIds(Server *server);
// Where one lands on a body: near a place (degrees), else its spawn. None if there's no land.
std::optional<v3s16> terraLanding(Server *server, int body, std::optional<std::pair<double, double>> near);
// The ground at a place on a body (radians), as one would stand on it; over water, its
// surface. None above the atlas, or on a world that isn't a planet.
std::optional<v3s16> terraGround(Server *server, int body, double lat, double lon);
// A body's name, as its system names it ("Moon").
std::string terraBodyName(Server *server, int body);
// The home's spawn (BS units). Throws if the planet has no land to stand on.
v3f terraSpawn(Server *server);
// Whether a position (nodes) is seam: on no column of the planet.
bool terraSeam(Server *server, const v3f &nodes);
// One in seam starts again at the spawn; `how` says how they got there, for the log.
void terraOffSeam(Server *server, PlayerSAO *sao, const char *how);
// Send a player to a body: "" when they're on the way, else why not.
std::string terraTravel(Server *server, RemotePlayer *player, const std::string &body,
		std::optional<std::pair<double, double>> near);
// Each step: the travelling are landed and let go; anyone in seam is moved out.
void terraStepPlanet(Server *server, float dtime);

struct MapEditEvent;
// The lakes (server/lakes.cpp): edits are noted, then looked at each step
// (a river dammed becomes a lake, a lake's dam taken away drains it), and
// each lake rises with its river's water, written into the map
void terraEdited(Server *server, const MapEditEvent &event);
void terraStepLakes(Server *server, float dtime);
// a map block's lake surface over its top block's (sixteenths), if a lake's top is in it
std::optional<s16> terraLakeRise(Server *server, v3s16 block);
// A river's water node whose flow puts its surface `surge` blocks over its
// generated level (rivers::Flow::surge): past its banks, its pool floods
// the ground beside it; back under them, a flood there goes down
void terraFlood(Server *server, v3s16 node, double surge);
// mm/h of rain on open water at `node` and of its evaporation, from the weather there
std::pair<double, double> terraLakeWeather(Server *server, v3s16 node);
// The level a dammed lake stands at when its river in and rain on it balance
// what evaporates off it (5e): its spill if they never do (it fills and spills)
double terraLakeBalance(const ServerTerra::Lake &lake);

