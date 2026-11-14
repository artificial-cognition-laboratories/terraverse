// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "channels.h"
#include "grid.h"
#include "noise.h"
#include "plates.h"
#include "world.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <stdexcept>
#include <vector>

namespace terragen {

using plates::mix;
using plates::unit;

namespace {

enum ChannelStream : uint32_t { NODE = 353, BEND = 379, CLUMPS = 439, PONDS = 443, BEDS = 449, SLOPES = 457, ROCK = 461, ROLLS = 463, ROUTE = 467, WANDER = 479 }; // (359 was the levels' wander)
// how far a level's channels bend either way: its lattice over BEND_SHARE,
// by noise BEND_WAVES times its waves (the cells' lattice: about its own).
// Gentle enough never to fold, every level's together: the bend's slope
// (amplitude over wavelength) well under one. At a quarter of the lattice
// over noise half as wide, a bend's slope reached π: its line turned back on
// itself, a stream in loops and pieces (6–25% of the ground on the finer
// levels; the stream probe, phase 2)
const int64_t BEND_SHARE = 12;
const int32_t BEND_WAVES = 2;

// The finest level's points are at least this far apart (blocks). What a
// channel drains is counted over the levels AREA_FINEST_BLOCKS apart or more:
// the finer are side valleys, each draining its own cell, so how fine the
// network goes never moves the coarser channels (sprint 109 P5: a level of
// side valleys down to 24, where the hillsides were planes under 48; sprint
// 112: to 12, so the smallest planet, whose cells are 36 blocks, has a level
// of its own and its ranges are valleys and ridges, not domes under noise)
const int64_t FINEST_BLOCKS = 12, AREA_FINEST_BLOCKS = 48;
// The levels that carry water: those whose lattice is at least STREAM_LEAST_M
// across as the Earth would have it (the planet's breadth over the Earth's: a
// stream drains a square kilometre or two), and STREAM_LEAST_BLOCKS (a small
// planet's are crowded to the block). By level, three, a small planet's
// streams came down to a valley 375 blocks wide and the Earth's to one 12 km
// wide: on a big planet only big rivers ran (sprint 110)
const int64_t STREAM_LEAST_M = 1500, STREAM_LEAST_BLOCKS = 150;
// Rain a level needs to run with water (the grid's rain: 1 is average), from
// the coarsest wet level's to the finest's: the wetter the land, the further
// up its network the water reaches
const fix WET_COARSE = ratio(30, 100), WET_FINE = ratio(120, 100);
// A level's stream's width as the noise land draws it (blocks, fixed), the
// coarsest's to the finest's, and how wide its valley is, of its lattice
const int64_t STREAM_WIDTH_COARSE = 5 * ONE, STREAM_WIDTH_FINE = 2 * ONE;
const fix VALLEY_WIDTH = ratio(55, 100);
// Each level's share of the relief, falling as real land's roughness does
const fix FALL = 37641; // 2^−0.8
// How much further a join across folded beds counts, squared
const fix TRELLIS = toFix(16);
// …and in a rising range's thrust sheets, this much of it (sprint 112)
const fix STRIKE = ratio(50, 100);
// The land from the drainage (sprint 108). A channel's slope is stream
// power's at steady state, S = ks / √(its flow), ks the grid's own run's (so
// a range's channels are steep and a plain's gentle, as the grid wore them);
// never under SLOPE_LEAST, nor over the hillsides' (critical(): a channel's
// head is a hillside's gully). Hillsides are the steepest ground there is:
// they rise from every channel at the critical slope, so the channels, gentler,
// cut valleys into them, and divides stand where two hillsides meet.
const fix SLOPE_LEAST = ratio(1, 4000);
const fix HILL_LEAST = ratio(1, 100), CRITICAL_RANGE = ratio(55, 100);
// On a smaller planet than the Earth the land's heights are exaggerated (they
// shrink as √ of its size, its breadth as all of it), and its hillsides with
// them, to this many times as steep (sprint 112): at the Earth's 0.55 a small
// planet's ranges rose like its exaggerated land and were grooved by valleys
// with the Earth's hillsides, a broad rise and no peaks between them
const fix STEEP_MOST = toFix(4);
// Where two hillsides meet the divide's rounded over this many of the finest
// level's lattice (soil creeps over it: a smooth minimum, not a knife edge)
const fix ROUNDING = ratio(1, 2);
// A grid run's trunk valley follows its line (World::onRun) in this many pieces
const int TRUNK_PIECES = 12;
// A run draining less than this (cells of average rain) isn't a trunk
// valley: it's a hollow in a hillside, and the finer levels shape it. As
// valleys every run on a range's flank cut its own, a cell apart, and the
// flank was a comb of parallel spurs
const int64_t TRUNK_LEAST = 3 * ONE;
// The land is seamless: what a place reads is gathered per window, so each
// channel's hillside rises away before any window could leave it out, and the
// lowest never jumps where windows meet. A trunk run's by its distance, over
// TRUNK_REACH_FROM..TRUNK_REACH of the grid's spacing; a finer channel's by
// its head's, over REACH_FROM..REACH of its lattice, its window WINDOW
// lattices each way (its own bend, up to a BEND_SHARE of one, kept inside).
// Over the band it rises as its square, up to FADE_SLOPE blocks a block at
// its end, and by FADE_TO only in its last FADE_STEEP: rising by FADE_TO all
// the way, a gentle valley's channel still the lowest past its reach (its
// neighbours steep) climbed the band's first blocks as a cliff, tens of
// blocks high (P4); steeper ramps stood as walls round the channels' heads.
// The ramp's at least FADE_LEAST high: a fine level's band is a few blocks
// wide, too little to climb to its neighbours before FADE_TO (P5)
// A finer channel's land is never the lowest past 1¾ of its lattice from its
// head, so it fades over 1½..2 (it was 2..2½: a third more read a column,
// for 8% of columns a block or two different and no steeper)
const fix TRUNK_REACH_FROM = toFix(1), TRUNK_REACH = ratio(160, 100);
const fix REACH_FROM = ratio(150, 100), REACH = toFix(2);
const int32_t WINDOW = 3;
const int64_t FADE_TO = (int64_t)1 << 32; // blocks, fixed: over any land
const fix FADE_SLOPE = toFix(1), FADE_STEEP = ratio(10, 100);
const int64_t FADE_LEAST = toFix(32); // blocks, fixed
// A channel carries a stream once this much drains to it (blocks² of average
// rain: a few hundred blocks square), as wide as World::riverWidth says
const int64_t STREAM_FLOW = 40000;
// A tributary meets what it joins heading downstream with it: it joins this
// far down from its nearest point, of how far off it is (cot 60°: it comes in
// at 60°), and never where that faces upstream or comes in squarer than 75°. Joining the nearest point,
// it came in square, and bent by a hair, against the flow (a quarter of joins)
const fix JOIN_DOWN = ratio(58, 100);
const int64_t JOIN_SQUARE = 7;
// A level's node runs into the level above's channel once this near it (of
// its lattice); further, down its own level's chain (build())
const fix JOIN_NEAR = ratio(90, 100);
// A channel's way wanders over the lie of the land (trace()): noise
// WANDER_WAVES times its level's waves (a few to a lattice), each turn
// costing WANDER_TURN of the noise's range by its sine
const int32_t WANDER_WAVES = 3;
const fix WANDER_TURN = ratio(15, 100);
// How rough the ground a level's chains wander over is: ROUTE_ROUGH of a
// lattice's climb either way, by noise about 2 / ROUTE_WAVES lattices wide (rise())
const fix ROUTE_ROUGH = ratio(150, 100);
const int32_t ROUTE_WAVES = 1; // cos² 75°, in hundredths: the squarest it comes in
const int64_t RUN_MOST = 11; // the longest a run to the level above, in quarters of its lattice along any axis…
const int64_t FAR_COST = (int64_t)1 << 56; // …unless there's no other: what a longer one costs more
// The steepest a channel runs, of its hillsides' slope
const fix CHANNEL_SHARE = ratio(60, 100);
// Rock strength (P3): how hard each province's rock is to running water and
// creep, 0..1. Hillsides stand at STANDS_SOFT..+STANDS_SPAN of the critical
// slope by it, and channels' steepness index with them (stream power's
// erodibility: soft rock's channels are gentler for the same uplift)
const fix STRENGTH[(int)Province::Count] = {
	ONE,            // shield: crystalline basement
	ratio(90, 100), // orogen
	ratio(70, 100), // arc
	ratio(50, 100), // rift
	ratio(25, 100), // basin: shale and sandstone
	ratio(60, 100), // volcanic
	ratio(30, 100), // ocean floor
	ratio(80, 100), // highland
	ratio(60, 100), // mare
};
// The grid's heights are the land's on average; valleys climbing freely from
// the trunk stood a range's ground a quarter over them (sprint 109 P6). Ground
// over the grid's surface by e stands e·OVER_GRID/(OVER_GRID + e) over it:
// a ridge a little over barely changes, a summit far over comes most of the
// way down; one rising map for land and water, as BEDDED's. OVER_GRID_M is
// metres, in blocks as heights are (blocksPerMetre): 300 blocks on the
// default planet. (As 300 blocks on any, an Earth's ranges stood 300 m over
// their grid: 45 m of relief in a kilometre, 170 in ten; real ranges have
// 300–600 and 1500–2500: sprint 110)
const int64_t OVER_GRID_M = 2400;
// …and in a rising range, this many times as far (sprint 112): the grid's
// height is a cell's mean, its valleys and peaks under and over it, and a
// range held to OVER_GRID_M over it stood as a plateau, its hillsides climbing
// into a lid (48 blocks on the smallest planet)
const int64_t RANGE_OVER = 6;
const fix STANDS_SOFT = ratio(70, 100), STANDS_SPAN = ratio(60, 100);
// The ground decides the land's texture (sprint 109 P3). Within a province
// rock varies by ROCK_VARY either way, over a few of the first level's
// lattices (harder and softer beds, jointing). Drainage is denser on soft
// ground and where it's wet (more of a level's points are channel heads:
// up to DENSE_SOFT and DENSE_WET more, as much less on hard dry ground), so
// valleys crowd on shale and stand apart on granite; hillsides stand steeper
// on harder rock, so its valleys are deeper; and a channel would rather run
// through softer rock: a join's cost rises by DEFLECT of how hard the ground
// is where it joins (its province's: the rock's own variation is too dear
// to read for every candidate)
const fix ROCK_VARY = ratio(30, 100);
const fix DENSE_SOFT = ratio(45, 100), DENSE_WET = ratio(20, 100);
const fix DEFLECT = ratio(60, 100);
// Gentle land rolls (P4): swells and hollows ROLL_HIGH blocks either way,
// ROLL_WAVE blocks across (two octaves), under hillsides gentler than
// ROLL_SLOPE (steeper ones are all the drainage's); clear of channels' floors
// and banks (fading in over ROLL_CLEAR blocks above the bed, never sinking
// more than half way to it), so the water and its floodplains are as they were
const int64_t ROLL_HIGH = toFix(5), ROLL_CLEAR = toFix(6);
const int64_t ROLL_WAVE = 600; // blocks
// The planet the rolls' and beds' heights were tuned on (blocks round): on a
// smaller one, whose heights are all less (blocksPerMetre), they're less too,
// or a 100-block range stood in 5–20-block terraces (sprint 112)
const int32_t TUNED_SIZE = 100000;
const fix ROLL_SLOPE = ratio(25, 100);
// Flat-lying beds (P3): how much of each province's surface is layered
// sediment, hard beds over soft, each BED_LEAST..BED_MOST blocks thick by
// noise; where hillsides cut through them, the soft wears back under the
// hard and the land steps: a cliff at each hard bed, a bench on it
// (escarpments, mesas, buttes, canyons' walls). Folded beds aren't flat
const fix BEDDED[(int)Province::Count] = {
	ratio(35, 100), // shield: a platform's cover
	0, 0,
	ratio(50, 100), // rift
	ONE,            // basin
	0, 0,
	ratio(30, 100), // highland
	0,
};
// Fans (P4): where a channel steeper than FAN_STEEPER× what it joins comes
// out onto a floor gentler than FAN_FLOOR, it drops what it carries: a cone
// round a point half its radius up it, falling FAN_GRADE as steep as the
// channel (so it buries the channel's last reach and spreads past its foot,
// its toe at the foot's height), its radius
// FAN_SHARE of √ its catchment (FAN_LEAST..FAN_MOST blocks, no more than
// its length), wider where it's dry (washes build fans; wet valleys
// carry their load on)
const int64_t FAN_STEEPER = 2;
const fix FAN_FLOOR = ratio(4, 100), FAN_GRADE = ratio(60, 100), FAN_SHARE = ratio(30, 100);
const int64_t FAN_LEAST = 16, FAN_MOST = 320; // blocks
// its width × 1.6 where the rain's FAN_DRY or less, down to × 0.6 at FAN_WET
const fix FAN_DRY = ratio(30, 100), FAN_WET = toFix(3);
// Climate in the valleys (P6). Where ice worked a valley (World::glacial)
// it's a trough: a floor TROUGH_SHARE of √ its catchment wide each side (to
// TROUGH_MOST blocks) and walls steepening to TROUGH_WALL, where its
// hillsides are a range's. Where little rain
// falls (under WASH_RAIN) a channel's a wash: a flat sandy floor WASH_SHARE
// of √ its catchment each side (to WASH_MOST), dry most of the year, banks cut
// steep (flash floods spread and drop sand, nothing grows to hold banks back)
const fix TROUGH_SHARE = ratio(8, 100), TROUGH_WALL = ratio(90, 100);
const fix TROUGH_HILL = ratio(15, 100); // troughs from hillsides this steep, fully at twice
const int64_t TROUGH_MOST = 160; // blocks
// Cirques (sprint 112): where ice worked a channel's head on the level whose
// channels are a cirque's size (CIRQUE apart), the head's a bowl: its floor
// near level from CIRQUE_FROM up the channel, CIRQUE_SHARE of the channel's
// length across (CIRQUE_LEAST..CIRQUE_MOST blocks, by how much ice), under a
// headwall CIRQUE_WALL steep (a cirque's back wall stands 30–50°; between
// heads, the walls meet in arêtes)
const fix CIRQUE_FROM = ratio(55, 100), CIRQUE_SHARE = ratio(30, 100), CIRQUE_WALL = toFix(1);
const int64_t CIRQUE_LEAST = 24, CIRQUE_MOST = 220; // blocks
const fix WASH_RAIN = ratio(50, 100), WASH_SHARE = ratio(5, 100);
const int64_t WASH_MOST = 60; // blocks
const int64_t BED_LEAST = 14, BED_MOST = 48; // blocks
const fix BED_HARD = ratio(20, 100);         // the share of a bed that's its hard cap
const fix BED_SOFT_SLOPE = ratio(50, 100);   // the soft part stands this much of its height's share
// where the beds show: only on hillsides at least this steep (on a plain a
// step would be a scarp across every field), fully by BED_SLOPE_FULL
const fix BED_SLOPE = ratio(6, 100), BED_SLOPE_FULL = ratio(20, 100);
// A node's rough height guesses it drains this many lattice cells (its own
// and its children's)
const int64_t ROUGH_CELLS = 2;
// The prototype's density of a level's points: never under DENSE_LEAST of them
const fix DENSE_LEAST = ratio(30, 100);
// Hillsides aren't planes: how steep they stand wanders by place, VARY of
// it either way over a few of the grid's cells (a convex spur, a slope eased
// by an old slump), so a valley's walls curve rather than stand as facets
const fix VARY = ratio(30, 100);
// A channel's floor: where it runs gentler than FLOOR_GRADE it lays its load
// down, a flat floor FLOOR_WIDTHS × √flow wide (a floodplain), none steeper
const fix FLOOR_GRADE = ratio(2, 100);
const int64_t FLOOR_WIDTHS = 4;
const int32_t FLOOR_MOST = 40;
// The network carries its water (phase 2 of the streams: water followed the
// lowest land, and another valley's lower hillside took it half the time): a
// wet channel's water runs down its whole line, and the land round it gives
// way. Its banks stand BANK_HIGH over its water for BANK_WIDE blocks either
// side, falling CREST_FALL a block past them where the land beyond is lower
// (a stream on a spur's shoulder, not perched over the next valley), and the
// land's cut down to its water where anything stood over it (a fan, a sill,
// the rolls); both within CORRIDOR blocks and widths of it
const int64_t BANK_HIGH = toFix(1), BANK_WIDE = toFix(2), CORRIDOR = toFix(8);
const int64_t CORRIDOR_WIDTHS = 3;
const fix CREST_FALL = ratio(33, 100);
// Ponds, where something held the water up (a sill across the channel, and
// a hollow worn or left behind it): tarns where the ice worked a channel's
// head (a cirque's floor scoured out, its lip of rock or moraine), only on the
// level whose channels are a cirque's size (nearest CIRQUE blocks apart), up
// to TARNS a km² of its lattice where it worked hardest; pools on floodplains
// (oxbows, sinks, a beaver's dam), POOLS a km² of a level's lattice where its
// channel has a floor. By area, not a share of channels: a share rolled on
// every level stacked them, 19 tarns a km² over high ground and 5 pools a km²
// on hillsides (sprint 109 P4; a glaciated range holds well under one). A
// pond's sill rises round it for half its radius more.
const int64_t CIRQUE = 600; // blocks
const fix TARNS = toFix(1), POOLS = toFix(1);
const int64_t TARN_LEAST = 12, TARN_MOST = 70, POOL_LEAST = 8, POOL_MOST = 60; // radius, blocks
// A pond is its own valley's: it fades out as its channel's ground stands
// over the lowest by up to POND_OWN (blocks, fixed)
const int64_t POND_OWN = toFix(8);

struct Near
{
	int64_t dist2; // place units²
	fix t;
};

// The closest a place comes to a segment. No wider than 64 bits: this runs
// for every segment near every column, so no 128-bit division and no root
Near nearest(const Place &p, const Place &a, const Place &b)
{
	const int64_t sx = (int64_t)b.x - a.x, sy = (int64_t)b.y - a.y, sz = (int64_t)b.z - a.z;
	const int64_t px = (int64_t)p.x - a.x, py = (int64_t)p.y - a.y, pz = (int64_t)p.z - a.z;
	// (a segment and the reach round it are under a few grid cells: 2^27 at most)
	const int64_t len2 = sx * sx + sy * sy + sz * sz;
	const int64_t on = px * sx + py * sy + pz * sz;
	fix t = 0;
	if (len2 > 0 && on > 0)
		t = on >= len2 ? ONE : (fix)(on / std::max<int64_t>(1, len2 >> FRAC));
	t = std::min(t, ONE);
	const int64_t dx = px - (sx * t >> FRAC), dy = py - (sy * t >> FRAC), dz = pz - (sz * t >> FRAC);
	return { dx * dx + dy * dy + dz * dz, t };
}

Place along(const Place &a, const Place &b, fix t)
{
	return placeOf((int64_t)a.x + mulShift((int64_t)b.x - a.x, t, FRAC), (int64_t)a.y + mulShift((int64_t)b.y - a.y, t, FRAC),
			(int64_t)a.z + mulShift((int64_t)b.z - a.z, t, FRAC));
}

// Ground `h` over the grid's surface `g` brought towards it, bending E over it (OVER_GRID_M)
int64_t keepWith(int64_t h, int64_t g, int64_t E)
{
	return h <= g ? h : g + (h - g) * E / (E + h - g);
}

// Where a tributary from `at` meets a → b, water running a → b (`downstream`
// +1) or b → a (−1): JOIN_DOWN of how far off it is down from its nearest
// point (`near`, moved there), so it comes in heading down with it. False
// where that still faces upstream (past the downstream end): no join there
bool joinDownstream(const Place &at, const Place &a, const Place &b, int downstream, Near &near)
{
	const int64_t sx = (int64_t)b.x - a.x, sy = (int64_t)b.y - a.y, sz = (int64_t)b.z - a.z;
	const int64_t length = (int64_t)isqrt((uint64_t)(sx * sx + sy * sy + sz * sz));
	if (length > 0) {
		const int64_t off = (int64_t)isqrt((uint64_t)near.dist2);
		const int64_t by = std::min<int64_t>(ONE, mulShift(off, JOIN_DOWN, FRAC) * ONE / length);
		const int64_t t = near.t + by * downstream;
		// (not past its foot: there it'd turn the corner into what that
		// joins, crossing what else meets it; it joins that instead)
		if (t < 0 || t > ONE)
			return false;
		near.t = (fix)t;
	}
	const Place on = along(a, b, near.t);
	const int64_t vx = (int64_t)on.x - at.x, vy = (int64_t)on.y - at.y, vz = (int64_t)on.z - at.z;
	near.dist2 = vx * vx + vy * vy + vz * vz;
	const __int128 with = ((__int128)vx * sx + (__int128)vy * sy + (__int128)vz * sz) * downstream;
	// (at 75° or less: where it slid to an end, it may still come in square)
	const __int128 v2 = (__int128)near.dist2, s2 = (__int128)sx * sx + (__int128)sy * sy + (__int128)sz * sz;
	return with > 0 && with / 64 * (with / 64) * 100 > v2 / 64 * (s2 / 64) * JOIN_SQUARE;
}

int32_t floorBy(int64_t v, int64_t d)
{
	return (int32_t)(v >= 0 ? v / d : -((-v + d - 1) / d));
}

fix lerpFix(fix a, fix b, fix t) { return a + mul(b - a, t); }

// A cache per thread, per level (Ways): the generator's threads and the far
// view ask for the same few nodes over and over
constexpr size_t SLOTS = 1 << 13;

// One thread's cache of something per node, two ways a set: a node's in
// either of its pair of slots, the less recently used replaced (direct-mapped,
// nodes near each other evicted each other: a world's river levels read
// 5.8 → 4.8 s from this alone)
// Each Channels' own mark on what it keeps in the threads' caches (never 0,
// an empty slot's). Not its seed and size: worlds sharing those (another's
// settings, a world's prototype land) were handed each other's nodes, by
// whichever thread had built them first
uint64_t nextOwner()
{
	static std::atomic<uint64_t> next{ 1 };
	return next++;
}

template <typename T>
class Ways
{
public:
	struct Entry
	{
		uint64_t owner = 0;
		int32_t i = 0, j = 0, l = 0;
		T value;
	};

