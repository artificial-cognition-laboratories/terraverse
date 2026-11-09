// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "channels.h"
#include "grid.h"
#include "noise.h"
#include "plates.h"
#include "world.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace terragen {

using plates::mix;
using plates::unit;

namespace {

enum ChannelStream : uint32_t { NODE = 353, BEND = 379, CLUMPS = 439, PONDS = 443, BEDS = 449 }; // (359 was the levels' wander)
// how far a level's channels bend either way: its lattice over this
const int64_t BEND_SHARE = 4;

// The finest level's points are this far apart (blocks): below it, noise
const int64_t FINEST_BLOCKS = 48;
// Rain a level needs to run with water (the grid's rain: 1 is average): the
// wetter the land, the further up its network the water reaches
const fix WET[Channels::STREAM_LEVELS] = { ratio(30, 100), ratio(70, 100), ratio(120, 100) };
// A level's stream's width (blocks, fixed), and how wide its valley is, of
// its lattice
const int64_t STREAM_WIDTH[Channels::STREAM_LEVELS] = { 5 * ONE, 3 * ONE, 2 * ONE };
const fix VALLEY_WIDTH = ratio(55, 100);
// Each level's share of the relief, falling as real land's roughness does
const fix FALL = 37641; // 2^−0.8
// How much further a join across folded beds counts, squared
const fix TRELLIS = toFix(16);
// The land from the drainage (sprint 108). A channel's slope is stream
// power's at steady state, S = ks / √(its flow), ks the grid's own run's (so
// a range's channels are steep and a plain's gentle, as the grid wore them);
// never under SLOPE_LEAST, nor over the hillsides' (critical(): a channel's
// head is a hillside's gully). Hillsides are the steepest ground there is:
// they rise from every channel at the critical slope, so the channels, gentler,
// cut valleys into them, and divides stand where two hillsides meet.
const fix SLOPE_LEAST = ratio(1, 4000);
const fix HILL_LEAST = ratio(1, 100), CRITICAL_RANGE = ratio(55, 100);
// Where two hillsides meet the divide's rounded over this many of the finest
// level's lattice (soil creeps over it: a smooth minimum, not a knife edge)
const fix ROUNDING = ratio(1, 2);
// A grid run's trunk valley follows its line (World::onRun) in this many pieces
const int TRUNK_PIECES = 12;
// A channel carries a stream once this much drains to it (blocks² of average
// rain: a few hundred blocks square), no wider than STREAM_WIDEST blocks
// (wider is the grid's rivers')
const int64_t STREAM_FLOW = 40000;
// A join facing upstream counts as this much further, squared
const int64_t JOIN_AGAINST = 4;
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
const fix STANDS_SOFT = ratio(70, 100), STANDS_SPAN = ratio(60, 100);
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
// A channel's floor: where it runs gentler than FLOOR_GRADE it lays its load
// down, a flat floor FLOOR_WIDTHS × √flow wide (a floodplain), none steeper
const fix FLOOR_GRADE = ratio(2, 100);
const int64_t FLOOR_WIDTHS = 4;
const int32_t FLOOR_MOST = 40;
const int32_t STREAM_WIDEST = 6;
// Ponds, where something held the water up (a sill across the channel, and
// a hollow worn or left behind it): tarns where the ice worked a channel's
// head (a cirque's floor scoured out, its lip of rock or moraine), on up to
// TARN_SHARE of them where it worked hardest; pools on gentle floors (oxbows,
// sinks, a beaver's dam) on POOL_SHARE of the channels gentler than
// FLOOR_GRADE. A pond's sill rises round it for half its radius more.
const fix TARN_SHARE = ratio(30, 100), POOL_SHARE = ratio(12, 100);
const int64_t TARN_LEAST = 12, TARN_MOST = 70, POOL_LEAST = 8, POOL_MOST = 60; // radius, blocks

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

int32_t floorBy(int64_t v, int64_t d)
{
	return (int32_t)(v >= 0 ? v / d : -((-v + d - 1) / d));
}

fix lerpFix(fix a, fix b, fix t) { return a + mul(b - a, t); }

// A cache per thread, per level, direct-mapped: the generator's threads and
// the far view ask for the same few nodes over and over
struct Slot
{
	uint64_t owner = 0;
	int32_t i = 0, j = 0, l = 0;
	Channels::Node node;
};
constexpr size_t SLOTS = 1 << 13;

} // namespace

