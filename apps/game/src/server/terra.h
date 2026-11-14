// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/terrain.h"
#include "mapgen/terra_species.h"
#include "matter/matter.h"
#include "terragen/system.h"
#include "terragen/world.h"
#include "rivers/basin.h"
#include "rivers/rivers.h"
#include "weather/weather.h"
#include "irr_v3d.h"
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
	std::vector<terra::SpeciesDef> species; // registration order
	std::optional<std::vector<terra::Species>> resolved; // against the node definitions
	std::vector<std::unique_ptr<terragen::World>> worlds; // by body
	std::unique_ptr<matter::Matter> matter;
	// the rivers as the weather fills them (sprint 115): stepped by the
	// weather's own steps, made on its first, saved beside it each day
	std::unique_ptr<rivers::Network> rivers;
	bool rivers_settled = false;
	double rivers_day = -1;
	u64 rivers_step = 0; // each step of them: what a block's flow was read at
	std::vector<rivers::Forcing> river_forcings; // storms and droughts put by hand (core.terra.force_rivers)
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

