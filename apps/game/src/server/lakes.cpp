// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

/*
	Lakes a river was dammed into (planning/sprint-115-rivers.md, phase 5).
	A node built or dug beside a river's held water looks again at that
	pool: if the way down its river is closed, its basin (rivers::Basin over
	the map) holds water up to a spill, and the pool becomes a lake there.
	Its river's discharge raises it (in the physics' seconds, as the block
	sim runs: a big river blocked overflows at once, a stream takes a
	while), and each block it rises is written into the map as held water
	(fill::HELD | fill::LAKE), the part block over as the client's rise. At
	its spill it stands; dig the dam out and it drains back to its river.

	Not yet: what pours over its spill (phase 6's outlets), the reach below
	the dam losing its water, ground not loaded (terragen's, 5c).
*/

#include "server/terra.h"
#include "server.h"
#include "serverenvironment.h"
#include "servermap.h"
#include "itemgroup.h"
#include "nodedef.h"
#include "map.h"
#include "fill/fill.h"
#include "filesys.h"
#include "log.h"
#include "util/numeric.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

using Lake = ServerTerra::Lake;

static const s32 SEARCH = 48;          // blocks over and under a pool a basin's looked for in
static const size_t MOST = 20000;      // columns a basin's flood looks at
static const double DRAIN = 0.25;      // blocks/s a lake falls with its dam gone (outlets' weir: phase 6)
static const double FLOOD_RISE = 0.05; // blocks/s a flood comes up over its banks
static const double BRIM = 15.0 / 16;  // a river's surface over its generated level past which it's over its banks
static const size_t WRITE_BUDGET = 8192; // nodes written a step, all lakes
static const float SAVE_EVERY = 10;    // s
static const int OUTLETS = 4;          // a lake's outlets run down as streams of their own (the largest)
static const int STREAM_MOST = 1500;   // columns a stream's run looked for over
static const int GENERATIONS = 6;      // hollows filled, one from another's stream, before it's let go

static s64 column(s32 x, s32 z) { return ((s64)x << 32) | (u32)z; }

static bool isLakeWater(const MapNode &n, const NodeDefManager *ndef)
{
	return ndef->get(n).liquid_move_physics && (n.getParam2() & fill::LAKE);
}

static bool isRiverWater(const MapNode &n, const NodeDefManager *ndef)
{
	return ndef->get(n).liquid_move_physics && (n.getParam2() & fill::HELD) && !(n.getParam2() & fill::LAKE);
}

// A plant's part (a trunk, a canopy, a stem): water stands round it, it
// doesn't stand on it, so a basin's ground is under it
static bool plant(const ContentFeatures &cf)
{
	return itemgroup_get(cf.groups, "tree") > 0 || itemgroup_get(cf.groups, "leaves") > 0 ||
			itemgroup_get(cf.groups, "plant") > 0;
}

// What stands at a column round a pool at level0, from the map: a lake's
// own water and plants are open (its ground is under them). A column not
// loaded is as terragen made it (an edit there is seen when it loads: the
// lake's looked at again then)
static rivers::Basin::Top mapTop(Server *server, s32 level0)
{
	ServerMap &map = server->getEnv().getServerMap();
	const NodeDefManager *ndef = server->ndef();
	return [server, mapp = &map, ndef, level0](s32 x, s32 z) -> s32 {
		bool any = false; // (high over a valley the map is often not loaded: open air, as far as water goes)
		for (s32 y = level0 + SEARCH; y >= level0 - SEARCH; y--) {
			bool valid = false;
			const MapNode n = mapp->getNode(v3s16(x, y, z), &valid);
			if (!valid)
				continue;
			any = true;
			if (isLakeWater(n, ndef))
				continue;
			const ContentFeatures &cf = ndef->get(n);
			if ((cf.walkable && !plant(cf)) || cf.liquid_move_physics)
				return y + 1;
		}
		if (any)
			return level0 - SEARCH; // a deep hole: a way out
		std::unordered_map<s64, s32> &made = server->terra().generated_tops;
		if (made.size() > 4000000)
			made.clear();
		auto it = made.find(column(x, z));
		if (it == made.end())
			it = made.emplace(column(x, z), terraGeneratedTop(server, x, z)).first;
		return it->second;
	};
}