	Ways() : m_entries(new Entry[SLOTS * (Channels::MOST_LEVELS + 1)]), m_newer(new uint8_t[SLOTS / 2 * (Channels::MOST_LEVELS + 1)]()) {}

	// what's kept for the node, or none
	T *find(uint64_t owner, int level, int32_t i, int32_t j, int32_t l, uint32_t h)
	{
		const size_t set = setOf(level, h);
		for (size_t w = 0; w < 2; w++) {
			Entry &e = m_entries[set * 2 + w];
			if (e.owner == owner && e.i == i && e.j == j && e.l == l) {
				m_newer[set] = (uint8_t)w;
				return &e.value;
			}
		}
		return nullptr;
	}

	// kept in place of the less recently used of its set
	T &put(uint64_t owner, int level, int32_t i, int32_t j, int32_t l, uint32_t h, T value)
	{
		const size_t set = setOf(level, h), w = m_newer[set] ^ 1;
		Entry &e = m_entries[set * 2 + w];
		m_newer[set] = (uint8_t)w;
		e = { owner, i, j, l, std::move(value) };
		return e.value;
	}

private:
	static size_t setOf(int level, uint32_t h) { return (size_t)level * (SLOTS / 2) + (h & (SLOTS / 2 - 1)); }

	std::unique_ptr<Entry[]> m_entries;
	std::unique_ptr<uint8_t[]> m_newer;
};

} // namespace

// Paths kept for every thread, in shards by node, each emptied once it holds
// TRACED_MOST (a world's never needs them all at once; a thread's own cache
// in front keeps it from asking often)
constexpr size_t TRACED_SHARDS = 64, TRACED_MOST = 1 << 14;
struct Channels::Traced
{
	struct Shard
	{
		std::mutex lock;
		std::unordered_map<uint64_t, Path> paths;
	};
	std::array<Shard, TRACED_SHARDS> shards;
	static uint64_t key(int level, int32_t i, int32_t j, int32_t l)
	{
		return (uint64_t)level << 60 ^ ((uint64_t)(uint32_t)i & 0xFFFFF) << 40 ^ ((uint64_t)(uint32_t)j & 0xFFFFF) << 20 ^ ((uint64_t)(uint32_t)l & 0xFFFFF);
	}
};

Channels::Channels(const WorldCode &code, const Grid &grid, const World &world, int64_t radius, int64_t cell_blocks, bool land) :
	m_grid(&grid), m_world(&world), m_seed(code.seed_hash + NODE), m_owner(nextOwner()),
	m_radius(radius), m_water(code.processes.water), m_land(land),
	m_pieces(new std::atomic<Pieces *>[grid.size()]()), // (all null)
	m_traced(new Traced()), m_ways(new Traced()), m_wanders(new Traced())
{
	m_lattice[0] = (int64_t)gridSphere().spacing() << (PLACE_FRAC - FRAC);
	// only where water ever ran
	if (!code.processes.water && code.processes.ancient_water <= 0)
		return;
	int64_t blocks = cell_blocks >> FRAC;
	for (int k = 1; k <= MOST_LEVELS && blocks / 2 >= FINEST_BLOCKS; k++) {
		blocks /= 2;
		m_lattice[k] = m_lattice[k - 1] / 2;
		m_levels = k;
		if (blocks >= AREA_FINEST_BLOCKS)
			m_area_levels = k;
	}
	// the wet levels (STREAM_LEAST_*): a lattice's blocks × R⊕ / R, as metres
	{
		int64_t across = cell_blocks >> FRAC;
		for (int k = 1; k <= m_levels; k++) {
			across /= 2;
			const int64_t earth_m = (int64_t)(((__int128)across * EARTH_RADIUS_M << FRAC) / std::max<int64_t>(1, radius));
			if (across >= STREAM_LEAST_BLOCKS && earth_m >= STREAM_LEAST_M)
				m_wet_levels = k;
		}
		for (int k = 1; k <= m_wet_levels; k++) {
			const fix t = m_wet_levels > 1 ? ratio(k - 1, m_wet_levels - 1) : 0;
			m_wet[k] = WET_COARSE + mul(WET_FINE - WET_COARSE, t);
			m_stream_width[k] = STREAM_WIDTH_COARSE + mulShift(STREAM_WIDTH_FINE - STREAM_WIDTH_COARSE, t, FRAC);
		}
	}
	const int32_t cell_waves = (int32_t)(((int64_t)TWO_PI << FRAC) / gridSphere().spacing() >> FRAC);
	for (int k = 0; k <= m_levels; k++)
		m_waves[k] = std::max(8, (cell_waves << k) / 2);
	m_roll_waves = (int32_t)std::max<int64_t>(8, code.size / ROLL_WAVE);
	{
		WorldCode tuned = code;
		tuned.size = TUNED_SIZE;
		m_small = std::min<fix>(ONE, div(world.metre(), blocksPerMetre(tuned)));
	}
	// a planet's heights are its land's over √ of its size against the Earth's
	// (blocksPerMetre), its breadth over all of it: its land stands 1 / metre
	// times steeper than the Earth's, and its hillsides so too, to STEEP_MOST
	m_steep = clamp(div(ONE, std::max<fix>(1, world.metre())), ONE, STEEP_MOST);
	int64_t nearest = INT64_MAX;
	for (int k = 2; k <= m_levels; k++) {
		const int64_t apart = std::abs(blocksOf(k) - CIRQUE);
		if (apart < nearest) {
			nearest = apart;
			m_tarn_level = k;
		}
	}
	if (m_land) {
		// a trunk window gathers every run that comes within its reach of a
		// place in it: a place is within half its diagonal of its middle, and
		// a run within its furthest wander of its cell's centre
		const Sphere &sphere = gridSphere();
		int64_t wander = 0;
		for (int32_t c = 0; c < grid.size(); c++) {
			if (grid.receiver[c] < 0 || grid.base[c] < 0)
				continue;
			const Place a(sphere.centre(c));
			for (int piece = 0; piece <= TRUNK_PIECES; piece++) {
				const Place q = world.onRun(c, ratio(piece, TRUNK_PIECES));
				const int64_t dx = (int64_t)q.x - a.x, dy = (int64_t)q.y - a.y, dz = (int64_t)q.z - a.z;
				wander = std::max<int64_t>(wander, (int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)));
			}
		}
		const int64_t L = m_lattice[0];
		m_trunk_ball = mulShift(L, TRUNK_REACH, FRAC) + wander + L * 7 / 8 + L / 8;
	}
}

