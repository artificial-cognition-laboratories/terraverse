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
#include <vector>

namespace terragen {

using plates::mix;

namespace {

enum ChannelStream : uint32_t { NODE = 353, BEND = 379 }; // (359 was the levels' wander; 439–479 the drainage land's)
// how far a level's channels bend either way: its lattice over BEND_SHARE,
// by noise BEND_WAVES times its waves (the cells' lattice: about its own).
// Gentle enough never to fold, every level's together: the bend's slope
// (amplitude over wavelength) well under one. At a quarter of the lattice
// over noise half as wide, a bend's slope reached π: its line turned back on
// itself, a stream in loops and pieces (6–25% of the ground on the finer
// levels; the stream probe, phase 2)
const int64_t BEND_SHARE = 12;
const int32_t BEND_WAVES = 2;

// The finest level's points are at least this far apart (blocks)
const int64_t FINEST_BLOCKS = 12;
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

Channels::Channels(const WorldCode &code, const Grid &grid, const World &world, int64_t radius, int64_t cell_blocks) :
	m_grid(&grid), m_world(&world), m_seed(code.seed_hash + NODE), m_owner(nextOwner()),
	m_radius(radius), m_water(code.processes.water)
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

	int64_t best = INT64_MAX;
	bool below_wet = false; // what the best runs into carries water
	int32_t link = -1;      // the best's grid cell, on the first level
	fix link_t = 0;         // and how far along its run it joins
	const auto consider = [&](const Place &p, const Place &q, const std::array<int32_t, 3> &above, bool wet, int32_t cell) {
		const Near near = nearest(n.at, p, q);
		if (near.dist2 >= best)
			return;
		const Place join = along(p, q, near.t);
		const Sphere::Spot there = sphere.spot(join.dir());
		// not uphill of here
		if (field(g.eroded, there) > here)
			return;
		int64_t cost = near.dist2;
		if (folded > 0) {
			const int64_t across = mulShift(field(g.tectonics.front, there) - front, m_lattice[0], FRAC);
			cost += (across * across >> FRAC) * mul(folded, TRELLIS);
		}
		if (cost >= best)
			return;
		best = cost;
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
		consider(Place(sphere.centre(c)), Place(sphere.centre(r)), { c, 0, 0 }, m_world->drawn(c), c);
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
						consider(parent.join, parent.at, { p, q, r }, parent.wet, -1);
				}
	}
	if (best == INT64_MAX)
		return n; // nothing downhill near: no channel
	// a first level's foot on the grid's water where it's drawn, which curves
	// off the straight run between centres: the stream ends in it, not beside it
	// (nearest() measured from the run's upstream end: t along it from there)
	if (link >= 0 && below_wet)
		n.join = m_world->onRun(link, link_t);
	n.exists = true;
	n.link = link;
	n.rain = g.rain(spot.cells[0]);
	n.wet = m_water && level <= m_wet_levels && below_wet && n.rain >= m_wet[level];
	return n;
}


Channels::Read Channels::read(const Place &p, fix range, int64_t least) const
{
	Read out;
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	// What's near a place's lattice cell, kept per thread: columns come in
	// runs through one cell, and only a few of a window's cells hold a node
	struct List
	{
		uint64_t owner = 0;
		int32_t i = 0, j = 0, l = 0;
		std::vector<Segment> segments;
	};
	thread_local std::array<List, MOST_LEVELS + 1> lists;
	int64_t closest2 = INT64_MAX, stream2 = INT64_MAX;
	int64_t sum = 0, norm = 0;
	fix weight = ONE, above = ONE;
	int64_t bent[3] = { 0, 0, 0 }; // the coarser levels' bends, summed (place units)
	for (int k = 0; k <= m_levels && (k == 0 || m_lattice[k] >= least); k++) {
		const int64_t L = m_lattice[k];
		const int32_t i = floorBy(p.x, L), j = floorBy(p.y, L), l = floorBy(p.z, L);
		List &list = lists[k];
		if (list.owner != m_owner || list.i != i || list.j != j || list.l != l) {
			list.owner = m_owner;
			list.i = i;
			list.j = j;
			list.l = l;
			list.segments.clear();
			if (k == 0) {
				// the trunk: the grid's links round here
				const Place middle = placeOf(((int64_t)i * 2 + 1) * L / 2, ((int64_t)j * 2 + 1) * L / 2, ((int64_t)l * 2 + 1) * L / 2);
				std::vector<int32_t> cells;
				for (int32_t c : sphere.spot(middle.dir()).cells) {
					cells.push_back(c);
					for (const int32_t *q = sphere.neighboursBegin(c); q != sphere.neighboursEnd(c); q++)
						cells.push_back(*q);
				}
				std::sort(cells.begin(), cells.end());
				cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
				for (int32_t c : cells) {
					const int32_t r = g.receiver[c];
					if (r >= 0 && g.base[c] >= 0)
						list.segments.push_back({ Place(sphere.centre(r)), Place(sphere.centre(c)), false, 0 });
				}
			} else {
				for (int32_t a = i - 2; a <= i + 2; a++)
					for (int32_t b = j - 2; b <= j + 2; b++)
						for (int32_t c = l - 2; c <= l + 2; c++) {
							const Node &n = node(k, a, b, c);
							if (n.exists)
								list.segments.push_back({ n.join, n.at, n.wet, n.rain });
						}
			}
		}
		// read a little way off, by noise a few times finer than the level's
		// segments: its channels bend, and their valleys with them (alone each
		// is a straight run from node to node). Each level is read where the
		// coarser are, bent by their bends too, so a channel and what it joins
		// bend together
		int64_t bend[3] = { 0, 0, 0 };
		if (k > 0)
			for (int c = 0; c < 3; c++)
				bend[c] = (int64_t)noise(p, m_seed + BEND + (uint32_t)(k * 3 + c), m_waves[k] * BEND_WAVES) * L / BEND_SHARE >> FRAC;
		const Place pk = placeOf((int64_t)p.x + bent[0] + bend[0], (int64_t)p.y + bent[1] + bend[1], (int64_t)p.z + bent[2] + bend[2]);
		for (const Segment &seg : list.segments) {
			const Near near = nearest(pk, seg.join, seg.at);
			closest2 = std::min(closest2, near.dist2);
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
				const int64_t d = (int64_t)isqrt((uint64_t)near.dist2);
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
	// centred on the land the grid gave (sharp divides sit lower: measured)
	out.relief = norm > 0 ? mul((fix)((sum << FRAC) / norm) + mul(range, ratio(35, 100)), ratio(60, 100)) : 0;
	if (out.stream_from > 0)
		out.stream_gap = mulShift((int64_t)isqrt((uint64_t)stream2), m_radius, PLACE_FRAC);
	return out;
}

} // namespace terragen
