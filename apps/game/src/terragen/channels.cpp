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

enum ChannelStream : uint32_t { NODE = 353, WANDER = 359 };

// The finest level's points are this far apart (blocks): below it, noise
const int64_t FINEST_BLOCKS = 48;
// Rain a level needs to run with water (the grid's rain: 1 is average): the
// wetter the land, the further up its network the water reaches
const fix WET[Channels::STREAM_LEVELS] = { ratio(30, 100), ratio(70, 100), ratio(120, 100) };
// Each level's share of the relief, falling as real land's roughness does
const fix FALL = 37641; // 2^−0.8
// How much further a join across folded beds counts, squared
const fix TRELLIS = toFix(16);

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

Channels::Channels(const WorldCode &code, const Grid &grid, int64_t radius, int64_t cell_blocks) :
	m_grid(&grid), m_seed(code.seed_hash + NODE), m_owner(((uint64_t)code.seed_hash << 32 | (uint32_t)code.size) + 1),
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
	const fix folded = field(g.tectonics.fold, spot), front = field(g.tectonics.front, spot);

	int64_t best = INT64_MAX;
	const auto consider = [&](const Place &p, const Place &q, const std::array<int32_t, 3> &above) {
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
	};
	if (level == 1) {
		// the grid's tree: the links of the cells round here
		const auto link = [&](int32_t c) {
			const int32_t r = g.receiver[c];
			if (r < 0 || g.base[c] < 0)
				return;
			consider(Place(sphere.centre(c)), Place(sphere.centre(r)), { c, 0, 0 });
		};
		for (int32_t c : spot.cells) {
			link(c);
			for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++)
				link(*p);
		}
	} else {
		const int64_t A = m_lattice[level - 1];
		const int32_t i0 = floorBy((int64_t)n.at.x - A * 3 / 2, A), i1 = floorBy((int64_t)n.at.x + A * 3 / 2, A);
		const int32_t j0 = floorBy((int64_t)n.at.y - A * 3 / 2, A), j1 = floorBy((int64_t)n.at.y + A * 3 / 2, A);
		const int32_t l0 = floorBy((int64_t)n.at.z - A * 3 / 2, A), l1 = floorBy((int64_t)n.at.z + A * 3 / 2, A);
		for (int32_t p = i0; p <= i1; p++)
			for (int32_t q = j0; q <= j1; q++)
				for (int32_t r = l0; r <= l1; r++) {
					const Node parent = node(level - 1, p, q, r); // (a copy: asking again can overwrite its slot)
					if (parent.exists)
						consider(parent.join, parent.at, { p, q, r });
				}
	}
	if (best == INT64_MAX)
		return n; // nothing downhill near: no channel
	n.exists = true;
	n.rain = g.rain(spot.cells[0]);
	return n;
}

std::array<int64_t, 3> Channels::wander(const Place &at, int64_t least) const
{
	std::array<int64_t, 3> w{ 0, 0, 0 };
	// the finer levels' own wiggle, a sixth of a wavelength: the same for every
	// level, so tributaries still meet what they join
	// (every other level: one octave in two is hardly seen, and it's a cost per column)
	for (int k = 1; k <= m_levels && m_lattice[k] >= least; k += 2)
		for (int c = 0; c < 3; c++)
			w[c] += (int64_t)noise(at, m_seed + WANDER + (uint32_t)(k * 3 + c), m_waves[k]) * m_lattice[k] >> (FRAC + 1);
	return w;
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
	};
	struct List
	{
		uint64_t owner = 0;
		int32_t i = 0, j = 0, l = 0;
		int count = 0;
		std::array<Segment, 128> segments;
	};
	thread_local std::unique_ptr<std::array<List, MOST_LEVELS + 1>> lists(new std::array<List, MOST_LEVELS + 1>());
	int64_t closest2 = INT64_MAX, stream2 = INT64_MAX;
	int64_t sum = 0, norm = 0;
	fix weight = ONE, above = ONE;
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
					if (r >= 0 && g.base[c] >= 0)
						add({ Place(sphere.centre(r)), Place(sphere.centre(c)), false });
				};
				const Place middle = placeOf(((int64_t)i * 2 + 1) * L / 2, ((int64_t)j * 2 + 1) * L / 2, ((int64_t)l * 2 + 1) * L / 2);
				for (int32_t c : sphere.spot(middle.dir()).cells) {
					link(c);
					for (const int32_t *q = sphere.neighboursBegin(c); q != sphere.neighboursEnd(c); q++)
						link(*q);
				}
			} else {
				const bool wet = m_water && k <= STREAM_LEVELS;
				for (int32_t a = i - 2; a <= i + 2; a++)
					for (int32_t b = j - 2; b <= j + 2; b++)
						for (int32_t c = l - 2; c <= l + 2; c++) {
							const Node &n = node(k, a, b, c);
							if (n.exists)
								add({ n.join, n.at, wet && n.rain >= WET[k - 1] });
						}
			}
		}
		for (int s = 0; s < list.count; s++) {
			const Segment &seg = list.segments[s];
			const Near near = nearest(p, seg.join, seg.at);
			closest2 = std::min(closest2, near.dist2);
			if (seg.wet && near.dist2 < stream2) {
				stream2 = near.dist2;
				out.stream_from = k;
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
	}
	// centred on the land the grid gave (sharp divides sit lower: measured), and
	// as wide as the noise it replaces, so a range stays as high as it was
	out.relief = norm > 0 ? mul((fix)((sum << FRAC) / norm) + mul(range, ratio(35, 100)), ratio(60, 100)) : 0;
	if (out.stream_from > 0)
		out.stream_gap = mulShift((int64_t)isqrt((uint64_t)stream2), m_radius, PLACE_FRAC);
	return out;
}

} // namespace terragen