Channels::Channels(const WorldCode &code, const Grid &grid, const World &world, int64_t radius, int64_t cell_blocks, bool land) :
	m_grid(&grid), m_world(&world), m_seed(code.seed_hash + NODE), m_owner(((uint64_t)code.seed_hash << 32 | (uint32_t)code.size) + 1),
	m_radius(radius), m_water(code.processes.water), m_land(land)
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
	}
	const int32_t cell_waves = (int32_t)(((int64_t)TWO_PI << FRAC) / gridSphere().spacing() >> FRAC);
	for (int k = 0; k <= m_levels; k++)
		m_waves[k] = std::max(8, (cell_waves << k) / 2);
}

int32_t Channels::latticeOf(int level, int32_t c) const
{
	return floorBy(c, m_lattice[level]);
}

const Channels::Node &Channels::node(int level, int32_t i, int32_t j, int32_t l) const
{
	thread_local std::unique_ptr<Slot[]> slots(new Slot[SLOTS * (MOST_LEVELS + 1)]);
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, 0x51ed270bu);
	Slot &s = slots[(size_t)level * SLOTS + (h & (SLOTS - 1))];
	if (s.owner != m_owner || s.i != i || s.j != j || s.l != l) {
		// (built first: building asks for nodes itself, which may land in this slot)
		Node built = build(level, i, j, l);
		Slot &t = slots[(size_t)level * SLOTS + (h & (SLOTS - 1))];
		t.owner = m_owner;
		t.i = i;
		t.j = j;
		t.l = l;
		t.node = built;
		return t.node;
	}
	return s.node;
}

static fix stands(const Grid &g, int32_t c)
{
	return STANDS_SOFT + mul(STANDS_SPAN, STRENGTH[(int)g.province[c]]);
}