static void found(Lake &lake, rivers::Basin &&basin)
{
	lake.basin = std::move(basin);
	lake.risen = lake.basin->spill();
	lake.blocks.clear();
	lake.touch.clear();
	lake.cols.clear();
	for (const rivers::Basin::Column &c : lake.basin->columns()) {
		lake.cols.insert(column(c.x, c.z));
		lake.blocks.insert(column(getContainerPos((s16)c.x, MAP_BLOCKSIZE), getContainerPos((s16)c.z, MAP_BLOCKSIZE)));
		for (s32 dz = -1; dz <= 1; dz++)
			for (s32 dx = -1; dx <= 1; dx++)
				lake.touch.insert(column(c.x + dx, c.z + dz));
	}
	lake.refind = false;
}

// The pool a river's water node is part of: its column's top, and the level over it
static v3s16 poolTop(ServerMap &map, const NodeDefManager *ndef, v3s16 p)
{
	for (int i = 0; i < SEARCH && isRiverWater(map.getNode(p + v3s16(0, 1, 0)), ndef); i++)
		p.Y++;
	return p;
}

void terraEdited(Server *server, const MapEditEvent &event)
{
	ServerTerra &t = server->terra();
	if (t.lakes_writing || !t.rivers)
		return;
	if (event.type == MEET_ADDNODE || event.type == MEET_REMOVENODE || event.type == MEET_SWAPNODE)
		t.lake_edits.push_back(event.p);
}