struct Channels::Pieces
{
	std::array<Segment, TRUNK_PIECES> pieces;
};

Channels::~Channels()
{
	if (m_pieces)
		for (int32_t c = 0; c < m_grid->size(); c++)
			delete m_pieces[c].load();
}

int32_t Channels::latticeOf(int level, int32_t c) const
{
	return floorBy(c, m_lattice[level]);
}

const Channels::Node &Channels::node(int level, int32_t i, int32_t j, int32_t l) const
{
	thread_local Ways<Node> kept;
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, 0x51ed270bu);
	if (const Node *n = kept.find(m_owner, level, i, j, l, h))
		return *n;
	// (built first: building asks for nodes itself, which may land in this set)
	Node built = build(level, i, j, l);
	return kept.put(m_owner, level, i, j, l, h, built);
}

const Channels::Node &Channels::down(int level, int32_t i, int32_t j, int32_t l) const
{
	thread_local Ways<Node> kept;
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, 0x3c6ef372u);
	if (const Node *n = kept.find(m_owner, level, i, j, l, h))
		return *n;
	Node built = toLevelAbove(level, i, j, l);
	return kept.put(m_owner, level, i, j, l, h, built);
}

static fix stands(const Grid &g, int32_t c)
{
	return STANDS_SOFT + mul(STANDS_SPAN, STRENGTH[(int)g.province[c]]);
}

fix Channels::hardness(const Place &p) const
{
	const Sphere::Spot spot = gridSphere().spot(p.dir());
	int64_t sum = 0;
	for (int k = 0; k < 3; k++)
		sum += (int64_t)STRENGTH[(int)m_grid->province[spot.cells[k]]] * spot.weights[k];
	const fix varies = clamp(fbm(p, m_seed + ROCK, m_waves[1], 2, HALF) * 2, -ONE, ONE);
	return clamp((fix)(sum >> FRAC) + mul(varies, ROCK_VARY), 0, ONE);
}

// How steep hillsides stand at a place: as steep as the ground holds
// (CRITICAL_RANGE) only where it rises fastest; where it rises slowly they're
// as steep as soil creeping down has to be to carry off what rises, which grows
// with the uplift (Tectonics::belt: a shield's 0.03, a young plain's 0.18, a
// range's to 1), as its 1.5th power: an old plain stays low and gentle
// (P3) and with the rock: hard rock stands steeper, soft wears gentler
// (`steep`: Channels::m_steep, how much steeper a small planet's hillsides stand)
static fix critical(const Grid &g, int32_t c, fix steep)
{
	const fix rising = clamp(div(g.tectonics.belt[c], ratio(80, 100)), 0, ONE);
	// (steeper as fast as it rises: a plain's low hills stay gentle, as worn)
	const fix risen = mul(rising, sqrt(rising));
	return mul(mul(HILL_LEAST + mul(CRITICAL_RANGE - HILL_LEAST, risen), stands(g, c)), ONE + mul(steep - ONE, risen));
}

// The grid's run from a cell: its slope × √its flow, stream power's steepness
// index ks (fixed × blocks)
int64_t Channels::steepness(int32_t c) const
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	const int32_t r = g.receiver[c];
	if (r < 0)
		return 0;
	const Place a(sphere.centre(c)), b(sphere.centre(r));
	const int64_t dx = (int64_t)a.x - b.x, dy = (int64_t)a.y - b.y, dz = (int64_t)a.z - b.z;
	const int64_t run = mulShift((int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)), m_radius, PLACE_FRAC);
	const int64_t drop = m_world->linkHeight(c, 0) - m_world->linkHeight(c, ONE);
	const int64_t cell_area = mulShift(m_lattice[0], m_radius, PLACE_FRAC) >> FRAC;
	const int64_t grid_flow = std::max<int64_t>(1, mulShift(g.drained[c], cell_area * cell_area, FRAC));
	if (run <= 0 || drop <= 0)
		return 0;
	return (drop << FRAC) / run * (int64_t)isqrt((uint64_t)grid_flow);
}

// A channel's slope for its flow (blocks²) in cell c, its hillsides `most`
// (a channel's always gentler than the hillsides it drains: as steep, it
// cuts no valley into them, and a range's slopes were bare ramps)
fix Channels::slopeFor(int32_t c, int64_t flow, fix most) const
{
	const int64_t steep = mulShift(steepness(c), stands(*m_grid, c), FRAC);
	return clamp((fix)std::min<int64_t>(ONE, steep / std::max<int64_t>(1, (int64_t)isqrt((uint64_t)flow))), SLOPE_LEAST, mul(most, CHANNEL_SHARE));
}

Channels::Node Channels::toLevelAbove(int level, int32_t i, int32_t j, int32_t l) const
{
	Node n;
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	const int64_t L = m_lattice[level];
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, m_seed + (uint32_t)level), (uint32_t)l, m_seed);
	const uint32_t a = mix(h, 1, m_seed), b = mix(h, 2, m_seed);
	const int64_t x = ((int64_t)i << 16 | (h & 0xFFFF)) * L >> 16;
	const int64_t y = ((int64_t)j << 16 | (a & 0xFFFF)) * L >> 16;
	const int64_t z = ((int64_t)l << 16 | (b & 0xFFFF)) * L >> 16;
	// only in a shell a cell thick round the surface, so they lie evenly
	const __int128 r2 = (__int128)x * x + (__int128)y * y + (__int128)z * z;
	const __int128 inner = PLACE_ONE - L / 2, outer = PLACE_ONE + L / 2;
	if (r2 < inner * inner || r2 >= outer * outer)
		return n;
	n.at = placeOf(x, y, z);
	// (the prototype's: drainage clumps, as rock, slope and rain vary: some of
	// a level's points are left out where noise a few lattices wide says the
	// ground drains less densely, so valleys aren't evenly spaced)
	if (m_land && level >= 1) {
		// (and by the ground: soft and wet, denser)
		const fix soft = HALF - hardness(n.at), wet = clamp(m_grid->rain(gridSphere().spot(n.at.dir()).cells[0]) - ONE, -ONE, ONE);
		const fix dense = clamp(HALF + fbm(n.at, m_seed + CLUMPS + (uint32_t)level, m_waves[level] / 3, 2, HALF) * 2
				+ mul(soft * 2, DENSE_SOFT) + mul(wet, DENSE_WET), DENSE_LEAST, ONE);
		if ((fix)(mix(h, 3, m_seed) & 0xFFFF) > dense)
			return n;
	}
	const Dir d = n.at.dir();
	const Sphere::Spot spot = sphere.spot(d);
	const auto field = [&](const std::vector<fix> &f, const Sphere::Spot &s) {
		int64_t sum = 0;
		for (int k = 0; k < 3; k++)
			sum += (int64_t)f[s.cells[k]] * s.weights[k];
		return (fix)(sum >> FRAC);
	};
	const fix here = field(g.eroded, spot);
	if (here < 0)
		return n; // under the sea
	// In folded rock streams keep to the weak beds, along the ridges, and only
	// the larger cross them (trellis drainage): a join across the belt's
	// contours counts as this much further
	// (and any range rising: its rock comes in bands along it, thrust sheets
	// hard and soft by turns, and its valleys keep to the soft between its
	// ridges, STRIKE of a fold belt's pull: the Rhône and Rhine in the Alps;
	// sprint 112)
	const fix rising = mul(clamp(field(g.tectonics.belt, spot), 0, ONE), ONE - field(g.tectonics.plateau, spot));
	const fix folded = std::max(field(g.tectonics.fold, spot), mul(rising, STRIKE)), front = field(g.tectonics.front, spot);

	// The prototype's land: water takes the way down, not the nearest line.
	// Each candidate's ground where this would join it (a rough height: the
	// network's, its flow guessed from its lattice cell) plus the climb from
	// there at the hillside's slope is the land this would stand on; the lowest
	// wins, so a deep valley's wall is cut by what drains into it (by distance,
	// a node joined the nearest channel, and a deep trunk's wall stood bare)
	const int32_t home = spot.cells[0];
	const fix hill = m_land ? critical(g, home, m_steep) : 0;
	const auto blocks = [&](int64_t place) { return mulShift(place, m_radius, PLACE_FRAC); };
	int64_t join_low = 0; // the best's rough height where it joins
	int64_t best = INT64_MAX;
	bool below_wet = false; // what the best runs into carries water
	int32_t link = -1;      // the best's grid cell, on the first level
	fix link_t = 0;         // and how far along its run it joins
	// `downstream`: the way water runs along p → q (+1) or q → p (−1)
	const auto consider = [&](const Place &p, const Place &q, const std::array<int32_t, 3> &above, bool wet, int32_t cell, int downstream, int64_t low, int64_t high) {
		Near near = nearest(n.at, p, q);
		if (!m_land && near.dist2 >= best)
			return;
		// (a tributary meets what it joins heading down with it, as rivers
		// do, never against it: joining the nearest point, it came in square
		// and, bent, a quarter against the flow; or, before, every stream
		// joined the nearest point, often a node, and a run's tributaries
		// fanned into it from all round)
		if (m_land && !joinDownstream(n.at, p, q, downstream, near))
			return;
		// (no further than read() gathers its channels round a place, WINDOW
		// lattices less its bend, unless there's nothing nearer: a longer
		// run's water ran out short of its foot, where its head was left out)
		// (by each axis apart, as the window's cells lie)
		const Place on_it = along(p, q, near.t);
		const int64_t apart = std::max({ std::abs((int64_t)on_it.x - n.at.x), std::abs((int64_t)on_it.y - n.at.y), std::abs((int64_t)on_it.z - n.at.z) });
		const bool far = m_land && apart * 4 > L * RUN_MOST;
		const Place join = along(p, q, near.t);
		const Sphere::Spot there = sphere.spot(join.dir());
		// not uphill of here (the prototype's land joins by the heights it
		// gives: a deep run sits far under its cells' mean, so by the grid's
		// field its own walls were uphill of it and nothing cut them)
		if (!m_land && field(g.eroded, there) > here)
			return;
		int64_t cost = near.dist2;
		if (folded > 0) {
			const int64_t across = mulShift(field(g.tectonics.front, there) - front, m_lattice[0], FRAC);
			cost += (across * across >> FRAC) * mul(folded, TRELLIS);
		}
		const int64_t height = low + mulShift(high - low, near.t, FRAC);
		if (m_land) {
			// (through harder rock, dearer: the rock where it joins, its
			// province's there, the cells already found)
			int64_t hard = 0;
			for (int k = 0; k < 3; k++)
				hard += (int64_t)STRENGTH[(int)g.province[there.cells[k]]] * there.weights[k];
			const fix through = ONE + mul(DEFLECT, (fix)(hard >> FRAC));
			cost = height + mulShift(mulShift(blocks((int64_t)isqrt((uint64_t)cost)), hill, FRAC), through, FRAC);
		}
		if (far)
			cost += FAR_COST;
		if (cost >= best)
			return;
		best = cost;
		join_low = height;
		n.join = join;
		n.parent = above;
		below_wet = wet;
		link = cell;
		link_t = near.t;
		n.t = near.t;
	};
	// the grid's tree: the links of the cells round here (the first level's
	// only: every level joining the trunk combed its walls with parallel
	// gullies, lineation 0.41 → 0.51 in a range)
	const auto run = [&](int32_t c) {
		const int32_t r = g.receiver[c];
		if (r < 0 || g.base[c] < 0)
			return;
		consider(Place(sphere.centre(c)), Place(sphere.centre(r)), { c, 0, 0 }, m_world->drawn(c), c, 1,
			m_land ? m_world->linkHeight(c, 0) : 0, m_land ? m_world->linkHeight(c, ONE) : 0);
	};
	if (level == 1)
		for (int32_t c : spot.cells) {
			run(c);
			for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++)
				run(*p);
		}
	if (level > 1) {
		const int64_t A = m_lattice[level - 1];
		const int32_t i0 = floorBy((int64_t)n.at.x - A * 3 / 2, A), i1 = floorBy((int64_t)n.at.x + A * 3 / 2, A);
		const int32_t j0 = floorBy((int64_t)n.at.y - A * 3 / 2, A), j1 = floorBy((int64_t)n.at.y + A * 3 / 2, A);
		const int32_t l0 = floorBy((int64_t)n.at.z - A * 3 / 2, A), l1 = floorBy((int64_t)n.at.z + A * 3 / 2, A);
		for (int32_t p = i0; p <= i1; p++)
			for (int32_t q = j0; q <= j1; q++)
				for (int32_t r = l0; r <= l1; r++) {
					const Node parent = node(level - 1, p, q, r); // (a copy: asking again can overwrite its slot)
					if (parent.exists)
						consider(parent.join, parent.at, { p, q, r }, parent.wet, -1, -1, parent.rough_low, parent.rough_high);
				}
	}
	if (best == INT64_MAX)
		return n; // nothing downhill near: no channel
	// a first level's foot on the grid's water where it's drawn, which curves
	// off the straight run between centres: the stream ends in it, not beside it
	// (nearest() measured from the run's upstream end: t along it from there)
	// (the prototype's land: always, its trunk valleys follow the runs' lines)
	if (link >= 0 && (below_wet || m_land))
		n.join = m_world->onRun(link, link_t);
	n.exists = true;
	n.link = link;
	if (m_land) {
		const int64_t lx = (int64_t)n.join.x - n.at.x, ly = (int64_t)n.join.y - n.at.y, lz = (int64_t)n.join.z - n.at.z;
		const int64_t length = blocks((int64_t)isqrt((uint64_t)(lx * lx + ly * ly + lz * lz)));
		const int64_t cell = blocks(L) >> FRAC;
		const int64_t flow = std::max<int64_t>(1, mulShift(cell * cell * ROUGH_CELLS, g.rain(home), FRAC));
		n.rough_low = join_low;
		n.rough_high = join_low + mulShift(length, slopeFor(home, flow, hill), FRAC);
	}
	n.cost = best;
	n.rain = g.rain(spot.cells[0]);
	n.wet = m_water && level <= m_wet_levels && below_wet && n.rain >= m_wet[level];
	return n;
}

