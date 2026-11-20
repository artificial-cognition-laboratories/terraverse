// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/amplify.h"
#include "library/world/terragen/grid.h"
#include <algorithm>
#include <stdexcept>

namespace terragen {

namespace {

// Each octave's depth: RELIEF · k_s · cell, × FALL for each octave finer
// (2^−0.35: the big valleys organise the small, as drainage does), never
// steeper than CRITICAL (a hillside's gradient: past it, it slides)
const fix RELIEF = ratio(6, 10), FALL = ratio(7846, 10000), CRITICAL = ratio(35, 100);
// Below a hillside's length (metres) nothing's gullied; and never under SMALLEST blocks
const int32_t HILLSIDE_M = 400;
const int64_t SMALLEST = 4;
// The smooth surface: a Wendland kernel over the cells within REACH spacings
const fix REACH = ratio(12, 10);
// A lattice point's reach, in its lattice's spacings
const fix SPREAD = ratio(3, 2);
// The fractional bits of a place in blocks, here
const int SUB = 8;

struct V
{
	int64_t x, y, z; // fixed, or blocks at SUB bits (as the use says)
};

int64_t dot(const V &a, const V &b) { return (a.x * b.x + a.y * b.y + a.z * b.z) >> FRAC; }
V cross(const V &a, const V &b)
{
	return { (a.y * b.z - a.z * b.y) >> FRAC, (a.z * b.x - a.x * b.z) >> FRAC, (a.x * b.y - a.y * b.x) >> FRAC };
}
V scaled(const V &a, int64_t k) { return { (a.x * k) >> FRAC, (a.y * k) >> FRAC, (a.z * k) >> FRAC }; }
int64_t length(const V &a) { return (int64_t)isqrt((uint64_t)(a.x * a.x + a.y * a.y + a.z * a.z)); }
V unit(const V &a)
{
	const int64_t l = length(a);
	return l == 0 ? V{ 0, 0, 0 } : V{ (a.x << FRAC) / l, (a.y << FRAC) / l, (a.z << FRAC) / l };
}

int64_t floorDiv(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

uint64_t mix(uint64_t v)
{
	v ^= v >> 30;
	v *= 0xbf58476d1ce4e5b9ull;
	v ^= v >> 27;
	v *= 0x94d049bb133111ebull;
	v ^= v >> 31;
	return v;
}

} // namespace

Amplifier::Amplifier(const Grid &grid, int64_t radius, int64_t cell_blocks, fix height, fix metre, uint32_t seed) :
	m_grid(grid), m_radius(radius), m_cell_blocks(cell_blocks), m_height(height), m_seed(seed)
{
	const Sphere &sphere = gridSphere();
	// each cell's channel steepness: its fall to its receiver over the way
	// (blocks a block), × √ the rain it drains (in cells)
	m_ks.assign(grid.size(), 0);
	for (int32_t c = 0; c < grid.size(); c++) {
		const int32_t r = grid.receiver[c];
		if (r < 0 || grid.eroded[c] < 0)
			continue;
		const int64_t fall = (int64_t)std::max<fix>(0, grid.filled[c] - std::max<fix>(0, grid.filled[r])) * m_height >> FRAC; // blocks, fixed
		int64_t gap = 0;
		const fix *d = sphere.distancesBegin(c);
		for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++, d++)
			if (*p == r)
				gap = (int64_t)*d * m_radius; // blocks, fixed
		if (gap <= 0)
			continue;
		const int64_t slope = (fall << FRAC) / gap;
		m_ks[c] = (fix)std::min<int64_t>(INT32_MAX, mulShift(slope, sqrtWide64(std::max<int64_t>(ONE, grid.drained[c])), FRAC));
	}
	// the octaves: half a cell, halving, down to a hillside's length
	const int64_t least = std::max<int64_t>(SMALLEST, ((int64_t)HILLSIDE_M * metre) >> FRAC);
	for (int64_t l = (cell_blocks >> FRAC) / 2; l >= least; l /= 2)
		m_octaves.push_back(l);
}

fix Amplifier::smooth(const std::vector<fix> &field, const Place &p) const
{
	const Sphere &sphere = gridSphere();
	const int64_t reach = (int64_t)mul(REACH, sphere.spacing()) << (PLACE_FRAC - FRAC);
	// the cells near: the place's three, and two rings round them (a cell
	// within REACH of the place is within that of one of its three)
	int32_t near[96];
	int count = 0;
	const auto add = [&](int32_t c) {
		for (int k = 0; k < count; k++)
			if (near[k] == c)
				return;
		if (count < 96)
			near[count++] = c;
	};
	const Sphere::Spot spot = sphere.spot(p.dir());
	for (int32_t c : spot.cells) {
		add(c);
		for (const int32_t *q = sphere.neighboursBegin(c); q != sphere.neighboursEnd(c); q++) {
			add(*q);
			for (const int32_t *r = sphere.neighboursBegin(*q); r != sphere.neighboursEnd(*q); r++)
				add(*r);
		}
	}
	int64_t sum = 0, weights = 0;
	for (int k = 0; k < count; k++) {
		const int64_t d = chord(p, Place(sphere.centre(near[k])));
		if (d >= reach)
			continue;
		const fix q = (fix)((d << FRAC) / reach), o = ONE - q;
		const fix w = mul(mul(mul(o, o), mul(o, o)), 4 * q + ONE); // (1 − q)⁴ (4q + 1)
		sum += (int64_t)w * field[near[k]];
		weights += w;
	}
	if (weights <= 0)
		throw std::logic_error("terragen::Amplifier::smooth: no cell within reach");
	return (fix)(sum / weights);
}

int64_t Amplifier::relief(const Place &p, int64_t below) const
{
	if (m_octaves.empty())
		return 0;
	// the place's up, and two ways along the ground
	const Dir up = p.dir();
	const V n{ up.x, up.y, up.z };
	const V axis = std::abs(n.y) < ratio(9, 10) ? V{ 0, ONE, 0 } : V{ ONE, 0, 0 };
	const V t1 = unit(cross(axis, n)), t2 = cross(n, t1);
	// the smooth surface's slope (blocks a block): read a stride either way
	const int64_t stride = std::max<int64_t>(1, (m_cell_blocks >> FRAC) / 8); // blocks
	const auto offset = [&](const V &t, int64_t blocks) {
		const int64_t by = (blocks << PLACE_FRAC) / m_radius; // place units
		return placeOf((int64_t)p.x + (t.x * by >> FRAC), (int64_t)p.y + (t.y * by >> FRAC), (int64_t)p.z + (t.z * by >> FRAC));
	};
	const auto at = [&](const V &t, int64_t blocks) { return (int64_t)smooth(m_grid.eroded, offset(t, blocks)) * m_height >> FRAC; };
	const int64_t g1 = (at(t1, stride) - at(t1, -stride)) / (2 * stride), g2 = (at(t2, stride) - at(t2, -stride)) / (2 * stride);
	V slope{ (t1.x * g1 + t2.x * g2) >> FRAC, (t1.y * g1 + t2.y * g2) >> FRAC, (t1.z * g1 + t2.z * g2) >> FRAC };
	const int64_t ks = smooth(m_ks, p);

	// the place in blocks, at SUB fractional bits
	const V at_blocks{ (int64_t)p.x * m_radius >> (PLACE_FRAC - SUB), (int64_t)p.y * m_radius >> (PLACE_FRAC - SUB),
		(int64_t)p.z * m_radius >> (PLACE_FRAC - SUB) };
	int64_t height = 0, fall = ONE;
	for (size_t k = 0; k < m_octaves.size(); k++, fall = mulShift(fall, FALL, FRAC)) {
		const int64_t steep = length(slope);
		if (steep < 4 || (below > 0 && m_octaves[k] >= below))
			continue; // (no way down to groove; or the streams' own)
		const V across = cross(n, unit(slope));
		const int64_t L = m_octaves[k], spacing = L << SUB, reach = (int64_t)mul(SPREAD, (fix)L) << SUB;
		const int64_t cx = floorDiv(at_blocks.x, spacing), cy = floorDiv(at_blocks.y, spacing), cz = floorDiv(at_blocks.z, spacing);
		int64_t c = 0, s = 0;
		for (int64_t i = -1; i <= 1; i++)
			for (int64_t j = -1; j <= 1; j++)
				for (int64_t l = -1; l <= 1; l++) {
					// the lattice point, jittered within its cube
					const uint64_t h = mix(((uint64_t)(cx + i) * 0x9e3779b97f4a7c15ull) ^ ((uint64_t)(cy + j) * 0xc2b2ae3d27d4eb4full) ^
							((uint64_t)(cz + l) * 0x165667b19e3779f9ull) ^ ((uint64_t)m_seed << 32 | (uint64_t)k));
					const V o{ at_blocks.x - ((cx + i) * spacing + (int64_t)((h & 0xfffff) * (uint64_t)spacing >> 20)),
						at_blocks.y - ((cy + j) * spacing + (int64_t)((h >> 20 & 0xfffff) * (uint64_t)spacing >> 20)),
						at_blocks.z - ((cz + l) * spacing + (int64_t)((h >> 40 & 0xfffff) * (uint64_t)spacing >> 20)) };
					const int64_t r = (int64_t)isqrt((uint64_t)(o.x * o.x + o.y * o.y + o.z * o.z));
					if (r >= reach)
						continue;
					const fix q = ONE - (fix)((r << FRAC) / reach), w = mul(q, q);
					// its wave across the slope: in turns of the lattice's spacing
					const fix turns = (fix)(((o.x * across.x + o.y * across.y + o.z * across.z) / spacing) % ONE);
					fix sn, cs;
					sincos(mul(turns, TWO_PI), sn, cs);
					c += mul(w, cs);
					s += mul(w, sn);
				}
		const int64_t m = (int64_t)isqrt((uint64_t)(c * c + s * s));
		if (m < 16)
			continue;
		const int64_t cn = (c << FRAC) / m, sn = (s << FRAC) / m;
		// how deep: the drainage's fall at this size, held to a hillside's
		const int64_t a = std::min(mulShift(mulShift(mulShift(RELIEF, ks, FRAC), m_cell_blocks, FRAC), fall, FRAC),
				(int64_t)CRITICAL * L * ONE / TWO_PI);
		height += (a * cn) >> FRAC;
		// the slope it leaves the next octave: its grooves across this one
		const int64_t groove = -(((a * sn) >> FRAC) * TWO_PI >> FRAC) / L;
		slope = { slope.x + (across.x * groove >> FRAC), slope.y + (across.y * groove >> FRAC), slope.z + (across.z * groove >> FRAC) };
	}
	return height;
}

} // namespace terragen