// Look again at what an edit touched: lakes over it, and river pools beside it
static void lookAt(Server *server, ServerTerra &t, v3s16 p)
{
	ServerMap &map = server->getEnv().getServerMap();
	const NodeDefManager *ndef = server->ndef();
	static const v3s16 AROUND[] = { { 0, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (const v3s16 &d : AROUND) {
		const v3s16 q0 = p + d;
		bool in_lake = false;
		for (Lake &lake : t.lakes)
			if (lake.wet.count(column(q0.X, q0.Z)) || lake.touch.count(column(q0.X, q0.Z))) {
				lake.refind = true;
				in_lake = true;
			}
		if (in_lake)
			continue;
		for (s16 dy = -1; dy <= 1; dy++) {
			const v3s16 q = q0 + v3s16(0, dy, 0);
			if (!isRiverWater(map.getNode(q), ndef))
				continue;
			const v3s16 top = poolTop(map, ndef, q);
			const s32 level0 = top.Y + 1;
			rivers::Basin basin = rivers::Basin::find(top.X, top.Z, level0, mapTop(server, level0), MOST);
			if (!basin.dammed())
				continue;
			Lake lake;
			lake.seed = top;
			lake.level0 = level0;
			lake.level = level0;
			lake.written = level0;
			found(lake, std::move(basin));
			actionstream << "lakes: a river dammed at " << top << ": a lake to " << lake.basin->spill() << " over "
					<< lake.basin->columns().size() << " columns" << (lake.basin->capped() ? " (at least)" : "") << std::endl;
			t.lakes.push_back(std::move(lake));
			return; // (the rest of this edit's pools: the next edits', or this lake's own)
		}
	}
}

// Is there water a stream can run into at the top of this column: a river's
// or a lake's held water, or the sea's?
static bool joinsWater(Server *server, s32 x, s32 z, s32 top)
{
	const MapNode n = server->getEnv().getServerMap().getNode(v3s16(x, top - 1, z));
	const ContentFeatures &cf = server->ndef()->get(n);
	return cf.liquid_move_physics && !isLakeWater(n, server->ndef());
}

// A lake's basin found again from its pool: from any of its first pool's
// columns still open at its level (the dam may stand on its seed)
static void refind(Server *server, Lake &lake)
{
	const rivers::Basin::Top top = mapTop(server, lake.level0);
	std::vector<std::pair<s32, s32>> seeds{ { lake.seed.X, lake.seed.Z } };
	if (lake.basin)
		for (const rivers::Basin::Column &c : lake.basin->columns())
			if (c.top <= lake.level0)
				seeds.emplace_back(c.x, c.z);
	for (const auto &[x, z] : seeds) {
		const s32 at = top(x, z);
		if (at == rivers::Basin::UNKNOWN || at > lake.level0)
			continue;
		found(lake, lake.flood ? rivers::Basin::spread(x, z, lake.level0, (s32)std::ceil(lake.target) + 1, top, MOST)
				: lake.generation > 0 ? rivers::Basin::find(x, z, lake.level0, top, MOST,
						[server](s32 wx, s32 wz, s32 wt) { return joinsWater(server, wx, wz, wt); })
				: rivers::Basin::find(x, z, lake.level0, top, MOST));
		lake.seed = v3s16(x, lake.level0 - 1, z);
		lake.routed = false;
		infostream << "lakes: the lake at " << lake.seed << " looked at again: "
				<< (lake.basin->dammed() ? "spills at " + std::to_string(lake.basin->spill()) : std::string("open")) << std::endl;
		return;
	}
	lake.refind = false; // none open: built over; it stands as it is
	infostream << "lakes: the lake at " << lake.seed << " has no open column left: it stands" << std::endl;
}

// Each column's water kept to the lake's level: laid where it's short, taken
// away where it's over, a column at a time and only where its blocks are
// loaded, so one away from players catches up when it loads (or is made)
static void write(Server *server, ServerTerra &t, Lake &lake, size_t &budget)
{
	ServerEnvironment &env = server->getEnv();
	ServerMap &map = env.getServerMap();
	const NodeDefManager *ndef = server->ndef();
	lake.written = (s32)std::floor(lake.level + 1e-6);
	const MapNode seed = map.getNode(lake.seed);
	const content_t water = ndef->get(seed).liquid_move_physics ? seed.getContent() : ndef->getId("tf_world:water_source");
	auto open = [&](const MapNode &n) {
		return n.getContent() == CONTENT_AIR || (ndef->get(n).buildable_to && !ndef->get(n).liquid_move_physics);
	};
	auto loaded = [&](s32 x, s32 z, s32 lo, s32 hi) {
		for (s32 y = lo; y <= hi; y += MAP_BLOCKSIZE)
			if (!map.getBlockNoCreateNoEx(getNodeBlockPos(v3s16(x, y, z))))
				return false;
		return map.getBlockNoCreateNoEx(getNodeBlockPos(v3s16(x, hi, z))) != nullptr;
	};
	// a column to `want` over its ground at `bottom` (none: everything of the lake's in it goes)
	auto keep = [&](s32 x, s32 z, std::optional<s32> bottom, s32 want) {
		const s64 k = column(x, z);
		auto had = lake.wet.find(k);
		if (!bottom) {
			// none of it the lake's any more: its water taken out, down through
			// it to whatever's under (only the blocks it's in need be loaded)
			if (had == lake.wet.end())
				return;
			for (s32 y = had->second - 1; budget > 0; y--) {
				bool valid = false;
				const MapNode n = map.getNode(v3s16(x, y, z), &valid);
				if (!valid)
					return; // (not loaded: when it is)
				if (!isLakeWater(n, ndef))
					break;
				env.swapNode(v3s16(x, y, z), MapNode(CONTENT_AIR));
				budget--;
			}
			if (budget > 0)
				lake.wet.erase(had);
			return;
		}
		s32 have;
		if (had != lake.wet.end()) {
			have = had->second;
		} else {
			if (!bottom)
				return;
			// not written by this lake in this run: what's in the map (a lake loaded from its file)
			if (!loaded(x, z, *bottom, *bottom))
				return;
			have = *bottom;
			bool valid = true;
			while (have < *bottom + SEARCH && isLakeWater(map.getNode(v3s16(x, have, z), &valid), ndef) && valid)
				have++;
		}
		const s32 lo = bottom ? *bottom : have - SEARCH;
		if (!bottom)
			want = lo;
		if (have == want || !loaded(x, z, std::min(lo, want), std::max(have, want)))
			return;
		for (s32 y = std::max(have, lo); y < want && budget > 0; y++, budget--)
			if (open(map.getNode(v3s16(x, y, z))))
				env.swapNode(v3s16(x, y, z), MapNode(water, 0, fill::HELD | fill::LAKE));
		for (s32 y = have - 1; y >= want && y >= lo && budget > 0; y--) {
			const MapNode n = map.getNode(v3s16(x, y, z));
			if (!isLakeWater(n, ndef)) {
				if (!bottom)
					break; // under the lake's water: its ground
				continue;
			}
			env.swapNode(v3s16(x, y, z), MapNode(CONTENT_AIR));
			budget--;
		}
		if (budget == 0)
			return; // (partly done: looked at again next step, from the map)
		if (want > (bottom ? *bottom : want))
			lake.wet[k] = want;
		else
			lake.wet.erase(k);
	};
	t.lakes_writing = true;
	std::unordered_set<s64> in_basin;
	// its streams: a block of water on their ground, all the way down
	for (const Lake::Stream &st : lake.streams)
		for (const std::array<s32, 3> &c : st.path) {
			if (!in_basin.insert(column(c[0], c[1])).second || budget == 0)
				continue;
			keep(c[0], c[1], c[2], c[2] + 1);
		}
	if (lake.basin)
		for (const rivers::Basin::Column &c : lake.basin->columns()) {
			in_basin.insert(column(c.x, c.z));
			keep(c.x, c.z, c.top, c.enter < lake.level && c.top < lake.written ? lake.written : c.top);
			if (budget == 0)
				break;
		}
	// columns it's written that aren't its basin's now: all of it goes
	std::vector<s64> gone;
	for (const auto &[k, have] : lake.wet)
		if (!in_basin.count(k))
			gone.push_back(k);
	for (s64 k : gone) {
		if (budget == 0)
			break;
		keep((s32)(k >> 32), (s32)(u32)k, std::nullopt, 0);
	}
	t.lakes_writing = false;
}

static std::string lakesPath(Server *server) { return server->getWorldPath() + DIR_DELIM "lakes.txt"; }

static void save(Server *server, const ServerTerra &t)
{
	std::ofstream out(lakesPath(server));
	for (const Lake &l : t.lakes)
	{
		out << l.seed.X << ' ' << l.seed.Y << ' ' << l.seed.Z << ' ' << l.level0 << ' ' << l.level << ' ' << l.written
			<< ' ' << (l.flood ? 1 : 0) << ' ' << l.target << ' ' << l.generation << ' ' << l.wet.size();
		for (const auto &[k, have] : l.wet)
			out << ' ' << (s32)(k >> 32) << ' ' << (s32)(u32)k << ' ' << have;
		out << '\n';
	}
}

static void load(Server *server, ServerTerra &t)
{
	t.lakes_loaded = true;
	std::ifstream in(lakesPath(server));
	std::string line;
	while (std::getline(in, line)) {
		std::istringstream row(line);
		Lake l;
		size_t wet = 0;
		int flood = 0;
		if (!(row >> l.seed.X >> l.seed.Y >> l.seed.Z >> l.level0 >> l.level >> l.written >> flood >> l.target >> l.generation >> wet))
			throw std::runtime_error("lakes: " + lakesPath(server) + ": not a lake: " + line);
		for (size_t i = 0; i < wet; i++) {
			s32 x, z, have;
			if (!(row >> x >> z >> have))
				throw std::runtime_error("lakes: " + lakesPath(server) + ": a lake's columns cut short: " + line);
			l.wet[column(x, z)] = have;
		}
		l.flood = flood != 0;
		l.refind = true; // its basin: once its ground's loaded
		t.lakes.push_back(std::move(l));
	}
}

// Its outlets' streams found: from each of its lowest rim columns down the
// steepest way over the land (the map's, terragen's where it isn't
// loaded), never back into it, till water takes it or a hollow holds it
static void route(Server *server, ServerTerra &t, Lake &lake, std::vector<Lake> &born)
{
	lake.routed = true;
	lake.streams.clear();
	if (!lake.basin)
		return;
	const rivers::Basin &b = *lake.basin;
	std::vector<rivers::Basin::Rim> outlets;
	for (const rivers::Basin::Rim &r : b.rim())
		if (r.top < lake.level)
			outlets.push_back(r);
	// the lowest first, and of those the nearest its way out (where its flood ran off)
	auto off = [&](const rivers::Basin::Rim &r) {
		return (s64)(r.x - b.outX()) * (r.x - b.outX()) + (s64)(r.z - b.outZ()) * (r.z - b.outZ());
	};
	std::sort(outlets.begin(), outlets.end(), [&](const auto &a, const auto &c) {
		return a.top != c.top ? a.top < c.top : off(a) < off(c);
	});
	std::unordered_set<s64> in, taken; // its own columns; columns a stream of it already runs down
	for (const rivers::Basin::Column &c : b.columns())
		in.insert(column(c.x, c.z));
	static const s32 SIDES[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	for (const rivers::Basin::Rim &r : outlets) {
		if ((int)lake.streams.size() >= OUTLETS)
			break;
		if (taken.count(column(r.x, r.z)))
			continue; // (beside an outlet already run: the same stream)
		Lake::Stream st;
		std::array<s32, 3> at{ r.x, r.z, r.top };
		std::unordered_set<s64> been{ column(r.x, r.z) };
		bool ended = false;
		for (int step = 0; step < STREAM_MOST && !ended; step++) {
			st.path.push_back(at);
			taken.insert(column(at[0], at[1]));
			if (joinsWater(server, at[0], at[1], at[2])) {
				// a river, the sea: its water's theirs now (a river's channel carries it on)
				const std::pair<s32, s32> run = terraRiverRun(server, v3s16(at[0], at[2] - 1, at[1]));
				st.joins_cell = run.first;
				st.joins_along = run.second;
				break;
			}
			// another lake's water: it feeds that lake
			bool into = false;
			for (const std::vector<Lake> *lakes : { &t.lakes, &born })
				for (const Lake &other : *lakes)
					if (!into && &other != &lake && other.wet.count(column(at[0], at[1]))) {
						st.fills = other.seed;
						into = true;
					}
			if (into)
				break;
			// the steepest way down; on a flat, across it to the nearest way
			// down (or water), the shortest way over the flat
			const rivers::Basin::Top top = mapTop(server, at[2]);
			std::optional<std::array<s32, 3>> next;
			for (const auto &s : SIDES) {
				const s32 nx = at[0] + s[0], nz = at[1] + s[1];
				if (in.count(column(nx, nz)) || been.count(column(nx, nz)))
					continue;
				const s32 nt = top(nx, nz);
				if (nt != rivers::Basin::UNKNOWN && nt < at[2] && (!next || nt < (*next)[2]))
					next = std::array<s32, 3>{ nx, nz, nt };
			}
			if (next) {
				been.insert(column((*next)[0], (*next)[1]));
				at = *next;
				continue;
			}
			std::vector<std::array<s32, 3>> flat{ at };
			std::unordered_map<s64, size_t> from{ { column(at[0], at[1]), 0 } };
			std::vector<size_t> parent{ 0 };
			std::optional<size_t> found;
			for (size_t f = 0; f < flat.size() && f < 4096 && !found; f++) {
				const std::array<s32, 3> c = flat[f];
				for (const auto &s : SIDES) {
					const s32 nx = c[0] + s[0], nz = c[1] + s[1];
					if (in.count(column(nx, nz)) || been.count(column(nx, nz)) || from.count(column(nx, nz)))
						continue;
					const s32 nt = top(nx, nz);
					if (nt == rivers::Basin::UNKNOWN || nt > at[2])
						continue;
					from.emplace(column(nx, nz), flat.size());
					flat.push_back({ nx, nz, nt });
					parent.push_back(f);
					if (nt < at[2] || joinsWater(server, nx, nz, nt)) {
						found = flat.size() - 1;
						break;
					}
				}
			}
			if (!found) {
				// a hollow: nothing lower across the flat it's on: it fills it
				if (lake.generation < GENERATIONS)
					st.fills = v3s16(at[0], at[2] - 1, at[1]);
				ended = true;
				break;
			}
			// the way across, then on from where it goes down
			std::vector<std::array<s32, 3>> way;
			for (size_t k = *found; k != 0; k = parent[k])
				way.push_back(flat[k]);
			for (auto w = way.rbegin(); w != way.rend(); ++w) {
				been.insert(column((*w)[0], (*w)[1]));
				if (std::next(w) != way.rend()) {
					st.path.push_back(*w);
					taken.insert(column((*w)[0], (*w)[1]));
				}
			}
			at = way.front();
		}
		lake.streams.push_back(std::move(st));
	}
	// the hollows its streams end in: lakes of their own, fed by them
	for (const Lake::Stream &st : lake.streams) {
		if (!st.fills)
			continue;
		const v3s16 seed = *st.fills;
		bool lake_there = false;
		for (const std::vector<Lake> *lakes : { &t.lakes, &born })
			for (const Lake &other : *lakes)
				lake_there = lake_there || other.seed == seed;
		if (lake_there)
			continue; // (a lake it runs into: fed, nothing to find)
		bool have = false;
		for (const std::vector<Lake> *lakes : { &t.lakes, &born })
			for (const Lake &other : *lakes)
				have = have || other.seed == seed || other.cols.count(column(seed.X, seed.Z));
		if (have)
			continue;
		rivers::Basin basin = rivers::Basin::find(seed.X, seed.Z, seed.Y + 1, mapTop(server, seed.Y + 1), MOST,
				[server](s32 x, s32 z, s32 top) { return joinsWater(server, x, z, top); });
		if (!basin.dammed())
			continue; // (it runs on from there after all: a flat it wandered off)
		Lake hollow;
		hollow.seed = seed;
		hollow.level0 = hollow.level = hollow.written = seed.Y + 1;
		hollow.generation = lake.generation + 1;
		found(hollow, std::move(basin));
		actionstream << "lakes: a stream from the lake at " << lake.seed << " fills a hollow at " << seed << " to "
				<< hollow.basin->spill() << " over " << hollow.basin->columns().size() << " columns" << std::endl;
		born.push_back(std::move(hollow)); // (into the lakes once they've all stepped)
	}
	// an edit across a stream's way looks again (dug across, built over: it runs another way)
	for (const Lake::Stream &st : lake.streams)
		for (const std::array<s32, 3> &c : st.path)
			lake.touch.insert(column(c[0], c[1]));
	actionstream << "lakes: the lake at " << lake.seed << " runs out by " << lake.streams.size() << " streams:";
	for (const Lake::Stream &st : lake.streams) {
		const std::array<s32, 3> &end = st.path.back();
		actionstream << " " << st.path.size() << " columns to (" << end[0] << ", " << end[2] << ", " << end[1] << ") "
				<< (st.fills ? "filling a hollow" : joinsWater(server, end[0], end[1], end[2]) ? "into water" : "lost") << ";";
	}
	actionstream << std::endl;
}

void terraStepLakes(Server *server, float dtime)
{
	ServerTerra &t = server->terra();
	if (!t.rivers)
		return;
	if (!t.lakes_loaded)
		load(server, t);
	std::vector<v3s16> edits;
	edits.swap(t.lake_edits);
	for (const v3s16 &p : edits)
		lookAt(server, t, p);
	size_t budget = WRITE_BUDGET;
	std::vector<Lake> born; // hollows streams found this step
	// what streams brought rivers last step (rebuilt as each lake's streams run)
	std::unordered_map<s32, std::vector<std::pair<s32, double>>> joins;
	for (Lake &lake : t.lakes) {
		if (lake.refind)
			refind(server, lake);
		if (!lake.basin)
			continue;
		if (lake.flood) {
			// as high as its river's flow puts it: up slowly over the ground, down as it drains
			const double target = std::max<double>(lake.level0, lake.target);
			if ((s32)std::ceil(target) + 1 > lake.basin->spill())
				refind(server, lake); // higher than it was spread for
			lake.inflow = terraRiverDischarge(server, lake.seed);
			if (lake.level < target)
				lake.level = std::min(target, lake.level + FLOOD_RISE * dtime);
			else
				lake.level = std::max(target, lake.level - DRAIN * dtime);
			write(server, t, lake, budget);
			continue;
		}
		const rivers::Basin &b = *lake.basin;
		lake.inflow = terraRiverDischarge(server, lake.seed) + lake.fed;
		// the rain on it and what the air takes off it, the weather's there (each rivers step)
		if (lake.weather_step != t.rivers->step) {
			lake.weather_step = t.rivers->step;
			const std::pair<double, double> w = terraLakeWeather(server, lake.seed);
			lake.rain = w.first;
			lake.evaporation = w.second;
		}
		if (!b.dammed()) {
			// its dam gone, nothing holds it: down to its river (the way out it
			// had is under its river's level now)
			lake.outflow = 0;
			lake.level = std::max<double>(lake.level0, lake.level - DRAIN * dtime);
		} else {
			// its river in, rain less evaporation over its surface, and what
			// pours out over its rim: rising to its spill and over it till its
			// way out passes what comes in, or standing where its losses take it
			// all (a closed lake). In steps short enough for a small lake's weir
			const double was = lake.level;
			const int steps = 8;
			for (int k = 0; k < steps; k++) {
				const double area = std::max(1.0, b.area(lake.level));
				lake.outflow = b.outflow(lake.level);
				const double net = lake.inflow + (lake.rain - lake.evaporation) / 3.6e6 * area - lake.outflow; // m³/s
				lake.level = std::max<double>(lake.level0, lake.level + net * dtime / steps / area);
			}
			if (was < b.spill() && lake.level >= b.spill())
				actionstream << "lakes: the lake at " << lake.seed << " is full: it pours out over " << b.spill() << " by ("
						<< b.outX() << ", " << b.outZ() << ")" << std::endl;
		}
		// over its spill by another block: it covers what that reaches, and pours
		// out where that falls away (its streams found again from there)
		if (b.dammed() && (s32)std::floor(lake.level) > lake.risen) {
			const s32 to = (s32)std::floor(lake.level);
			rivers::Basin risen = *lake.basin;
			risen.rise(to + 1, mapTop(server, to), MOST);
			found(lake, std::move(risen));
			lake.risen = to;
			lake.routed = false;
		}
		// pouring out: its streams run (found once it does, and again when its
		// ground changes); each takes its outlet's share by the weir; dry, they go
		if (lake.outflow > 1e-3 && !lake.routed)
			route(server, t, lake, born);
		else if (lake.outflow <= 1e-3 && !lake.streams.empty()) {
			lake.streams.clear();
			lake.routed = false;
		}
		for (Lake::Stream &st : lake.streams) {
			const std::array<s32, 3> &o = st.path.front();
			st.q = lake.level > o[2] ? rivers::Basin::WEIR * std::pow(lake.level - o[2], 1.5) : 0;
			if (st.joins_cell >= 0)
				joins[st.joins_cell].emplace_back(st.joins_along, st.q);
			if (!st.fills)
				continue;
			for (Lake &other : t.lakes)
				if (other.seed == *st.fills)
					other.fed_next += st.q;
		}
		write(server, t, lake, budget);
	}
	for (Lake &lake : born)
		t.lakes.push_back(std::move(lake));
	t.river_joins.swap(joins);
	for (Lake &lake : t.lakes) {
		lake.fed = lake.fed_next;
		lake.fed_next = 0;
	}
	// drained back to its river: no lake
	const size_t before = t.lakes.size();
	t.lakes.erase(std::remove_if(t.lakes.begin(), t.lakes.end(), [](const Lake &l) {
		return l.basin && (l.flood ? l.target <= l.level0 : !l.basin->dammed()) && l.level <= l.level0 && l.wet.empty();
	}), t.lakes.end());
	if (t.lakes.size() != before)
		actionstream << "lakes: " << before - t.lakes.size() << " drained back to their rivers" << std::endl;
	if ((t.lakes_saved += dtime) >= SAVE_EVERY || t.lakes.size() != before) {
		t.lakes_saved = 0;
		save(server, t);
	}
}

void terraFlood(Server *server, v3s16 node, double surge)
{
	ServerTerra &t = server->terra();
	ServerMap &map = server->getEnv().getServerMap();
	const NodeDefManager *ndef = server->ndef();
	if (!isRiverWater(map.getNode(node), ndef))
		return;
	const v3s16 top = poolTop(map, ndef, node);
	const s32 level0 = top.Y + 1;
	const double target = surge > BRIM ? level0 + surge : level0;
	for (Lake &lake : t.lakes) {
		if (!lake.touch.count(column(top.X, top.Z)) && !lake.wet.count(column(top.X, top.Z)))
			continue;
		if (!lake.flood)
			return; // dammed: its lake's, not its river's
		if (lake.level0 != level0)
			continue; // the next step's flood, over this pool's banks
		// the highest of its pool's blocks this step (each its own depth)
		lake.target = lake.target_step == t.rivers->step ? std::max(lake.target, target) : target;
		lake.target_step = t.rivers->step;
		return;
	}
	if (surge <= BRIM)
		return;
	Lake lake;
	lake.flood = true;
	lake.seed = top;
	lake.level0 = level0;
	lake.level = level0;
	lake.written = level0;
	lake.target = target;
	lake.target_step = t.rivers->step;
	found(lake, rivers::Basin::spread(top.X, top.Z, level0, (s32)std::ceil(target) + 1, mapTop(server, level0), MOST));
	actionstream << "lakes: a river over its banks at " << top << ": " << surge << " blocks over, onto "
			<< lake.basin->columns().size() << " columns" << std::endl;
	t.lakes.push_back(std::move(lake));
}

double terraLakeBalance(const Lake &lake)
{
	if (!lake.basin || !lake.basin->dammed())
		return lake.level0;
	const rivers::Basin &b = *lake.basin;
	const double loss = (lake.evaporation - lake.rain) / 3.6e6; // m/s off its surface
	auto net = [&](double h) { return lake.inflow - loss * std::max(1.0, b.area(h)); };
	if (net(b.spill() - 1e-6) >= 0)
		return b.spill(); // it fills: an open lake
	double lo = lake.level0, hi = b.spill();
	for (int i = 0; i < 40; i++) {
		const double mid = (lo + hi) / 2;
		(net(mid) > 0 ? lo : hi) = mid;
	}
	return lo;
}

std::optional<s16> terraLakeRise(Server *server, v3s16 block)
{
	const ServerTerra &t = server->terra();
	for (const Lake &lake : t.lakes) {
		if (!lake.basin || lake.written <= lake.level0 || !lake.blocks.count(column(block.X, block.Z)))
			continue;
		if (getContainerPos((s16)(lake.written - 1), MAP_BLOCKSIZE) != block.Y)
			continue;
		return (s16)std::lround((lake.level - lake.written) * 16);
	}
	return std::nullopt;
}