/*
	A node's way down (the prototype's land), as the grid's cells drain: to
	whichever neighbour of its own level it falls to most steeply, over
	what each stands at (rise()), so a level's streams are chains of short
	steps that wander as the land does, merging where they meet, as a
	grid's do; once within JOIN_NEAR of the level above's channel, or with
	no neighbour lower, into that (toLevelAbove()). (Into that wherever it
	fell more steeply, as a grid cell would, nodes leapt to a channel two
	lattices off in a straight line.) Each step is a tree's
	edge into a lower node, so there's never a loop. Joined straight to the
	level above, node to channel, a level's streams were lines a lattice or
	three long at fixed angles, over spurs and on dykes across coarser floors.
*/
Channels::Node Channels::build(int level, int32_t i, int32_t j, int32_t l) const
{
	Node n = down(level, i, j, l);
	if (!n.exists || !m_land)
		return n;
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	const Sphere::Spot spot = sphere.spot(n.at.dir());
	const fix hill = critical(g, spot.cells[0], m_steep);
	const auto blocks = [&](int64_t place) { return mulShift(place, m_radius, PLACE_FRAC); };
	const auto apart = [](const Place &a, const Place &b) {
		const int64_t dx = (int64_t)a.x - b.x, dy = (int64_t)a.y - b.y, dz = (int64_t)a.z - b.z;
		return (int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz));
	};
	const int64_t L = m_lattice[level];
	// near the level above's channel, it runs into it
	if (apart(n.at, n.join) <= mulShift(L, JOIN_NEAR, FRAC))
		return n;
	const std::array<int32_t, 3> self{ i, j, l };
	const int64_t here = rise(level, i, j, l);
	// (ordered by what they stand at, then by place: two as high never both run into the other)
	const auto lower = [&](int64_t at, const std::array<int32_t, 3> &o) { return at < here || (at == here && o < self); };
	// the lower neighbours, and their ways: what a step mustn't cross
	struct Low
	{
		std::array<int32_t, 3> at;
		int64_t rise;
		Node node;
	};
	std::array<Low, 26> lows;
	int count = 0;
	for (int32_t a = i - 1; a <= i + 1; a++)
		for (int32_t b = j - 1; b <= j + 1; b++)
			for (int32_t c = l - 1; c <= l + 1; c++) {
				const std::array<int32_t, 3> at{ a, b, c };
				if (at == self || !down(level, a, b, c).exists)
					continue;
				const int64_t r = rise(level, a, b, c);
				if (!lower(r, at))
					continue;
				const Node o = node(level, a, b, c); // (a copy: asking again can overwrite its slot)
				if (o.exists)
					lows[count++] = { at, r, o };
			}
	// (crossing in the plane square to the place's largest axis: the
	// lattice's few cells round, as flat as a plane there)
	const int axis = std::abs(n.at.x) >= std::abs(n.at.y) && std::abs(n.at.x) >= std::abs(n.at.z) ? 0 : std::abs(n.at.y) >= std::abs(n.at.z) ? 1 : 2;
	const auto flat = [&](const Place &q) {
		return axis == 0 ? std::array<int64_t, 2>{ q.y, q.z } : axis == 1 ? std::array<int64_t, 2>{ q.x, q.z } : std::array<int64_t, 2>{ q.x, q.y };
	};
	const auto side = [](const std::array<int64_t, 2> &a, const std::array<int64_t, 2> &b, const std::array<int64_t, 2> &c) {
		const __int128 v = (__int128)(b[0] - a[0]) * (c[1] - a[1]) - (__int128)(b[1] - a[1]) * (c[0] - a[0]);
		return v > 0 ? 1 : v < 0 ? -1 : 0;
	};
	// the steepest fall to one, by what it falls over how far (no crossing another's step)
	const Low *best = nullptr;
	__int128 best_fall = 0, best_run = 1;
	for (int k = 0; k < count; k++) {
		const Low &o = lows[k];
		const int64_t run = std::max<int64_t>(1, apart(n.at, o.node.at));
		const __int128 fall = (__int128)(here - o.rise);
		if (fall <= 0 || (best && fall * best_run <= best_fall * run))
			continue;
		// (on with it, never back the way it came: water turning back past
		// square at a join, a tributary met its stream against its flow)
		{
			const __int128 sx = (int64_t)o.node.at.x - n.at.x, sy = (int64_t)o.node.at.y - n.at.y, sz = (int64_t)o.node.at.z - n.at.z;
			const __int128 ox = (int64_t)o.node.join.x - o.node.at.x, oy = (int64_t)o.node.join.y - o.node.at.y, oz = (int64_t)o.node.join.z - o.node.at.z;
			if (sx * ox + sy * oy + sz * oz <= 0)
				continue;
		}
		const std::array<int64_t, 2> a = flat(n.at), b = flat(o.node.at);
		bool crosses = false;
		for (int m = 0; m < count && !crosses; m++) {
			if (m == k)
				continue;
			const std::array<int64_t, 2> c = flat(lows[m].node.at), d = flat(lows[m].node.join);
			crosses = side(a, b, c) * side(a, b, d) < 0 && side(c, d, a) * side(c, d, b) < 0;
		}
		if (crosses)
			continue;
		best = &o;
		best_fall = fall;
		best_run = run;
	}
	if (!best)
		return n; // a hollow: none round it lower, so into the level above
	// into its head, as a grid cell into its receiver
	const Node &o = best->node;
	n.sibling = true;
	n.into = best->at;
	n.join = o.at;
	n.t = ONE;
	n.cost = here;
	// (its bed its own climb from the level above's, as down() has it, never
	// under what it runs into: by its chain's length, a node stood tens of
	// blocks over the valley beside it that it didn't drain into, on a dyke)
	n.rough_low = o.rough_high;
	n.rough_high = std::max(n.rough_low, down(level, i, j, l).rough_high);
	n.wet = m_water && level <= m_wet_levels && o.wet && n.rain >= m_wet[level];
	return n;
}

// What a node stands at, as its level's drain over it (build()): its climb
// from the level above (down()'s cost: the height where it'd join plus the
// climb there) and the lie of the land between, ROUTE_ROUGH of a lattice's
// climb either way, by noise a couple of lattices wide: what bends a chain
// round, as hard and soft ground and the hollows erosion left do
int64_t Channels::rise(int level, int32_t i, int32_t j, int32_t l) const
{
	const Node &n = down(level, i, j, l);
	const fix hill = critical(*m_grid, gridSphere().spot(n.at.dir()).cells[0], m_steep);
	const int64_t climb = mulShift(mulShift(m_lattice[level], m_radius, PLACE_FRAC), hill, FRAC);
	const fix lie = fbm(n.at, m_seed + ROUTE + (uint32_t)level, std::max(4, m_waves[level] / ROUTE_WAVES), 2, HALF);
	return n.cost + mulShift(mulShift(climb, ROUTE_ROUGH, FRAC), lie * 2, FRAC);
}

const Channels::Shape &Channels::shape(int level, int32_t i, int32_t j, int32_t l) const
{
	thread_local Ways<Shape> kept;
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, 0x2545f491u);
	if (const Shape *k = kept.find(m_owner, level, i, j, l, h))
		return *k;
	const Shape worn = wear(level, i, j, l); // (asks for its parent's: may land in this set)
	return kept.put(m_owner, level, i, j, l, h, worn);
}

// A node's path, way or wandering, kept for every thread in `store` and in
// this thread's own cache in front (one per caller: each `make` its own type)
template <typename Make>
const Channels::Path &Channels::keptIn(Traced &store, uint32_t salt, int level, int32_t i, int32_t j, int32_t l, Make make) const
{
	thread_local Ways<Path> kept;
	const uint64_t owner = m_owner;
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, salt);
	if (const Path *k = kept.find(owner, level, i, j, l, h))
		return *k;
	const uint64_t key = Traced::key(level, i, j, l);
	Traced::Shard &shard = store.shards[mix((uint32_t)key, (uint32_t)(key >> 32), 0x7f4a7c15u) % TRACED_SHARDS];
	{
		std::lock_guard<std::mutex> hold(shard.lock);
		const auto found = shard.paths.find(key);
		if (found != shard.paths.end())
			return kept.put(owner, level, i, j, l, h, found->second);
	}
	// (made unlocked: it asks for others'. Another thread may make it too:
	// the same, so either's kept)
	const Path made = make(); // (asks for what it joins's: may land in this set)
	{
		std::lock_guard<std::mutex> hold(shard.lock);
		if (shard.paths.size() >= TRACED_MOST)
			shard.paths.clear();
		shard.paths.emplace(key, made);
	}
	return kept.put(owner, level, i, j, l, h, made);
}

const Channels::Path &Channels::path(int level, int32_t i, int32_t j, int32_t l) const
{
	return keptIn(*m_traced, 0x9b05688cu, level, i, j, l, [&] { return trace(level, i, j, l); });
}

const Channels::Path &Channels::way(int level, int32_t i, int32_t j, int32_t l) const
{
	return keptIn(*m_ways, 0x510e527fu, level, i, j, l, [&] { return wayOf(level, i, j, l); });
}

const Channels::Path &Channels::wander(int level, int32_t i, int32_t j, int32_t l) const
{
	return keptIn(*m_wanders, 0x1f83d9abu, level, i, j, l, [&] { return wanderOf(level, i, j, l); });
}

Place Channels::pathAt(const Path &path, fix t)
{
	const int64_t on = (int64_t)std::clamp<fix>(t, 0, ONE) * PATH_PIECES;
	const int q = (int)std::min<int64_t>(PATH_PIECES - 1, on >> FRAC);
	return along(path.at[q], path.at[q + 1], (fix)(on - ((int64_t)q << FRAC)));
}

static int64_t bedAt(int64_t low, int64_t high, fix flat, fix t);

/*
	A channel's way (the prototype's): from its head to its foot on what it
	joins's own way, wandering over the lie of the land between. Node to node
	in straight steps, a level's streams were lines hundreds of blocks long:
	most join the level above in one.
*/
Channels::Path Channels::wanderOf(int level, int32_t i, int32_t j, int32_t l) const
{
	const Node n = node(level, i, j, l); // (a copy: what it joins's way can overwrite its slot)
	Path out;
	out.ground.fill(INT64_MAX);
	if (!n.exists)
		throw std::logic_error("terragen::Channels: a path asked of no channel");
	// its foot on what it joins's own way (on the first level, the grid's run: n.join's on it)
	Place foot = n.join;
	if (m_land && (level > 1 || n.sibling)) {
		const Path &into = n.sibling ? way(level, n.into[0], n.into[1], n.into[2]) : way(level - 1, n.parent[0], n.parent[1], n.parent[2]);
		foot = pathAt(into, n.t);
	}
	out.at[0] = foot;
	out.at[PATH_PIECES] = n.at;
	// (straight, off the prototype's land)
	if (!m_land) {
		for (int s = 0; s <= PATH_PIECES; s++)
			out.at[s] = along(foot, n.at, ratio(s, PATH_PIECES));
		return out;
	}
	// from its head, each piece to the lowest of the lie of the land a step on
	// (WANDER: the rock's noise, a few times finer than its lattice), within
	// 50° of the way to its foot, a step as long as its share of what's left:
	// so it wanders as water finds its way, and still ends on its foot
	struct Turn
	{
		fix cos, sin;
	};
	static constexpr Turn TURNS[] = {
		{ ONE, 0 },
		{ 59395, 27697 }, { 59395, -27697 }, // 25°
		{ 42125, 50203 }, { 42125, -50203 }, // 50°
	};
	const int32_t waves = m_waves[level] * WANDER_WAVES;
	Place cur = n.at;
	for (int s = PATH_PIECES - 1; s >= 1; s--) {
		const int64_t dx = (int64_t)foot.x - cur.x, dy = (int64_t)foot.y - cur.y, dz = (int64_t)foot.z - cur.z;
		// square to the way, along the ground, as long (cur's a radius long)
		const int64_t px = ((int64_t)cur.y * dz - (int64_t)cur.z * dy) >> PLACE_FRAC;
		const int64_t py = ((int64_t)cur.z * dx - (int64_t)cur.x * dz) >> PLACE_FRAC;
		const int64_t pz = ((int64_t)cur.x * dy - (int64_t)cur.y * dx) >> PLACE_FRAC;
		const int64_t left = s + 1; // pieces from here to the foot
		Place best = cur;
		fix lowest = 0;
		bool any = false;
		for (const Turn &turn : TURNS) {
			const Place q = placeOf((int64_t)cur.x + (dx * turn.cos + px * turn.sin) / left / ONE,
					(int64_t)cur.y + (dy * turn.cos + py * turn.sin) / left / ONE,
					(int64_t)cur.z + (dz * turn.cos + pz * turn.sin) / left / ONE);
			// (a turn costs a little: it bends where the land says, not by a hair)
			const fix lie = fbm(q, m_seed + WANDER + (uint32_t)level, waves, 2, HALF) + mul(std::abs(turn.sin), WANDER_TURN);
			if (!any || lie < lowest) {
				any = true;
				lowest = lie;
				best = q;
			}
		}
		out.at[s] = best;
		cur = best;
	}
	// its last piece comes in along what it joins, from upstream of its foot
	// (half way to there): as the noise left it, it met its stream at any
	// angle, a dozen in a range against its flow
	{
		Place up = foot;
		if (level > 1 || n.sibling) {
			const Path &into = n.sibling ? way(level, n.into[0], n.into[1], n.into[2]) : way(level - 1, n.parent[0], n.parent[1], n.parent[2]);
			up = pathAt(into, std::min<fix>(ONE, n.t + ONE / PATH_PIECES)); // (t runs from its foot up it)
		} else if (n.link >= 0)
			up = m_world->onRun(n.link, std::max<fix>(0, n.t - ONE / TRUNK_PIECES)); // (a run's t runs down it)
		const int64_t ux = (int64_t)up.x - foot.x, uy = (int64_t)up.y - foot.y, uz = (int64_t)up.z - foot.z;
		const int64_t ul = (int64_t)isqrt((uint64_t)(ux * ux + uy * uy + uz * uz));
		const int64_t px = (int64_t)out.at[1].x - foot.x, py = (int64_t)out.at[1].y - foot.y, pz = (int64_t)out.at[1].z - foot.z;
		const int64_t pl = (int64_t)isqrt((uint64_t)(px * px + py * py + pz * pz));
		if (ul > 0 && pl > 0) {
			const Place along_it = placeOf(foot.x + ux * pl / ul, foot.y + uy * pl / ul, foot.z + uz * pl / ul);
			out.at[1] = placeOf(((int64_t)out.at[1].x + along_it.x) / 2, ((int64_t)out.at[1].y + along_it.y) / 2, ((int64_t)out.at[1].z + along_it.z) / 2);
		}
	}
	// eased once: no corner of a step's turn left in it
	{
		Path eased = out;
		for (int s = 1; s < PATH_PIECES; s++) {
			const Place &a = out.at[s - 1], &b = out.at[s], &c = out.at[s + 1];
			eased.at[s] = placeOf(((int64_t)a.x + 2 * (int64_t)b.x + c.x) / 4, ((int64_t)a.y + 2 * (int64_t)b.y + c.y) / 4,
					((int64_t)a.z + 2 * (int64_t)b.z + c.z) / 4);
		}
		out.at = eased.at;
	}
	return out;
}