// How steep hillsides stand at a place: as steep as the ground holds
// (CRITICAL_RANGE) only where it rises fastest; where it rises slowly they're
// as steep as soil creeping down has to be to carry off what rises, which grows
// with the uplift (Tectonics::belt: a shield's 0.03, a young plain's 0.18, a
// range's to 1), as its 1.5th power: an old plain stays low and gentle
// (P3) and with the rock: hard rock stands steeper, soft wears gentler
static fix critical(const Grid &g, int32_t c)
{
	const fix rising = clamp(div(g.tectonics.belt[c], ratio(80, 100)), 0, ONE);
	return mul(HILL_LEAST + mul(CRITICAL_RANGE - HILL_LEAST, mul(rising, sqrt(rising))), stands(g, c));
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

Channels::Node Channels::build(int level, int32_t i, int32_t j, int32_t l) const
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
	if (m_land && level > 1) {
		const fix dense = clamp(HALF + fbm(n.at, m_seed + CLUMPS + (uint32_t)level, m_waves[level] / 3, 2, HALF) * 2, DENSE_LEAST, ONE);
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
	const fix folded = field(g.tectonics.fold, spot), front = field(g.tectonics.front, spot);

	// The prototype's land: water takes the way down, not the nearest line.
	// Each candidate's ground where this would join it (a rough height: the
	// network's, its flow guessed from its lattice cell) plus the climb from
	// there at the hillside's slope is the land this would stand on; the lowest
	// wins, so a deep valley's wall is cut by what drains into it (by distance,
	// a node joined the nearest channel, and a deep trunk's wall stood bare)
	const int32_t home = spot.cells[0];
	const fix hill = m_land ? critical(g, home) : 0;
	const auto blocks = [&](int64_t place) { return mulShift(place, m_radius, PLACE_FRAC); };
	int64_t join_low = 0; // the best's rough height where it joins
	int64_t best = INT64_MAX;
	bool below_wet = false; // what the best runs into carries water
	int32_t link = -1;      // the best's grid cell, on the first level
	fix link_t = 0;         // and how far along its run it joins
	// `downstream`: the way water runs along p → q (+1) or q → p (−1)
	const auto consider = [&](const Place &p, const Place &q, const std::array<int32_t, 3> &above, bool wet, int32_t cell, int downstream, int64_t low, int64_t high) {
		const Near near = nearest(n.at, p, q);
		if (!m_land && near.dist2 >= best)
			return;
		const Place join = along(p, q, near.t);
		const Sphere::Spot there = sphere.spot(join.dir());
		// not uphill of here (the prototype's land joins by the heights it
		// gives: a deep run sits far under its cells' mean, so by the grid's
		// field its own walls were uphill of it and nothing cut them)
		if (!m_land && field(g.eroded, there) > here)
			return;
		int64_t cost = near.dist2;
		// A tributary meets what it joins heading downstream with it (40–70°
		// as rivers do), never against it: a join facing upstream costs
		// JOIN_AGAINST× (or every stream joins the nearest point, often a
		// node, and a run's tributaries fan into it from all round)
		if (m_land) {
			const Place on = along(p, q, near.t);
			const int64_t vx = (int64_t)on.x - n.at.x, vy = (int64_t)on.y - n.at.y, vz = (int64_t)on.z - n.at.z;
			const int64_t dx = ((int64_t)q.x - p.x) * downstream, dy = ((int64_t)q.y - p.y) * downstream, dz = ((int64_t)q.z - p.z) * downstream;
			const __int128 with = (__int128)vx * dx + (__int128)vy * dy + (__int128)vz * dz;
			if (with < 0)
				cost *= JOIN_AGAINST;
		}
		if (folded > 0) {
			const int64_t across = mulShift(field(g.tectonics.front, there) - front, m_lattice[0], FRAC);
			cost += (across * across >> FRAC) * mul(folded, TRELLIS);
		}
		const int64_t height = low + mulShift(high - low, near.t, FRAC);
		if (m_land)
			cost = height + mulShift(blocks((int64_t)isqrt((uint64_t)cost)), hill, FRAC);
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
	n.rain = g.rain(spot.cells[0]);
	n.wet = m_water && level <= STREAM_LEVELS && below_wet && n.rain >= WET[level - 1];
	return n;
}


const Channels::Shape &Channels::shape(int level, int32_t i, int32_t j, int32_t l) const
{
	struct Kept
	{
		uint64_t owner = 0;
		int32_t i = 0, j = 0, l = 0;
		Shape shape;
	};
	thread_local std::unique_ptr<Kept[]> kept(new Kept[SLOTS * (MOST_LEVELS + 1)]);
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, 0x2545f491u);
	Kept *k = &kept[(size_t)level * SLOTS + (h & (SLOTS - 1))];
	if (k->owner != m_owner || k->i != i || k->j != j || k->l != l) {
		const Shape worn = wear(level, i, j, l); // (asks for its parent's: may land in this slot)
		k = &kept[(size_t)level * SLOTS + (h & (SLOTS - 1))];
		k->owner = m_owner;
		k->i = i;
		k->j = j;
		k->l = l;
		k->shape = worn;
	}
	return k->shape;
}

int64_t Channels::drains(int level, int32_t i, int32_t j, int32_t l) const
{
	struct Kept
	{
		uint64_t owner = 0;
		int32_t i = 0, j = 0, l = 0;
		int64_t area = 0;
	};
	thread_local std::unique_ptr<Kept[]> kept(new Kept[SLOTS * (MOST_LEVELS + 1)]);
	const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, 0x6a09e667u);
	{
		const Kept &k = kept[(size_t)level * SLOTS + (h & (SLOTS - 1))];
		if (k.owner == m_owner && k.i == i && k.j == j && k.l == l)
			return k.area;
	}
	// its own lattice cell's hillsides, and all its children's (the next
	// level's nodes that join it: they lie within a lattice or two of it)
	const Node n = node(level, i, j, l);
	const int64_t own = mulShift(m_lattice[std::min(level + 1, m_levels)], m_radius, PLACE_FRAC) >> FRAC;
	int64_t area = own * own;
	if (n.exists && level < m_levels) {
		// its children found, each counted as what a child of that level
		// drains on average: its own cell's and each finer level's again (a
		// level's cells cover the ground once). Counted down every level, the
		// first column in a place built the whole network under a coarse
		// node: a far tile, which reads only the coarse levels, 1 s a column
		const int64_t child_own = mulShift(m_lattice[std::min(level + 2, m_levels)], m_radius, PLACE_FRAC) >> FRAC;
		const int64_t below = child_own * child_own * (m_levels - level);
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
	Kept &k = kept[(size_t)level * SLOTS + (h & (SLOTS - 1))];
	k.owner = m_owner;
	k.i = i;
	k.j = j;
	k.l = l;
	k.area = area;
	return area;
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
	const fix most = critical(g, c);
	out.slope = slopeFor(c, flow, most);
	out.hill = most;
	// its foot's height: on the grid's run, or the level above's channel where it joins
	fix floor_grade = ONE; // what it joins: how steep there
	if (level == 1) {
		out.low = m_world->linkHeight(n.link, n.t);
		const int32_t to = g.receiver[n.link];
		if (to >= 0) {
			const int64_t run = blocks(apart(Place(sphere.centre(n.link)), Place(sphere.centre(to))));
			const int64_t drop = m_world->linkHeight(n.link, 0) - m_world->linkHeight(n.link, ONE);
			if (run > 0)
				floor_grade = (fix)std::clamp<int64_t>((drop << FRAC) / run, 0, ONE);
		}
	} else {
		const Shape &above = shape(level - 1, n.parent[0], n.parent[1], n.parent[2]);
		out.low = above.low + mulShift(above.high - above.low, n.t, FRAC);
		floor_grade = above.slope;
	}
	if (m_land && floor_grade < FAN_FLOOR && out.slope >= floor_grade * FAN_STEEPER) {
		const fix dry = ratio(160, 100) - clamp(div(n.rain - FAN_DRY, FAN_WET - FAN_DRY), 0, ONE);
		const int64_t radius = mulShift(mulShift(toFix((int64_t)isqrt((uint64_t)out.area)), FAN_SHARE, FRAC), dry, FRAC);
		out.fan_r = std::min(std::clamp<int64_t>(radius, toFix(FAN_LEAST), toFix(FAN_MOST)), length);
	}
	out.high = out.low + mulShift(length, out.slope, FRAC);
	out.ice = m_land ? m_world->glacial(c, out.high) : 0;
	// a pond on it: a tarn near a worked head, or a pool on a gentle floor
	if (m_land && m_water && level > 1) {
		const uint32_t h = mix(mix((uint32_t)i, (uint32_t)j, (uint32_t)level), (uint32_t)l, m_seed + PONDS);
		const fix roll = (fix)(h & 0xFFFF), where = (fix)(h >> 16 & 0xFFFF);
		const fix ice = out.ice;
		int64_t radius = 0;
		if (ice > 0 && roll < mul(ice, TARN_SHARE)) {
			radius = std::clamp<int64_t>(length / 5, toFix(TARN_LEAST), toFix(TARN_MOST));
			out.pond_t = ratio(80, 100) + mul(where, ratio(10, 100));
			out.pond_depth = radius / 3;
		} else if (out.slope < FLOOR_GRADE && roll < POOL_SHARE) {
			radius = std::clamp<int64_t>(floorOf(flow, out.slope) * 2, toFix(POOL_LEAST), toFix(POOL_MOST));
			out.pond_t = ratio(25, 100) + mul(where, ratio(50, 100));
			out.pond_depth = std::max<int64_t>(toFix(1), radius / 6);
		}
		// (no bigger than a quarter of its channel: a pond's in its valley)
		if (radius > 0 && radius * 4 <= length) {
			out.pond_r = radius;
			// its level the channel's at its downstream rim
			const int64_t centre = out.low + mulShift(out.high - out.low, out.pond_t, FRAC);
			out.pond_level = std::max(out.low, centre - mulShift(radius, out.slope, FRAC));
		}
	}
	return out;
}

Channels::Read Channels::read(const Place &p, fix range, int64_t least) const
{
	Read out;
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	// What's near a place's lattice cell, kept per thread: columns come in runs
	// through one cell, and only a few of a window's cells hold a node
	struct Segment
	{
		Place join, at;
		bool wet;
		fix rain;
		int64_t low = 0, high = 0; // heights at its foot and head (blocks, fixed)
		fix hill = 0, grade = 0; // its sides' slope, and its own
		int64_t flow = 0; // what runs down it, blocks² of average rain (the grid's runs: none drawn here)
		int64_t floor = 0; // its flat floor's half width (blocks, fixed)
		Place pond; // its pond's centre, if pond_r > 0 (Shape's)
		bool wash = false; // a dry channel's floor (WASH_RAIN)
		fix wall = 0;      // a trough's walls' slope, rising wall_run off its floor (blocks, fixed), then hill
		int64_t wall_run = 0;
		fix ice = 0;       // how much ice worked it
		Place fan;  // its fan's centre, half fan_r up from its foot, if fan_r > 0
		int64_t fan_r = 0;
		int64_t pond_r = 0, pond_depth = 0, pond_level = 0;
	};
	struct List
	{
		uint64_t owner = 0;
		int32_t i = 0, j = 0, l = 0;
		int count = 0;
		std::array<Segment, 512> segments;
	};
	thread_local std::unique_ptr<std::array<List, MOST_LEVELS + 1>> lists(new std::array<List, MOST_LEVELS + 1>());
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
	// the land: the lowest channel-and-hillside, the next, and how much the
	// divide between them is rounded (blocks, fixed)
	int64_t lowest = INT64_MAX, second = INT64_MAX, round = 0;
	const int64_t finest = mulShift(m_lattice[m_levels], m_radius, PLACE_FRAC);
	// the pond nearest, by its radius: how far off (blocks, fixed) and which
	int64_t pond_d = 0;
	// the highest fan's cone over here, and whose (P4)
	int64_t fan_top = INT64_MIN;
	const Segment *fan = nullptr, *owner = nullptr;
	const Segment *pond = nullptr;
	fix pond_by = toFix(2);
	for (int k = 0; k <= m_levels && (k == 0 || m_lattice[k] >= least); k++) {
		const int64_t L = m_lattice[k];
		const int32_t i = floorBy(p.x, L), j = floorBy(p.y, L), l = floorBy(p.z, L);
		List &list = (*lists)[k];
		if (list.owner != m_owner || list.i != i || list.j != j || list.l != l) {
			list.owner = m_owner;
			list.i = i;
			list.j = j;
			list.l = l;
			list.count = 0;
			const auto add = [&](const Segment &s) {
				if (list.count == (int)list.segments.size())
					throw std::logic_error("terragen::Channels: more segments round a place than a list holds");
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
					// the prototype's: the run's line, in pieces, at the grid's heights along it
					const fix hill = critical(g, c);
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
						add(s);
					}
				};
				const Place middle = placeOf(((int64_t)i * 2 + 1) * L / 2, ((int64_t)j * 2 + 1) * L / 2, ((int64_t)l * 2 + 1) * L / 2);
				// (the land's: two rings, as a run's hillside can reach that far
				// and still be the lowest: one ring, and the ground jumped along
				// the lattice's planes where a list lost it)
				std::vector<int32_t> cells;
				for (int32_t c : sphere.spot(middle.dir()).cells) {
					cells.push_back(c);
					for (const int32_t *q = sphere.neighboursBegin(c); q != sphere.neighboursEnd(c); q++) {
						cells.push_back(*q);
						if (m_land)
							for (const int32_t *w = sphere.neighboursBegin(*q); w != sphere.neighboursEnd(*q); w++)
								cells.push_back(*w);
					}
				}
				std::sort(cells.begin(), cells.end());
				cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
				for (int32_t c : cells)
					link(c);
			} else {
				for (int32_t a = i - 2; a <= i + 2; a++)
					for (int32_t b = j - 2; b <= j + 2; b++)
						for (int32_t c = l - 2; c <= l + 2; c++) {
							const Node n = node(k, a, b, c); // (a copy: its shape asks for more)
							if (!n.exists)
								continue;
							Segment s{ n.join, n.at, n.wet, n.rain };
							if (m_land) {
								const Shape &sh = shape(k, a, b, c);
								s.low = sh.low;
								s.high = sh.high;
								s.hill = sh.hill;
								s.flow = mulShift(sh.area, n.rain, FRAC);
								s.floor = floorOf(s.flow, sh.slope);
								s.grade = sh.slope;
								s.ice = sh.ice;
								climate(s, sh.area, sh.ice, n.rain);
								if (sh.fan_r > 0) {
									const int64_t dx = (int64_t)n.at.x - n.join.x, dy = (int64_t)n.at.y - n.join.y, dz = (int64_t)n.at.z - n.join.z;
									const int64_t length = mulShift((int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)), m_radius, PLACE_FRAC);
									s.fan = along(n.join, n.at, (fix)std::min<int64_t>(ONE, (sh.fan_r << FRAC) / 2 / std::max<int64_t>(1, length)));
									s.fan_r = sh.fan_r;
								}
								if (sh.pond_r > 0) {
									s.pond = along(n.join, n.at, sh.pond_t);
									s.pond_r = sh.pond_r;
									s.pond_depth = sh.pond_depth;
									s.pond_level = sh.pond_level;
								}
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
		if (k > 0)
			for (int c = 0; c < 3; c++)
				bend[c] = (int64_t)noise(p, m_seed + BEND + (uint32_t)(k * 3 + c), m_waves[k] * 4) * L / BEND_SHARE >> FRAC;
		const int64_t bx = (int64_t)p.x + bent[0], by = (int64_t)p.y + bent[1], bz = (int64_t)p.z + bent[2];
		const int64_t fade = std::max<int64_t>(1, L / 2);
		for (int s = 0; s < list.count; s++) {
			const Segment &seg = list.segments[s];
			Place pk = placeOf(bx, by, bz);
			if (k > 0) {
				const int64_t dx = bx - seg.join.x, dy = by - seg.join.y, dz = bz - seg.join.z;
				const int64_t foot = (int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz));
				const fix own = smooth((fix)std::min<int64_t>(ONE, (foot << FRAC) / fade));
				pk = placeOf(bx + mulShift(bend[0], own, FRAC), by + mulShift(bend[1], own, FRAC), bz + mulShift(bend[2], own, FRAC));
			}
			const Near near = nearest(pk, seg.join, seg.at);
			closest2 = std::min(closest2, near.dist2);
			// a fan's cone, its edge at its foot's height
			if (seg.fan_r > 0) {
				const int64_t dx = (int64_t)pk.x - seg.fan.x, dy = (int64_t)pk.y - seg.fan.y, dz = (int64_t)pk.z - seg.fan.z;
				const int64_t d = mulShift((int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)), m_radius, PLACE_FRAC);
				if (d < seg.fan_r) {
					const int64_t top = seg.low + mulShift(seg.fan_r - d, mul(seg.grade, FAN_GRADE), FRAC);
					if (top > fan_top) {
						fan_top = top;
						fan = &seg;
					}
				}
			}
			if (seg.pond_r > 0) {
				const int64_t dx = (int64_t)pk.x - seg.pond.x, dy = (int64_t)pk.y - seg.pond.y, dz = (int64_t)pk.z - seg.pond.z;
				const int64_t d = mulShift((int64_t)isqrt((uint64_t)(dx * dx + dy * dy + dz * dz)), m_radius, PLACE_FRAC);
				const fix by = (fix)std::min<int64_t>(toFix(2), (d << FRAC) / seg.pond_r);
				if (by < pond_by) {
					pond_by = by;
					pond_d = d;
					pond = &seg;
				}
			}
			// the ground: this channel's height here and its side rising to the place
			if (m_land) {
				const int64_t gap = mulShift((int64_t)isqrt((uint64_t)near.dist2), m_radius, PLACE_FRAC);
				const int64_t off = std::max<int64_t>(0, gap - seg.floor), up = std::min(off, seg.wall_run);
				const int64_t h = seg.low + mulShift(seg.high - seg.low, near.t, FRAC) + mulShift(up, seg.wall, FRAC) + mulShift(off - up, seg.hill, FRAC);
				if (h < lowest) {
					second = lowest;
					lowest = h;
					round = mulShift(mulShift(finest, ROUNDING, FRAC), seg.hill, FRAC);
					owner = &seg;
					out.bed = seg.low + mulShift(seg.high - seg.low, near.t, FRAC);
					out.wash = seg.wash && gap <= seg.floor;
					out.ice = seg.ice;
					out.slope = gap <= seg.floor ? seg.grade : off <= seg.wall_run ? seg.wall : seg.hill;
					// its water: a stream once enough drains to it, wider as √ of the flow
					if (m_water && seg.flow >= STREAM_FLOW) {
						out.water = seg.low + mulShift(seg.high - seg.low, near.t, FRAC);
						out.water_gap = gap;
						out.water_width = std::min<int64_t>(toFix(STREAM_WIDEST), toFix(1) + (int64_t)isqrt((uint64_t)(seg.flow / STREAM_FLOW)) * ONE / 2);
						out.water_from = k;
					} else {
						out.water = INT64_MIN;
						out.water_gap = INT64_MAX;
						out.water_from = 0;
					}
				} else
					second = std::min(second, h);
			}
			if (seg.wet && near.dist2 < stream2) {
				stream2 = near.dist2;
				out.stream_from = k;
				out.stream_width = STREAM_WIDTH[k - 1];
			}
			// the valley it's worn: as deep as its flow (√ of its rain), a V
			// VALLEY_WIDTH of its lattice across with a flat floor its stream's
			// width; a coarser level's deeper, as Horton's laws have it
			if (k > 0) {
				const int64_t half = L * VALLEY_WIDTH >> FRAC;
				const int64_t d = (int64_t)isqrt((uint64_t)near.dist2);
				if (d < half) {
					const fix x = (fix)((d << FRAC) / std::max<int64_t>(1, half));
					const fix deep = mul(weight, clamp(sqrt(clamp(div(seg.rain, WET[0]), 0, toFix(4))) / 2, 0, ONE));
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
		// a fan buries the ground under its cone; its own stream runs on it
		// (under it, it cut a slot as deep as the fan's thick)
		if (fan != nullptr && fan_top > out.land) {
			out.land = fan_top;
			out.slope = mul(fan->grade, FAN_GRADE);
			if (owner == fan && out.water != INT64_MIN)
				out.water = std::max(out.water, fan_top);
		}
		// a pond: a bowl under its level, its water standing in it; round
		// it a sill holds the ground at its level at least (where its channel
		// runs out, the stream cuts back through), easing to the land outside.
		// (Where a lower valley crosses, the sill dams it: a pond only where
		// the land's lower, and the bowl's a dry ring round a hollow.)
		if (pond != nullptr && pond_by < ONE + HALF) {
			const int64_t level = pond->pond_level;
			if (pond_by < ONE) {
				out.land = level - mulShift(pond->pond_depth, ONE - mul(pond_by, pond_by), FRAC);
				out.water = level;
				out.water_gap = 0;
				out.water_width = toFix(1 << 12);
				out.pond = true;
			} else {
				const int64_t cone = level + mulShift(pond_d - pond->pond_r, pond->hill, FRAC);
				const int64_t held = std::clamp(out.land, level, std::max(level, cone));
				const fix out_by = smooth((pond_by - ONE) * 2);
				out.land = held + mulShift(out.land - held, out_by, FRAC);
			}
		}
	}
	if (out.land != INT64_MAX && m_land)
		beds(p, out);
	if (out.stream_from > 0)
		out.stream_gap = mulShift((int64_t)isqrt((uint64_t)stream2), m_radius, PLACE_FRAC);
	return out;
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
	const int64_t thick = toFix(BED_LEAST) + mulShift(toFix(BED_MOST - BED_LEAST), thick_by, FRAC);
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
