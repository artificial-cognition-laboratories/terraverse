// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "geology.h"
#include "noise.h"
#include "world.h"
#include <algorithm>

namespace terragen {

namespace {

enum VegStream : uint32_t { WOODS = 211, SPOT = 223, ROLL = 227 };

uint32_t mix(uint32_t a, uint32_t b, uint32_t seed)
{
	uint32_t h = seed ^ (a * 0x27d4eb2du) ^ (b * 0x165667b1u);
	h = (h ^ (h >> 15)) * 0x2c1b3c6du;
	h = (h ^ (h >> 12)) * 0x297a2d39u;
	return h ^ (h >> 15);
}

// 0 below lo, 1 above hi, a straight ramp between
fix ramp(fix v, fix lo, fix hi)
{
	return clamp(div(v - lo, hi - lo), 0, ONE);
}

const int32_t SPOT_CELL = 5; // blocks: at most one tree per 5×5

} // namespace

Cover World::cover(const Place &at, const Ground &ground) const
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	const Column &c = ground.column;
	// the column's own roll: by its block at the surface, so neighbours differ
	const std::array<int32_t, 3> p = solid(at, 0);
	Cover out{ 0, 0, 0, false, false, mix((uint32_t)p[0], mix((uint32_t)p[1], (uint32_t)p[2], ROLL), m_code.seed_hash + ROLL) };
	if (ground.kind != Water::None || c.elevation < 0 || !m_code.processes.life)
		return out; // (nothing grows on a barren body)
	out.fertility = between(g.fertility, at);

	// the lie of the land, from the coarse grid: over a plane, a ring of k
	// neighbours' slopes s holds Σ s² = k·|gradient|² / 2
	int32_t i = cell(at);
	{
		int64_t squares = 0;
		const fix *gap = sphere.distancesBegin(i);
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, gap++) {
			const fix s = div(mul(g.eroded[*p] - g.eroded[i], m_height), (fix)blocks(*gap)); // rise over run, blocks (a cell apart: fits)
			squares += (int64_t)s * s;
		}
		out.slope = sqrtWide64(((squares * 2) / sphere.neighbourCount(i)) >> FRAC);
	}

	// beside water: a river or lake within a cell
	out.bank = g.river(i) || g.lake[i];
	for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i) && !out.bank; p++)
		out.bank = g.river(*p) || g.lake[*p];

	// warmth (heat already falls with height: treelines emerge) and water
	fix warm = ramp(c.heat, -ratio(45, 100), -ratio(15, 100));
	fix wet = ramp(c.wet, -ratio(40, 100), ratio(30, 100));
	if (out.bank) // rivers water their banks through dry country
		wet = std::max(wet, ratio(75, 100));
	fix density = mul(warm, wet);
	// steep ground holds little soil; bare peaks none
	density = mul(density, ONE - ramp(out.slope, ratio(8, 10), ratio(16, 10)));
	if (c.elevation > ratio(55, 100))
		density = 0;
	if (g.province[i] == Province::Volcanic && density > 0) // ash makes rich soil
		density = std::min<fix>(ONE, density + ratio(10, 100));
	// woods and clearings
	fix woods = noise(at, m_code.seed_hash + WOODS, std::max(8, m_code.size / 160));
	density = clamp(mul(density, ratio(70, 100) + mul(woods, ratio(60, 100))), 0, ONE);
	out.density = density;

	// a tree here? one jittered spot per small cell, kept if the roll is under
	// the density. The cells are a square lattice on the plane of the cube face
	// this place looks out through (its surface block's other two coordinates),
	// with the face's axis in the hash so the faces' lattices differ
	const int axis = std::abs(at.x) >= std::abs(at.y) && std::abs(at.x) >= std::abs(at.z) ? 0
			: std::abs(at.y) >= std::abs(at.z) ? 1 : 2;
	const int32_t x = p[(axis + 1) % 3], z = p[(axis + 2) % 3];
	int32_t sx = (x >= 0 ? x : x - SPOT_CELL + 1) / SPOT_CELL, sz = (z >= 0 ? z : z - SPOT_CELL + 1) / SPOT_CELL;
	uint32_t h = mix((uint32_t)sx, (uint32_t)sz, m_code.seed_hash + SPOT + (uint32_t)(axis * 2 + (p[axis] < 0)));
	int32_t px = sx * SPOT_CELL + (int32_t)(h % SPOT_CELL), pz = sz * SPOT_CELL + (int32_t)((h >> 8) % SPOT_CELL);
	out.tree = px == x && pz == z && (fix)((h >> 12) & (ONE - 1)) < mul(density, ratio(85, 100));
	return out;
}

} // namespace terragen