// A channel's way as it lies (trace()'s, without the land under it)
Channels::Path Channels::wayOf(int level, int32_t i, int32_t j, int32_t l) const
{
	Path out = wander(level, i, j, l);
	if (!m_land)
		return out;
	// never across a lower one of its level's ways round it (build() kept
	// their straight lines apart, not their wanderings): straight, if so.
	// (Against their wanderings, before their own straightening: by their
	// finished ways, each way asked for every lower one's round it, and those
	// theirs, down a whole basin: half a minute a column)
	{
		const Node &n = node(level, i, j, l);
		const Place foot = out.at[0];
		const int64_t here = rise(level, i, j, l);
		const int axis = std::abs(n.at.x) >= std::abs(n.at.y) && std::abs(n.at.x) >= std::abs(n.at.z) ? 0 : std::abs(n.at.y) >= std::abs(n.at.z) ? 1 : 2;
		const auto flat = [&](const Place &q) {
			return axis == 0 ? std::array<int64_t, 2>{ q.y, q.z } : axis == 1 ? std::array<int64_t, 2>{ q.x, q.z } : std::array<int64_t, 2>{ q.x, q.y };
		};
		const auto side = [](const std::array<int64_t, 2> &a, const std::array<int64_t, 2> &b, const std::array<int64_t, 2> &c) {
			const __int128 v = (__int128)(b[0] - a[0]) * (c[1] - a[1]) - (__int128)(b[1] - a[1]) * (c[0] - a[0]);
			return v > 0 ? 1 : v < 0 ? -1 : 0;
		};
		const auto cross = [&](const Place &a0, const Place &a1, const Place &b0, const Place &b1) {
			const auto a = flat(a0), b = flat(a1), c = flat(b0), d = flat(b1);
			return side(a, b, c) * side(a, b, d) < 0 && side(c, d, a) * side(c, d, b) < 0;
		};
		const std::array<int32_t, 3> self{ i, j, l };
		const auto crossesAny = [&]() {
		bool crosses = false;
		for (int32_t a = i - 2; a <= i + 2 && !crosses; a++)
			for (int32_t b = j - 2; b <= j + 2 && !crosses; b++)
				for (int32_t c = l - 2; c <= l + 2 && !crosses; c++) {
					const std::array<int32_t, 3> at{ a, b, c };
					if (at == self || !down(level, a, b, c).exists)
						continue;
					const int64_t r = rise(level, a, b, c);
					if (!(r < here || (r == here && at < self)) || !node(level, a, b, c).exists)
						continue;
					const Path &other = wander(level, a, b, c);
					for (int p = 0; p < PATH_PIECES && !crosses; p++)
						for (int q = 0; q < PATH_PIECES && !crosses; q++)
							crosses = cross(out.at[p], out.at[p + 1], other.at[q], other.at[q + 1]);
				}
		return crosses;
		};
		// (pulled half way to straight first: straight, every crossing way
		// was a line hundreds of blocks long among the wandering)
		for (int tries = 0; tries < 2 && crossesAny(); tries++)
			for (int s = 1; s < PATH_PIECES; s++) {
				const Place straight = along(foot, n.at, ratio(s, PATH_PIECES));
				out.at[s] = tries == 0 ? placeOf(((int64_t)out.at[s].x + straight.x) / 2, ((int64_t)out.at[s].y + straight.y) / 2, ((int64_t)out.at[s].z + straight.z) / 2)
						: straight;
			}
	}
	return out;
}

/*
	A channel's path: its way, and the land it runs over (Path::ground).
*/
Channels::Path Channels::trace(int level, int32_t i, int32_t j, int32_t l) const
{
	const Node n = node(level, i, j, l); // (a copy: what it joins's path can overwrite its slot)
	Path out = way(level, i, j, l);
	if (!m_land)
		return out;
	// the land it runs over, made by what it can drain into (its level's lower
	// nodes and the coarser levels', so at its foot what it joins), never
	// rising toward its foot: its water's held to it, so it's never on a dyke
	// over a lower valley it lies in (a stream in a wide trough beside it
	// stood tens of blocks over the trough's floor)
	// (on a chain, what its receiver's runs over and its receiver: so where
	// they meet their water's held alike. By all lower than itself, more than
	// its receiver's saw, its water stood under its receiver's there, and ran
	// up into it)
	// (and its own valley: its water's never over its own floor either. The
	// grid's heights it's brought to the grid by are what it joins's, rising
	// to its head, and stood it tens of blocks over its own valley there)
	const Below below = n.sibling ? Below{ level, rise(level, n.into[0], n.into[1], n.into[2]), n.into, true, { i, j, l }, &out }
			: Below{ level, rise(level, i, j, l), { i, j, l }, false, { i, j, l }, &out };
	for (int s = 0; s <= PATH_PIECES; s++)
		out.ground[s] = readWith(out.at[s], rangeAt(out.at[s]), m_lattice[level], &below).land;
	for (int s = PATH_PIECES - 1; s >= 0; s--)
		out.ground[s] = std::min(out.ground[s], out.ground[s + 1]);
	// and never under what it joins's water where it joins: what that runs
	// over isn't quite what this does (each holds to what's lower than what
	// it runs into), and its water ran up into the other's
	if (level > 1 || n.sibling) {
		const int tl = n.sibling ? level : level - 1;
		const std::array<int32_t, 3> to = n.sibling ? n.into : n.parent;
		const Path into = path(tl, to[0], to[1], to[2]);
		const Shape sh = shape(tl, to[0], to[1], to[2]);
		int64_t there = keepWith(bedAt(sh.low, sh.high, sh.flat, n.t), sh.grid_low + mulShift(sh.grid_high - sh.grid_low, n.t, FRAC),
				sh.over_low + mulShift(sh.over_high - sh.over_low, n.t, FRAC));
		const int64_t on = (int64_t)std::clamp<fix>(n.t, 0, ONE) * PATH_PIECES;
		const int q = (int)std::min<int64_t>(PATH_PIECES - 1, on >> FRAC);
		if (into.ground[q] != INT64_MAX && into.ground[q + 1] != INT64_MAX)
			there = std::min(there, into.ground[q] + mulShift(into.ground[q + 1] - into.ground[q], (fix)(on - ((int64_t)q << FRAC)), FRAC));
		for (int64_t &g : out.ground)
			g = std::max(g, there);
	}
	return out;
}

int64_t Channels::drains(int level, int32_t i, int32_t j, int32_t l) const
{
	thread_local Ways<int64_t> kept;
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, 0x6a09e667u);
	if (const int64_t *area = kept.find(m_owner, level, i, j, l, h))
		return *area;
	// its own lattice cell's hillsides, and all its children's (the next
	// level's nodes that join it: they lie within a lattice or two of it)
	const Node n = node(level, i, j, l);
	const int64_t own = mulShift(m_lattice[level < m_area_levels ? level + 1 : level], m_radius, PLACE_FRAC) >> FRAC;
	int64_t area = own * own;
	if (n.exists && level < m_area_levels) {
		// its children found, each counted as what a child of that level
		// drains on average: its own cell's and each finer level's again (a
		// level's cells cover the ground once). Counted down every level, the
		// first column in a place built the whole network under a coarse
		// node: a far tile, which reads only the coarse levels, 1 s a column
		const int64_t child_own = mulShift(m_lattice[std::min(level + 2, m_area_levels)], m_radius, PLACE_FRAC) >> FRAC;
		const int64_t below = child_own * child_own * (m_area_levels - level);
		const int64_t B = m_lattice[level + 1], reach = m_lattice[level] * 2;
		const int64_t lo[3] = { std::min<int64_t>(n.join.x, n.at.x) - reach, std::min<int64_t>(n.join.y, n.at.y) - reach, std::min<int64_t>(n.join.z, n.at.z) - reach };
		const int64_t hi[3] = { std::max<int64_t>(n.join.x, n.at.x) + reach, std::max<int64_t>(n.join.y, n.at.y) + reach, std::max<int64_t>(n.join.z, n.at.z) + reach };
		for (int32_t a = floorBy(lo[0], B); a <= floorBy(hi[0], B); a++)
			for (int32_t b = floorBy(lo[1], B); b <= floorBy(hi[1], B); b++)
				for (int32_t c = floorBy(lo[2], B); c <= floorBy(hi[2], B); c++) {
					// (only the shell round the surface holds nodes)
					const __int128 mx = ((__int128)a * 2 + 1) * B / 2, my = ((__int128)b * 2 + 1) * B / 2, mz = ((__int128)c * 2 + 1) * B / 2;
					const __int128 r2 = mx * mx + my * my + mz * mz, in = PLACE_ONE - B * 2, out = PLACE_ONE + B * 2;
					if (r2 < in * in || r2 > out * out)
						continue;
					const Node child = node(level + 1, a, b, c);
					if (child.exists && child.parent == std::array<int32_t, 3>{ i, j, l })
						area += below;
				}
	}
	// and all that runs into its head from its own level's nodes round it
	if (n.exists && m_land)
		for (int32_t a = i - 1; a <= i + 1; a++)
			for (int32_t b = j - 1; b <= j + 1; b++)
				for (int32_t c = l - 1; c <= l + 1; c++) {
					const Node o = node(level, a, b, c);
					if (o.exists && o.sibling && o.into == std::array<int32_t, 3>{ i, j, l })
						area += drains(level, a, b, c);
				}
	return kept.put(m_owner, level, i, j, l, h, area);
}

// A channel's flat floor, half its width (blocks, fixed): FLOOR_WIDTHS of its
// stream's width (as the stream's drawn: 1 + √(flow / STREAM_FLOW) / 2), no
// more than FLOOR_MOST, where it's gentle, fading out as it steepens to FLOOR_GRADE
static int64_t floorOf(int64_t flow, fix grade)
{
	if (grade >= FLOOR_GRADE || flow < STREAM_FLOW)
		return 0;
	const fix gentle = ONE - div(grade, FLOOR_GRADE);
	const int64_t stream = toFix(1) + (int64_t)isqrt((uint64_t)(flow / STREAM_FLOW)) * ONE / 2;
	return mulShift(std::min<int64_t>(toFix(FLOOR_MOST), stream * FLOOR_WIDTHS / 2), gentle, FRAC);
}

// The flat floor of a valley, half its width (blocks, fixed), as read()
// shapes it (floorOf, and climate()'s trough and wash): what drains to it
// (blocks², and with its rain), its grade, its hillsides' slope, its ice
static int64_t valleyFloor(int64_t area, fix rain, fix grade, fix hill, fix ice)
{
	int64_t half = floorOf(mulShift(area, rain, FRAC), grade);
	const int64_t across = toFix((int64_t)isqrt((uint64_t)std::max<int64_t>(0, area)));
	ice = mul(ice, smooth(clamp(div(hill - TROUGH_HILL, TROUGH_HILL), 0, ONE)));
	if (ice > 0)
		half = std::max(half, mulShift(std::min<int64_t>(toFix(TROUGH_MOST), mulShift(across, TROUGH_SHARE, FRAC)), ice, FRAC));
	const fix dry = clamp(div(WASH_RAIN - rain, WASH_RAIN) * 2, 0, ONE);
	if (dry > 0 && ice == 0)
		half = std::max(half, mulShift(std::min<int64_t>(toFix(WASH_MOST), mulShift(across, WASH_SHARE, FRAC)), dry, FRAC));
	return half;
}

// A channel's bed `t` along it from its foot: level across what it joins's
// floor (`flat` of the way), then rising
static int64_t bedAt(int64_t low, int64_t high, fix flat, fix t)
{
	if (t <= flat)
		return low;
	return low + mulShift(high - low, (fix)(((int64_t)(t - flat) << FRAC) / std::max<int64_t>(1, ONE - flat)), FRAC);
}

int64_t Channels::cellArea() const
{
	const int64_t cell = mulShift(m_lattice[0], m_radius, PLACE_FRAC) >> FRAC;
	return std::max<int64_t>(1, cell * cell);
}

int64_t Channels::streamWidth(int64_t flow) const
{
	return m_world->riverWidth(flow);
}

Channels::Shape Channels::wear(int level, int32_t i, int32_t j, int32_t l) const
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	const Node n = node(level, i, j, l);
	Shape out;
	if (!n.exists)
		return out;
	const auto blocks = [&](int64_t place) { return mulShift(place, m_radius, PLACE_FRAC); }; // fixed
	const auto apart = [](const Place &a, const Place &b) {
		const int64_t dx = (int64_t)a.x - b.x, dy = (int64_t)a.y - b.y, dz = (int64_t)a.z - b.z;
		return (int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz));
	};
	const int64_t length = blocks(apart(n.join, n.at));
	out.area = drains(level, i, j, l);
	const int64_t flow = std::max<int64_t>(1, mulShift(out.area, n.rain, FRAC));
	// the grid's run here: its slope and flow give the steepness index
	const int32_t c = sphere.cell(n.at.dir());
	const fix most = critical(g, c, m_steep);
	out.slope = slopeFor(c, flow, most);
	out.hill = most;
	// its foot's height: on the grid's run, or the level above's channel where it joins
	fix floor_grade = ONE; // what it joins: how steep there
	int64_t joins_floor = 0; // and its floor's half width (blocks, fixed)
	Place joins_a, joins_b;  // and its line
	if (n.sibling) {
		const Shape &into = shape(level, n.into[0], n.into[1], n.into[2]);
		out.low = bedAt(into.low, into.high, into.flat, n.t);
		floor_grade = into.slope;
		const Node o = node(level, n.into[0], n.into[1], n.into[2]);
		joins_floor = valleyFloor(into.area, o.rain, into.slope, into.hill, into.ice);
		joins_a = o.join;
		joins_b = o.at;
	} else if (level == 1) {
		out.low = m_world->linkHeight(n.link, n.t);
		const int32_t to = g.receiver[n.link];
		if (to >= 0) {
			const int64_t run = blocks(apart(Place(sphere.centre(n.link)), Place(sphere.centre(to))));
			const int64_t drop = m_world->linkHeight(n.link, 0) - m_world->linkHeight(n.link, ONE);
			if (run > 0)
				floor_grade = (fix)std::clamp<int64_t>((drop << FRAC) / run, 0, ONE);
			// (a grid run's trunk valley: its floor as read() traces its pieces)
			if (!m_world->drawn(n.link) && g.drained[n.link] >= TRUNK_LEAST) {
				const int64_t cell = mulShift(m_lattice[0], m_radius, PLACE_FRAC) >> FRAC;
				const int64_t area = mulShift(g.drained[n.link], cell * cell, FRAC);
				joins_floor = valleyFloor(area, g.rain(n.link), floor_grade, critical(g, n.link, m_steep), m_world->glacial(n.link, out.low));
			}
			joins_a = Place(sphere.centre(n.link));
			joins_b = Place(sphere.centre(to));
		}
	} else {
		const Shape &above = shape(level - 1, n.parent[0], n.parent[1], n.parent[2]);
		out.low = bedAt(above.low, above.high, above.flat, n.t);
		floor_grade = above.slope;
		const Node o = node(level - 1, n.parent[0], n.parent[1], n.parent[2]);
		joins_floor = valleyFloor(above.area, o.rain, above.slope, above.hill, above.ice);
		joins_a = o.join;
		joins_b = o.at;
	}
	// it runs level across that floor till it's out of it (its head this far
	// off its line): its bed rising from its foot stood over the floor,
	// tens of blocks in a trough, and its water on a dyke across it
	if (m_land && joins_floor > 0 && length > 0) {
		const int64_t off = blocks((int64_t)isqrt((uint64_t)nearest(n.at, joins_a, joins_b).dist2));
		const int64_t across = off > 0 ? std::min<int64_t>(length, (int64_t)((__int128)joins_floor * length / off)) : length;
		out.flat = (fix)std::min<int64_t>(ONE * 3 / 4, ((int64_t)across << FRAC) / length);
	}
	if (m_land && floor_grade < FAN_FLOOR && out.slope >= floor_grade * FAN_STEEPER) {
		const fix dry = ratio(160, 100) - clamp(div(n.rain - FAN_DRY, FAN_WET - FAN_DRY), 0, ONE);
		const int64_t radius = mulShift(mulShift(toFix((int64_t)isqrt((uint64_t)out.area)), FAN_SHARE, FRAC), dry, FRAC);
		out.fan_r = std::min(std::clamp<int64_t>(radius, toFix(FAN_LEAST), toFix(FAN_MOST)), length);
	}
	out.high = out.low + mulShift(length - mulShift(length, out.flat, FRAC), out.slope, FRAC);
	// on a chain (build()), its own climb from the level above's channel,
	// as if it ran straight there, never under what it runs into: so a
	// level's beds stand as the ground they drain does, each node's its own,
	// not summed down its chain
	if (m_land && n.sibling) {
		const Node d = down(level, i, j, l);
		int64_t base;
		if (level == 1)
			base = m_world->linkHeight(d.link, d.t);
		else {
			const Shape &above = shape(level - 1, d.parent[0], d.parent[1], d.parent[2]);
			base = bedAt(above.low, above.high, above.flat, d.t);
		}
		out.flat = 0;
		out.high = std::max(out.low, base + mulShift(blocks(apart(d.join, d.at)), out.slope, FRAC));
	}
	out.ice = m_land ? m_world->glacial(c, out.high) : 0;
	if (m_land) {
		// what its water's brought to the grid by: what it joins's, where it
		// joins (on the first level, the grid's there), and at its head no less
		if (n.sibling || level > 1) {
			const Shape &to = n.sibling ? shape(level, n.into[0], n.into[1], n.into[2]) : shape(level - 1, n.parent[0], n.parent[1], n.parent[2]);
			out.grid_low = to.grid_low + mulShift(to.grid_high - to.grid_low, n.t, FRAC);
			out.over_low = to.over_low + mulShift(to.over_high - to.over_low, n.t, FRAC);
		} else {
			out.grid_low = m_world->gridHeight(n.join);
			out.over_low = overGrid(rangeAt(n.join));
		}
		out.grid_high = std::max(out.grid_low, m_world->gridHeight(n.at));
		out.over_high = std::max(out.over_low, overGrid(rangeAt(n.at)));
	}
	// a cirque at its head where ice worked it, on the level whose channels
	// are a cirque's size, in a range's steep ground
	if (m_land && level == m_tarn_level && out.ice > 0 && out.hill >= TROUGH_HILL)
		// (no wider than the channel climbs: a bowl's scooped under a headwall,
		// and on gentle ground there's none to scoop under)
		if (out.high - out.low >= toFix(CIRQUE_LEAST))
			out.cirque_r = mulShift(std::clamp<int64_t>(std::min(mulShift(length, CIRQUE_SHARE, FRAC), out.high - out.low), toFix(CIRQUE_LEAST), toFix(CIRQUE_MOST)),
					out.ice, FRAC);
	// a pond on it: a tarn near a worked head, or a pool on a gentle floor
	if (m_land && m_water && level > 1) {
		const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, m_seed + PONDS);
		const fix roll = (fix)(h & 0xFFFF), where = (fix)(h >> 16 & 0xFFFF);
		const fix ice = out.ice;
		const int64_t L = blocksOf(level);
		const fix km2 = (fix)std::min<int64_t>(ONE, (L * L << FRAC) / 1000000); // a lattice cell's
		const int64_t floor = floorOf(flow, out.slope);
		int64_t radius = 0;
		if (level == m_tarn_level && ice > 0 && roll < mul(mul(ice, TARNS), km2)) {
			radius = std::clamp<int64_t>(length / 5, toFix(TARN_LEAST), toFix(TARN_MOST));
			out.pond_t = ratio(80, 100) + mul(where, ratio(10, 100));
			out.pond_depth = radius / 3;
		} else if (floor > 0 && roll < mul(POOLS, km2)) {
			radius = std::clamp<int64_t>(floor * 2, toFix(POOL_LEAST), toFix(POOL_MOST));
			out.pond_t = ratio(25, 100) + mul(where, ratio(50, 100));
			out.pond_depth = std::max<int64_t>(toFix(1), radius / 6);
		}
		// (no bigger than a quarter of its channel: a pond's in its valley)
		if (radius > 0 && radius * 4 <= length) {
			out.pond_r = radius;
			// its level the channel's at its downstream rim
			const int64_t centre = bedAt(out.low, out.high, out.flat, out.pond_t);
			out.pond_level = std::max(out.low, centre - mulShift(radius, out.slope, FRAC));
		}
	}
	return out;
}

Channels::Read Channels::read(const Place &p, fix range, int64_t least) const
{
	return readWith(p, range, least, nullptr);
}

Channels::Read Channels::readWith(const Place &p, fix range, int64_t least, const Below *below) const
{
	Read out;
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	// What's near a place's lattice cell, kept per thread: columns come in runs
	// through one cell, and only a few of a window's cells hold a node
	struct List
	{
		uint64_t owner = 0;
		bool hollows = false; // (the trunk's) its runs that drain little, as valleys
		int32_t i = 0, j = 0, l = 0;
		int count = 0;
		std::vector<Segment> segments;
	};
	// (a set a depth: tracing a channel's path reads the land, inside a read,
	// and what this one's found in its lists is pointed at)
	thread_local int depth = 0;
	// (a level's own nodes' ways read the land of those lower: as deep as a chain is long)
	thread_local std::vector<std::unique_ptr<std::array<List, MOST_LEVELS + 1>>> stacked;
	if (depth >= (int)stacked.size())
		stacked.resize(depth + 1);
	if (!stacked[depth])
		stacked[depth].reset(new std::array<List, MOST_LEVELS + 1>());
	// (the set itself, not its slot: a nested read may grow `stacked`)
	std::array<List, MOST_LEVELS + 1> *const lists = stacked[depth].get();
	struct Deeper
	{
		int &d;
		explicit Deeper(int &d) : d(d) { d++; }
		~Deeper() { d--; }
	} deeper(depth);

	// a valley as the climate shapes it: a trough where ice worked it, a wash
	// where it's dry (P6); `area` what it drains (blocks²)
	const auto climate = [&](Segment &s, int64_t area, fix ice, fix rain) {
		const int64_t across = toFix((int64_t)isqrt((uint64_t)std::max<int64_t>(0, area)));
		// (valley glaciers cut troughs, in mountains: on a lowland the ice
		// was a sheet, and every channel's floor widened into one flat)
		ice = mul(ice, smooth(clamp(div(s.hill - TROUGH_HILL, TROUGH_HILL), 0, ONE)));
		if (ice > 0) {
			const int64_t half = mulShift(std::min<int64_t>(toFix(TROUGH_MOST), mulShift(across, TROUGH_SHARE, FRAC)), ice, FRAC);
			s.floor = std::max(s.floor, half);
			// its walls as high as it's half wide, then the hillside as it was
			if (TROUGH_WALL > s.hill) {
				s.wall = s.hill + mul(TROUGH_WALL - s.hill, ice);
				s.wall_run = (half << FRAC) / s.wall;
			}
		}
		const fix dry = clamp(div(WASH_RAIN - rain, WASH_RAIN) * 2, 0, ONE);
		if (dry > 0 && ice == 0) {
			s.floor = std::max(s.floor, mulShift(std::min<int64_t>(toFix(WASH_MOST), mulShift(across, WASH_SHARE, FRAC)), dry, FRAC));
			s.wash = dry >= HALF;
		}
	};
	int64_t closest2 = INT64_MAX, stream2 = INT64_MAX;
	int64_t sum = 0, norm = 0;
	fix weight = ONE, above = ONE;
	int64_t bent[3] = { 0, 0, 0 }; // the coarser levels' bends, summed (place units)
	int64_t to_above2 = INT64_MAX; // how far (²) the place, bent so far, is from the last level's channels
	// the land: the lowest channel-and-hillside, the next, and how much the
	// divide between them is rounded (blocks, fixed)
	int64_t lowest = INT64_MAX, second = INT64_MAX, round = 0;
	int64_t deepest = INT64_MAX; // the least of each channel's height here and over its bed (Read::clear)
	// (a field of the place, not of a channel: the land stays seamless)
	fix vary = m_land ? ONE + mul(clamp(fbm(p, m_seed + SLOPES, m_waves[0] * 2, 2, HALF) * 2, -ONE, ONE), VARY) : ONE;
	// (and by the rock here against its province's, which the channels'
	// slopes already stand at)
	if (m_land) {
		const Sphere::Spot spot = gridSphere().spot(p.dir());
		int64_t province = 0;
		for (int k = 0; k < 3; k++)
			province += (int64_t)STRENGTH[(int)m_grid->province[spot.cells[k]]] * spot.weights[k];
		const fix here = STANDS_SOFT + mul(STANDS_SPAN, hardness(p)), usual = STANDS_SOFT + mul(STANDS_SPAN, (fix)(province >> FRAC));
		vary = mul(vary, div(here, usual));
	}
	const int64_t finest = mulShift(m_lattice[m_levels], m_radius, PLACE_FRAC);
	// the pond nearest, by its radius: how far off (blocks, fixed) and which
	int64_t pond_d = 0;
	// the highest fan's cone over here, and whose (P4)
	int64_t fan_top = INT64_MIN;
	const Segment *fan = nullptr, *owner = nullptr;
	const Segment *pond = nullptr;
	int64_t pond_h = INT64_MAX; // its channel's ground here
	// the water: the nearest wet channel's (the larger flow, as near),
	// its level there, and its width (blocks, fixed); and the land round
	// every wet channel near: raised to its banks, cut down to its water
	int64_t wet_gap = INT64_MAX, wet_edge = INT64_MAX, wet_level = 0, wet_flow = 0;
	int wet_from = 0;
	int64_t banks = INT64_MIN, cut = INT64_MAX;
	int64_t grid_here = INT64_MIN; // the grid's height here, once asked (none yet)
	fix pond_by = toFix(2);
	for (int k = 0; k <= m_levels && (k == 0 || m_lattice[k] >= least); k++) {
		const int64_t L = m_lattice[k];
		// (the land's: round where the coarser levels' bends put the place, as
		// its channels are measured from there)
		const int64_t wx = m_land ? (int64_t)p.x + bent[0] : p.x, wy = m_land ? (int64_t)p.y + bent[1] : p.y, wz = m_land ? (int64_t)p.z + bent[2] : p.z;
		const int32_t i = floorBy(wx, L), j = floorBy(wy, L), l = floorBy(wz, L);
		List &list = (*lists)[k];
		// (read no finer than the trunk, a far tile's land, nothing finer
		// shapes what drains little: its runs are valleys there too)
		const bool hollows = k == 0 && (m_levels == 0 || m_lattice[1] < least);
		// (a list of only the lower of a level's nodes is this read's alone: never kept)
		const bool only_lower = below && below->level == k;
		if (only_lower || list.owner != m_owner || list.hollows != hollows || list.i != i || list.j != j || list.l != l) {
			list.owner = only_lower ? 0 : m_owner;
			list.hollows = hollows;
			list.i = i;
			list.j = j;
			list.l = l;
			list.count = 0;
			const auto add = [&](const Segment &s) {
				if (list.count == (int)list.segments.size())
					list.segments.emplace_back();
				list.segments[list.count++] = s;
			};
			if (k == 0) {
				// the trunk: the grid's links round here
				const auto link = [&](int32_t c) {
					const int32_t r = g.receiver[c];
					if (r < 0 || g.base[c] < 0)
						return;
					if (!m_land) {
						add({ Place(sphere.centre(r)), Place(sphere.centre(c)), false, 0 });
						return;
					}
					if (g.drained[c] < TRUNK_LEAST && !m_world->drawn(c) && !hollows)
						return;
					// (kept: a run's within reach of many windows round it, and each
					// gathered it again, most of a coarse planet's time; sprint 110.
					// Kept once for every thread, not per thread: a coarse planet's
					// tiles reach every run, past any thread's cache, and each
					// thread traced each run again; sprint 114)
					std::atomic<Pieces *> &slot = m_pieces[c];
					if (const Pieces *kept = slot.load(std::memory_order_acquire)) {
						for (const Segment &s : kept->pieces)
							add(s);
						return;
					}
					auto fresh = std::make_unique<Pieces>();
					Pieces &kept = *fresh;
					int made = 0;
					// the prototype's: the run's line, in pieces, at the grid's heights along it
					const fix hill = critical(g, c, m_steep);
					// (what it drains, in blocks²; a drawn run's water is the rivers', drawn apart)
					const int64_t cell = mulShift(m_lattice[0], m_radius, PLACE_FRAC) >> FRAC;
					const int64_t flow = m_world->drawn(c) ? 0 : mulShift(g.drained[c], cell * cell, FRAC);
					for (int piece = 0; piece < TRUNK_PIECES; piece++) {
						const fix down = ratio(piece + 1, TRUNK_PIECES), up = ratio(piece, TRUNK_PIECES);
						Segment s{ m_world->onRun(c, down), m_world->onRun(c, up), false, 0 };
						s.low = m_world->linkHeight(c, down);
						s.high = m_world->linkHeight(c, up);
						s.hill = hill;
						s.flow = flow;
						{
							const int64_t dx = (int64_t)s.at.x - s.join.x, dy = (int64_t)s.at.y - s.join.y, dz = (int64_t)s.at.z - s.join.z;
							const int64_t run = mulShift((int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)), m_radius, PLACE_FRAC);
							const fix grade = run > 0 ? (fix)std::clamp<int64_t>(((s.high - s.low) << FRAC) / run, 0, ONE) : 0;
							s.floor = floorOf(mulShift(g.drained[c], cell * cell, FRAC), grade);
							s.grade = grade;
						}
						s.ice = m_world->glacial(c, s.high);
						climate(s, mulShift(g.drained[c], cell * cell, FRAC), s.ice, g.rain(c));
						kept.pieces[made++] = s;
						add(s);
					}
					if (made != TRUNK_PIECES)
						throw std::logic_error("terragen::Channels: a run kept with pieces missing");
					// (another thread may have traced it too: the same pieces, so either's kept)
					Pieces *none = nullptr;
					if (slot.compare_exchange_strong(none, fresh.get(), std::memory_order_acq_rel))
						fresh.release();
				};
				const Place middle = placeOf(((int64_t)i * 2 + 1) * L / 2, ((int64_t)j * 2 + 1) * L / 2, ((int64_t)l * 2 + 1) * L / 2);
				std::vector<int32_t> cells;
				if (m_land) {
					// (the land's: every run within reach of a place here, by
					// distance; rings by count left some out, and the ground
					// jumped along the lattice's planes where a window lost one)
					const Place centre(middle.dir());
					const auto near = [&](int32_t c) {
						const Place q(sphere.centre(c));
						const int64_t dx = (int64_t)q.x - centre.x, dy = (int64_t)q.y - centre.y, dz = (int64_t)q.z - centre.z;
						return (int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)) <= m_trunk_ball;
					};
					// (what's in already, marked by a stamp a cell, the thread's:
					// searching the list for each neighbour was a quarter of a
					// read where every column's window is a new one; sprint 114)
					thread_local std::vector<uint32_t> marks;
					thread_local uint32_t stamp = 0;
					if (marks.size() < (size_t)sphere.size())
						marks.assign((size_t)sphere.size(), 0);
					if (++stamp == 0) { // wrapped round: an old mark could pass for this window's
						std::fill(marks.begin(), marks.end(), 0);
						stamp = 1;
					}
					const auto in = [&](int32_t c) { return marks[c] == stamp; };
					for (int32_t c : sphere.spot(middle.dir()).cells)
						if (!in(c)) {
							marks[c] = stamp;
							cells.push_back(c);
						}
					for (size_t at = 0; at < cells.size(); at++)
						for (const int32_t *q = sphere.neighboursBegin(cells[at]); q != sphere.neighboursEnd(cells[at]); q++)
							if (!in(*q)) {
								// (one out of reach is marked too: it's out from anywhere)
								marks[*q] = stamp;
								if (near(*q))
									cells.push_back(*q);
							}
				} else {
					for (int32_t c : sphere.spot(middle.dir()).cells) {
						cells.push_back(c);
						for (const int32_t *q = sphere.neighboursBegin(c); q != sphere.neighboursEnd(c); q++)
							cells.push_back(*q);
					}
				}
				std::sort(cells.begin(), cells.end());
				cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
				for (int32_t c : cells)
					link(c);
			} else {
				const int32_t w = m_land ? WINDOW : 2;
				for (int32_t a = i - w; a <= i + w; a++)
					for (int32_t b = j - w; b <= j + w; b++)
						for (int32_t c = l - w; c <= l + w; c++) {
							const Node n = node(k, a, b, c); // (a copy: its shape asks for more)
							if (!n.exists)
								continue;
							const bool own = only_lower && std::array<int32_t, 3>{ a, b, c } == below->own;
							if (only_lower && !own) {
								const int64_t r = rise(k, a, b, c);
								const std::array<int32_t, 3> at{ a, b, c };
								if (!(r < below->rise || (r == below->rise && at < below->self) || (below->with_self && at == below->self)))
									continue;
							}
							Segment s{ n.join, n.at, n.wet, n.rain };
							s.head = n.at;
							if (m_land) {
								// (first, and a copy: tracing reads, and shape's slot may go; the
								// way being traced as it's known, never asked again: that's a loop)
								// (a lower one of the traced way's own level by its way alone, not
								// the land under it: that asked for every lower one's round it, and
								// those theirs, down a whole basin, half a minute a column)
								const Path way = own ? *below->own_path : only_lower ? this->way(k, a, b, c) : path(k, a, b, c);
								const Shape &sh = shape(k, a, b, c);
								s.low = sh.low;
								s.high = sh.high;
								s.hill = sh.hill;
								s.flow = mulShift(sh.area, n.rain, FRAC);
								s.floor = floorOf(s.flow, sh.slope);
								s.grade = sh.slope;
								s.ice = sh.ice;
								s.cirque = sh.cirque_r;
								s.flat = sh.flat;
								s.grid_low = sh.grid_low;
								s.grid_high = sh.grid_high;
								s.over_low = sh.over_low;
								s.over_high = sh.over_high;
								climate(s, sh.area, sh.ice, n.rain);
								if (sh.fan_r > 0) {
									const int64_t dx = (int64_t)n.at.x - n.join.x, dy = (int64_t)n.at.y - n.join.y, dz = (int64_t)n.at.z - n.join.z;
									const int64_t length = mulShift((int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)), m_radius, PLACE_FRAC);
									s.fan = pathAt(way, (fix)std::min<int64_t>(ONE, (sh.fan_r << FRAC) / 2 / std::max<int64_t>(1, length)));
									s.fan_r = sh.fan_r;
								}
								if (sh.pond_r > 0) {
									s.pond = pathAt(way, sh.pond_t);
									s.pond_r = sh.pond_r;
									s.pond_depth = sh.pond_depth;
									s.pond_level = sh.pond_level;
								}
								// (the way being traced: its valley alone, no water yet, so
								// no banks raised to the very water this holds down)
								if (own)
									s.flow = 0;
								// its way, piece by piece
								for (int q = 0; q < PATH_PIECES; q++) {
									Segment piece = s;
									piece.join = way.at[q];
									piece.at = way.at[q + 1];
									piece.t0 = ratio(q, PATH_PIECES);
									piece.t1 = ratio(q + 1, PATH_PIECES);
									piece.ground_low = way.ground[q];
									piece.ground_high = way.ground[q + 1];
									add(piece);
								}
								continue;
							}
							add(s);
						}
			}
		}
		// read a little way off, by noise a few times finer than the level's
		// segments: its channels bend, and their valleys with them (alone each
		// is a straight run from node to node). Each level is read where the
		// coarser are, bent by their bends too, so a channel and what it joins
		// bend together; its own bend fades out toward its foot, so it ends
		// where it joins (on the first level, on the grid's water)
		int64_t bend[3] = { 0, 0, 0 };
		// (not on the prototype's land: its channels' ways are traced down it)
		if (k > 0 && !m_land)
			for (int c = 0; c < 3; c++)
				bend[c] = (int64_t)noise(p, m_seed + BEND + (uint32_t)(k * 3 + c), m_waves[k] * BEND_WAVES) * L / BEND_SHARE >> FRAC;
		const int64_t bx = (int64_t)p.x + bent[0], by = (int64_t)p.y + bent[1], bz = (int64_t)p.z + bent[2];
		// its bend fades out toward the level above's channels, the same for
		// all its own (by how far the place is from the nearest, as this
		// level reads them), so its channels end on them and, bent alike,
		// never cross each other (each fading toward its own foot, two crossed
		// by a hair, one's water stood over the other's)
		const int64_t fade = std::max<int64_t>(1, L / 2);
		const fix own = k == 0 || to_above2 == INT64_MAX ? ONE
				: smooth((fix)std::min<int64_t>(ONE, ((int64_t)isqrt((uint64_t)to_above2) << FRAC) / fade));
		const Place here_k = placeOf(bx + mulShift(bend[0], own, FRAC), by + mulShift(bend[1], own, FRAC), bz + mulShift(bend[2], own, FRAC));
		out.bent[k] = here_k;
		// (and where the next level reads, unfaded: how far that is from these)
		const bool finer = m_land && k < m_levels;
		const Place next = placeOf(bx + bend[0], by + bend[1], bz + bend[2]);
		int64_t next2 = INT64_MAX;
		// The trunk's window holds every run within reach of anywhere in it, so
		// most are out of reach of any one place: past both its hillside's
		// reach (its ground fades out) and the divide's (the hillside saturates
		// there), a run with no stream, pond or fan changes nothing read here.
		// A box round its ends tells, cheaper than its nearest point (most of
		// a coarse planet's channel reads went on runs out of reach; sprint 110)
		const int64_t out_of_reach = std::max(mulShift(L, TRUNK_REACH, FRAC), L * 3 / 4);
		const __int128 out_of_reach2 = (__int128)out_of_reach * out_of_reach;
		const int64_t reach = mulShift(L, REACH, FRAC);
		const __int128 reach2 = (__int128)reach * reach;
		for (int s = 0; s < list.count; s++) {
			const Segment &seg = list.segments[s];
			if (k == 0 && m_land && !seg.wet && seg.fan_r == 0 && seg.pond_r == 0) {
				const auto off = [](int64_t v, int64_t a, int64_t b) { return std::max<int64_t>({ 0, std::min(a, b) - v, v - std::max(a, b) }); };
				const int64_t ox = off(bx, seg.join.x, seg.at.x), oy = off(by, seg.join.y, seg.at.y), oz = off(bz, seg.join.z, seg.at.z);
				// (out along one axis alone, most are: no squares needed)
				if (ox >= out_of_reach || oy >= out_of_reach || oz >= out_of_reach ||
						(__int128)ox * ox + (__int128)oy * oy + (__int128)oz * oz >= out_of_reach2)
					continue;
			}
			// (a piece of a finer channel's way, as far: past its reach from
			// anywhere on it, so from its head, it shapes nothing here, and its
			// water's a corridor's width wide)
			if (k > 0 && m_land) {
				const auto off = [](int64_t v, int64_t a, int64_t b) { return std::max<int64_t>({ 0, std::min(a, b) - v, v - std::max(a, b) }); };
				const int64_t ox = off(bx, seg.join.x, seg.at.x), oy = off(by, seg.join.y, seg.at.y), oz = off(bz, seg.join.z, seg.at.z);
				if (ox >= reach || oy >= reach || oz >= reach || (__int128)ox * ox + (__int128)oy * oy + (__int128)oz * oz >= reach2)
					continue;
			}
			const Place pk = here_k;
			if (finer)
				next2 = std::min(next2, nearest(next, seg.join, seg.at).dist2);
			const Near near = nearest(pk, seg.join, seg.at);
			const fix tn = seg.t0 + mul(near.t, seg.t1 - seg.t0); // along its node's way, from its foot
			// (its distance and its bed there, each worked out once: the parts below all ask)
			int64_t root = -1, bed_here = INT64_MIN;
			const auto dist = [&] { return root >= 0 ? root : (root = (int64_t)isqrt((uint64_t)near.dist2)); };
			const auto bedHere = [&] { return bed_here != INT64_MIN ? bed_here : (bed_here = bedAt(seg.low, seg.high, seg.flat, tn)); };
			closest2 = std::min(closest2, near.dist2);
			// (the land's) how far its hillside has risen away: 0 in reach .. ONE
			// past it, where its window may have left it out
			fix gone = 0;
			int64_t rise = 0; // (blocks, fixed)
			if (m_land) {
				int64_t d, from, to;
				if (k == 0) {
					d = dist();
					from = mulShift(L, TRUNK_REACH_FROM, FRAC);
					to = mulShift(L, TRUNK_REACH, FRAC);
				} else {
					const int64_t dx = (int64_t)pk.x - seg.head.x, dy = (int64_t)pk.y - seg.head.y, dz = (int64_t)pk.z - seg.head.z;
					d = (int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz));
					from = mulShift(L, REACH_FROM, FRAC);
					to = mulShift(L, REACH, FRAC);
				}
				const fix x = d <= from ? 0 : d >= to ? ONE : (fix)(((d - from) << FRAC) / (to - from));
				gone = smooth(x);
				const fix steep = smooth(clamp(div(x - (ONE - FADE_STEEP), FADE_STEEP), 0, ONE));
				rise = mulShift(std::max(FADE_LEAST, mulShift(to - from, m_radius, PLACE_FRAC)), mul(mul(x, x), FADE_SLOPE / 2), FRAC) + mulShift(FADE_TO, steep, FRAC);
			}
			// a fan's cone, its edge at its foot's height
			if (seg.fan_r > 0) {
				const int64_t dx = (int64_t)pk.x - seg.fan.x, dy = (int64_t)pk.y - seg.fan.y, dz = (int64_t)pk.z - seg.fan.z;
				const int64_t d = mulShift((int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)), m_radius, PLACE_FRAC);
				if (d < seg.fan_r && gone < ONE) {
					const int64_t top = seg.low + mulShift(mulShift(seg.fan_r - d, mul(seg.grade, FAN_GRADE), FRAC), ONE - gone, FRAC);
					if (top > fan_top) {
						fan_top = top;
						fan = &seg;
					}
				}
			}
			if (seg.pond_r > 0) {
				const int64_t dx = (int64_t)pk.x - seg.pond.x, dy = (int64_t)pk.y - seg.pond.y, dz = (int64_t)pk.z - seg.pond.z;
				const int64_t d = mulShift((int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)), m_radius, PLACE_FRAC);
				const fix by = (fix)std::min<int64_t>(toFix(2), (d << FRAC) / seg.pond_r + gone * 2);
				if (by < pond_by) {
					pond_by = by;
					pond_d = d;
					pond = &seg;
				}
			}
			// its water, the network's: down its whole line, whatever valley's
			// the land's lowest (by the lowest, another's lower hillside took it)
			if (m_land && m_water && seg.flow >= STREAM_FLOW) {
				const int64_t gap = mulShift(dist(), m_radius, PLACE_FRAC);
				// (its level as the land's brought to the grid, by its own way
				// there: a trunk piece's where it is, as the grid's run is)
				int64_t level = bedHere();
				if (k > 0) {
					level = keepWith(level, seg.grid_low + mulShift(seg.grid_high - seg.grid_low, tn, FRAC),
							seg.over_low + mulShift(seg.over_high - seg.over_low, tn, FRAC));
					// (never over the land its way runs over: Path::ground)
					if (seg.ground_low != INT64_MAX && seg.ground_high != INT64_MAX)
						level = std::min(level, seg.ground_low + mulShift(seg.ground_high - seg.ground_low, near.t, FRAC));
				} else {
					if (grid_here == INT64_MIN)
						grid_here = m_world->gridHeight(p);
					level = keepWith(level, grid_here, overGrid(range));
				}
				const int64_t half = streamWidth(seg.flow) / 2;
				// (by how far from its edge: inside a wide one's water, a narrow
				// one's line nearer by still leaves it the wide one's)
				if (gap - half < wet_edge || (gap - half == wet_edge && seg.flow > wet_flow)) {
					wet_edge = gap - half;
					wet_gap = gap;
					wet_level = level;
					wet_flow = seg.flow;
					wet_from = k;
				}
				// (within its corridor only: past it both fall away four blocks a
				// block, under anything, so no window's edge shows)
				const int64_t past = std::max<int64_t>(0, gap - half - std::max(CORRIDOR, half * 2 * CORRIDOR_WIDTHS)) * 4;
				const int64_t off = std::max<int64_t>(0, gap - half);
				banks = std::max(banks, level + BANK_HIGH - mulShift(std::max<int64_t>(0, off - BANK_WIDE), CREST_FALL, FRAC) - past);
				cut = std::min(cut, gap < half ? level - ONE : level + mulShift(off, mul(seg.hill, vary), FRAC) + past);
			}
			// the ground: this channel's height here and its side rising to the place
			if (m_land && gone < ONE) {
				const int64_t gap = mulShift(dist(), m_radius, PLACE_FRAC);
				const fix hill = mul(seg.hill, vary);
				int64_t floor = seg.floor, wall_run = seg.wall_run;
				fix wall = seg.wall;
				int64_t bed = bedHere();
				// a cirque: its head scooped to a bowl, a near-level floor (from
				// its lip, CIRQUE_FROM up the channel) under a steep headwall
				// round it, as high as the bowl's wide, then the hillside
				if (seg.cirque > 0) {
					const fix bowl = smooth(clamp(div(tn - CIRQUE_FROM, ONE - CIRQUE_FROM), 0, ONE));
					if (bowl > 0) {
						const int64_t across = mulShift(seg.cirque, bowl, FRAC);
						const int64_t lip = seg.low + mulShift(seg.high - seg.low, CIRQUE_FROM, FRAC);
						bed = bed + mulShift(lip + (bed - lip) / 4 - bed, bowl, FRAC);
						floor = std::max(floor, across);
						wall = std::max(wall, lerp(hill, CIRQUE_WALL, bowl));
						wall_run = std::max(wall_run, across);
					}
				}
				const int64_t off = std::max<int64_t>(0, gap - floor), up = std::min(off, wall_run);
				const int64_t h = bed + mulShift(up, wall, FRAC) + mulShift(off - up, hill, FRAC) + rise;
				deepest = std::min(deepest, h * 2 - bed);
				if (pond == &seg)
					pond_h = h;
				if (h < lowest) {
					second = lowest;
					lowest = h;
					round = mulShift(mulShift(finest, ROUNDING, FRAC), hill, FRAC);
					owner = &seg;
					out.bed = bed;
					out.bed_level = k;
					out.wash = seg.wash && gap <= floor;
					out.ice = seg.ice;
					out.slope = gap <= floor ? seg.grade : off <= wall_run ? wall : hill;
				} else
					second = std::min(second, h);
			}
			if (seg.wet && near.dist2 < stream2) {
				stream2 = near.dist2;
				out.stream_from = k;
				out.stream_width = m_stream_width[k];
			}
			// the valley it's worn: as deep as its flow (√ of its rain), a V
			// VALLEY_WIDTH of its lattice across with a flat floor its stream's
			// width; a coarser level's deeper, as Horton's laws have it
			if (k > 0) {
				const int64_t half = L * VALLEY_WIDTH >> FRAC;
				const int64_t d = dist();
				if (d < half) {
					const fix x = (fix)((d << FRAC) / std::max<int64_t>(1, half));
					const fix deep = mul(weight, clamp(sqrt(clamp(div(seg.rain, WET_COARSE), 0, toFix(4))) / 2, 0, ONE));
					// (smooth across: a broad floor and shoulders, no groove down its middle)
					out.cut = std::max(out.cut, mul(deep, ONE - smooth(x)));
				}
			}
		}
		// across the hillside, valley 0 to divide 1: three quarters of the
		// lattice is about as far as a place gets from a channel of this level
		const int64_t saturate = L * 3 / 4;
		const fix x = closest2 >= saturate * saturate ? ONE : (fix)(((int64_t)isqrt((uint64_t)closest2) << FRAC) / saturate);
		// V-floored valleys with round tops; in rising ranges sharp divides too
		if (k == 1)
			out.trunk = x;
		const fix round = ONE - mul(ONE - x, ONE - x);
		const fix s = lerpFix(round, x, range);
		// a coarser valley's floor isn't cut again as deep
		const fix w = mul(weight, ratio(30, 100) + mul(above, ratio(70, 100)));
		sum += (int64_t)w * (2 * s - ONE) >> FRAC;
		norm += w;
		weight = mul(weight, FALL);
		above = x;
		for (int c = 0; c < 3; c++)
			bent[c] += bend[c];
		to_above2 = next2;
	}
	// centred on the land the grid gave (sharp divides sit lower: measured), and
	// as wide as the noise it replaces, so a range stays as high as it was
	out.relief = norm > 0 ? mul((fix)((sum << FRAC) / norm) + mul(range, ratio(35, 100)), ratio(60, 100)) : 0;
	if (lowest != INT64_MAX) {
		// a smooth minimum: where the two are within `round`, the divide's crest
		// is let down by up to a quarter of it
		const int64_t gap = second == INT64_MAX ? round : second - lowest;
		const int64_t over = std::max<int64_t>(0, round - gap);
		out.land = lowest - (round > 0 ? over * over / round / 4 : 0);
		out.clear = deepest - lowest;
		// a fan buries the ground under its cone; its own stream runs on it
		// (under it, it cut a slot as deep as the fan's thick)
		if (fan != nullptr && fan_top > out.land) {
			out.land = fan_top;
			out.slope = mul(fan->grade, FAN_GRADE);
		}
		// a pond: a bowl under its level, its water standing in it; round
		// it a sill holds the ground at its level at least (where its channel
		// runs out, the stream cuts back through), easing to the land outside.
		// (In its own valley only: where another's lower floor is the land,
		// a bowl stood perched over it, and its rim walled that valley off.
		// Fading by how far over the lowest its channel's ground is, as which
		// is lowest changes from column to column.)
		const fix own = pond != nullptr && pond_h != INT64_MAX
				? ONE - smooth((fix)std::min<int64_t>(ONE, ((pond_h - lowest) << FRAC) / POND_OWN)) : 0;
		if (pond != nullptr && pond_by < ONE + HALF && own > 0) {
			const int64_t level = pond->pond_level;
			if (pond_by < ONE) {
				const int64_t bowl = level - mulShift(pond->pond_depth, ONE - mul(pond_by, pond_by), FRAC);
				out.land += mulShift(bowl - out.land, own, FRAC);
				if (own >= HALF) {
					out.water = level;
					out.water_gap = 0;
					out.water_width = toFix(1 << 12);
					out.pond = true;
				}
			} else {
				const int64_t cone = level + mulShift(pond_d - pond->pond_r, pond->hill, FRAC);
				const int64_t held = std::clamp(out.land, level, std::max(level, cone));
				const fix out_by = smooth((pond_by - ONE) * 2);
				const int64_t rim = held + mulShift(out.land - held, out_by, FRAC);
				out.land += mulShift(rim - out.land, own, FRAC);
			}
		}
		// the land rolls where it's gentle, clear of the water
		if (m_land && !out.pond && out.bed != INT64_MAX) {
			const fix gentle = ONE - smooth(clamp(div(out.slope, ROLL_SLOPE), 0, ONE));
			const int64_t clear = out.clear;
			const fix away = smooth((fix)std::clamp<int64_t>((clear << FRAC) / std::max<int64_t>(1, mulShift(ROLL_CLEAR, m_small, FRAC)), 0, ONE));
			if (gentle > 0 && away > 0) {
				const fix swell = clamp(fbm(p, m_seed + ROLLS, m_roll_waves, 2, HALF) * 2, -ONE, ONE);
				const int64_t by = mulShift(mulShift(mulShift(mulShift(ROLL_HIGH, m_small, FRAC), swell, FRAC), mul(gentle, away), FRAC), ONE - own, FRAC);
				out.land += std::max(by, -clear / 2);
			}
		}
	}
	if (out.land != INT64_MAX && m_land) {
		toGrid(p, range, out);
		beds(p, out);
		// the network's water (already at the grid's: its levels run down its
		// way there), and the land gives way to it: its banks stand over it,
		// never a lower valley's hillside beside it, and nothing over its
		// channel (a fan, a sill, a swell, a bench: it runs on through, never under)
		if (!out.pond && wet_gap != INT64_MAX) {
			out.water = wet_level;
			out.water_gap = wet_gap;
			out.water_width = streamWidth(wet_flow);
			out.water_from = wet_from;
			out.water_flow = wet_flow;
			if (banks != INT64_MIN)
				out.land = std::max(out.land, banks);
			if (cut != INT64_MAX)
				out.land = std::min(out.land, cut);
		}
	}
	if (out.stream_from > 0)
		out.stream_gap = mulShift((int64_t)isqrt((uint64_t)stream2), m_radius, PLACE_FRAC);
	return out;
}


// Ground over the grid's surface brought towards it (OVER_GRID_M)
void Channels::toGrid(const Place &p, fix range, Read &out) const
{
	// (a rising range's peaks stand free: its ridges climb from the valleys
	// till they meet, RANGE_OVER times as far over the grid before they bend)
	const int64_t g = m_world->gridHeight(p), E = overGrid(range);
	const auto keep = [&](int64_t h) { return keepWith(h, g, E); };
	if (out.land > g) {
		// (its steepness by the map's: (E / (E + e))²)
		const fix by = (fix)((E << FRAC) / (E + out.land - g));
		out.slope = mul(out.slope, mul(by, by));
	}
	out.land = keep(out.land);
	if (out.bed != INT64_MAX)
		out.bed = keep(out.bed);
	if (out.water != INT64_MIN)
		out.water = keep(out.water);
}

int64_t Channels::overGrid(fix range) const
{
	return (OVER_GRID_M * m_world->metre()) * (ONE + mul(range, toFix(RANGE_OVER - 1))) >> FRAC;
}

// (World::channelsAt's: how much a place is a rising range)
fix Channels::rangeAt(const Place &p) const
{
	const fix belt = m_world->between(m_grid->tectonics.belt, p);
	return clamp(div(belt - ratio(18, 100), ratio(62, 100)), 0, ONE);
}

// Flat-lying beds step the land (BEDDED): every height is mapped through one
// bed's profile, the soft part gentle, the hard cap steep; one rising map of
// height, the same for the land and its water, so nothing that ran downhill
// runs up and no water leaves its bed
void Channels::beds(const Place &p, Read &out) const
{
	const Grid &g = *m_grid;
	const Sphere::Spot spot = gridSphere().spot(p.dir());
	fix bedded = 0;
	for (int k = 0; k < 3; k++)
		bedded += mul(mul(BEDDED[(int)g.province[spot.cells[k]]], ONE - clamp(g.tectonics.fold[spot.cells[k]], 0, ONE)), spot.weights[k]);
	bedded = mul(bedded, smooth(clamp(div(out.slope - BED_SLOPE, BED_SLOPE_FULL - BED_SLOPE), 0, ONE)));
	if (bedded <= 0)
		return;
	const fix thick_by = clamp(HALF + fbm(p, m_seed + BEDS, m_waves[0] / 2, 2, HALF), 0, ONE);
	const int64_t thick = mulShift(toFix(BED_LEAST) + mulShift(toFix(BED_MOST - BED_LEAST), thick_by, FRAC), m_small, FRAC);
	// the beds' profile over one: rising BED_SOFT_SLOPE as fast through the
	// soft, then the rest over the hard cap
	const fix soft = ONE - BED_HARD, soft_rise = mul(soft, BED_SOFT_SLOPE);
	const fix cliff = div(ONE - soft_rise, BED_HARD); // the cap's steepening
	fix steepening = ONE;
	const auto step = [&](int64_t h, bool slope) {
		if (h <= 0)
			return h; // (never lifting the sea's floor out, nor land into it)
		const int64_t whole = h / thick * thick;
		const fix x = (fix)((h - whole) * ONE / thick);
		const fix y = x < soft ? mul(x, BED_SOFT_SLOPE) : soft_rise + mul(x - soft, cliff);
		// eased in over the first bed, so the coast isn't lifted or sunk
		const fix by = mul(bedded, clamp((fix)(h * ONE / thick), 0, ONE));
		if (slope)
			steepening = ONE + mul(by, (x < soft ? BED_SOFT_SLOPE : cliff) - ONE);
		return h + mulShift(whole + mulShift(thick, y, FRAC) - h, by, FRAC);
	};
	out.land = step(out.land, true);
	if (out.bed != INT64_MAX)
		out.bed = step(out.bed, false);
	if (out.water != INT64_MIN)
		out.water = step(out.water, false);
	out.slope = (fix)std::min<int64_t>(toFix(4), mulShift(out.slope, steepening, FRAC));
}

} // namespace terragen
