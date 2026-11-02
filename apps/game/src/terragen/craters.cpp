// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "craters.h"
#include "plates.h"
#include <algorithm>
#include <stdexcept>

namespace terragen {

using plates::mix;
using plates::unit;

namespace {

const uint32_t IMPACT = 211; // the craters' hashes, apart from every other stream

// Each class covers this share of the surface (the classes' craters are
// [D, 2D) across): near the saturation of old highlands, where a new crater
// wipes out as many as it makes
const fix COVER = ratio(15, 100);
// Diameters (metres) of the bowl-to-complex transition at Earth's pull:
// ~3 km on Earth, ~18 km on the Moon (it goes as 1/g)
const int32_t TRANSITION_M = 3000;
// Water wears craters smaller than this away (metres) before they're old
const int32_t WORN_SMALL_M = 2000;
// The smallest crater a column reads, in blocks
const int32_t SMALLEST_BLOCKS = 8;

uint32_t hash4(int32_t i, int32_t j, int32_t l, uint32_t seed)
{
	return mix(mix((uint32_t)i, (uint32_t)j, seed), (uint32_t)l, seed ^ 0x9e3779b9u);
}

int32_t floorDiv(int64_t v, int64_t d)
{
	return (int32_t)(v >= 0 ? v / d : -((-v + d - 1) / d));
}

} // namespace

Craters::Craters(const WorldCode &code, int64_t radius, fix blocksPerMetre) :
	m_seed(code.seed_hash + IMPACT), m_radius(radius), m_scale(blocksPerMetre), m_water(code.processes.water),
	m_young((uint32_t)std::min<int64_t>(65536, ((int64_t)code.processes.young * 65536) >> FRAC))
{
	const Processes &p = code.processes;
	if (p.gravity <= 0)
		throw std::invalid_argument("terragen::Craters: a body without gravity");
	// blocks (fixed) → place units
	const auto place = [&](int64_t blocks) { return (int64_t)(((__int128)blocks << PLACE_FRAC) / radius); };
	m_simple = place(div(toFix(TRANSITION_M), p.gravity) * (int64_t)blocksPerMetre >> FRAC);
	if (p.craters <= 0)
		return;
	// the smallest: a few blocks, what the air lets through, and on a wet
	// body what lasts (blocks, fixed)
	int64_t least = toFix(SMALLEST_BLOCKS);
	least = std::max<int64_t>(least, (int64_t)p.smallest_crater * blocksPerMetre);
	if (p.water)
		least = std::max<int64_t>(least, (int64_t)WORN_SMALL_M * blocksPerMetre);
	// a class's craters are [D, 2D) across in cells 2D wide: a cell holds
	// one with chance COVER / (π D² / 4) · (2D)² = COVER · 16 / π
	const uint32_t keep = (uint32_t)std::min<int64_t>(65536, ((int64_t)mul(mul(COVER, p.craters), div(toFix(16), PI)) * 65536) >> FRAC);
	for (int64_t d = radius / 4; d >= least && keep > 0; d /= 2)
		m_classes.push_back({ place(2 * d), place(d), (2 * d) >> FRAC, keep });
}

bool Craters::candidate(int k, int32_t i, int32_t j, int32_t l, Crater &out) const
{
	const Class &c = m_classes[k];
	const uint32_t h = hash4(i, j, l, m_seed + (uint32_t)k * 7919u);
	if ((h & 0xFFFF) >= c.keep)
		return false;
	const uint32_t g = mix(h, 1, m_seed), a = mix(h, 2, m_seed), b = mix(h, 3, m_seed);
	const int64_t x = ((int64_t)i << 16 | (g & 0xFFFF)) * c.lattice >> 16;
	const int64_t y = ((int64_t)j << 16 | (g >> 16)) * c.lattice >> 16;
	const int64_t z = ((int64_t)l << 16 | (a & 0xFFFF)) * c.lattice >> 16;
	// only in a shell a cell thick round the surface, so they lie as evenly
	// as the lattice: a cell's chance per area is the same everywhere
	const __int128 r2 = (__int128)x * x + (__int128)y * y + (__int128)z * z;
	const __int128 inner = PLACE_ONE - c.lattice / 2, outer = PLACE_ONE + c.lattice / 2;
	if (r2 < inner * inner || r2 >= outer * outer)
		return false;
	out.at = placeOf(x, y, z);
	// across: [D, 2D) with counts falling as D⁻² (D / √(1 − ¾u))
	const fix spread = sqrt(ONE - mul(ratio(3, 4), unit(a >> 16 | b << 16)));
	const int64_t across = (c.smallest << FRAC) / spread;
	out.radius = across / 2;
	const fix across_blocks = (fix)std::min<int64_t>(INT32_MAX, mulShift(across, m_radius, PLACE_FRAC));
	// worn with age: depth and rim down to a quarter (a wet body's, to an
	// eighth); the youngest (least worn) came after the lava flooded
	out.young = (b & 0xFFFF) < m_young;
	fix wear = ONE - mul(ratio(3, 4), unit(b));
	if (m_water)
		wear /= 2;
	const fix q = (fix)std::min<int64_t>(toFix(30000), (across << FRAC) / std::max<int64_t>(1, m_simple));
	out.complex = q > ONE;
	out.basin = q > toFix(12);
	fix depth, rim;
	if (!out.complex) {
		// a bowl: a fifth as deep as it's wide
		depth = mul(across_blocks, ratio(20, 100));
		rim = mul(across_blocks, ratio(36, 1000));
	} else {
		// shallower the wider (depth ∝ D^0.3, rim ∝ D^0.4), from the transition's
		const fix transition = (fix)std::min<int64_t>(INT32_MAX, mulShift(m_simple, m_radius, PLACE_FRAC));
		depth = mul(mul(transition, ratio(20, 100)), pow(q, ratio(3, 10)));
		rim = mul(mul(transition, ratio(36, 1000)), pow(q, ratio(4, 10)));
	}
	out.depth = floorInt(mul(depth, wear));
	out.rim = floorInt(mul(rim, wear));
	// a central peak in complex craters short of basins (they ring instead)
	out.peak = out.complex && q < toFix(10) ? out.depth * 2 / 5 : 0;
	return true;
}

std::vector<Crater> Craters::all(int k) const
{
	std::vector<Crater> out;
	const int64_t L = m_classes[k].lattice;
	const int32_t n = floorDiv(PLACE_ONE + L, L);
	Crater c;
	for (int32_t i = -n - 1; i <= n; i++)
		for (int32_t j = -n - 1; j <= n; j++)
			for (int32_t l = -n - 1; l <= n; l++)
				if (candidate(k, i, j, l, c))
					out.push_back(c);
	return out;
}

fix Craters::shape(const Crater &c, const Place &at)
{
	const int64_t d = chord(at, c.at);
	if (d >= 2 * c.radius)
		return 0; // past its ejecta
	const fix x = (fix)((d << FRAC) / c.radius);
	const fix depth = toFix(c.depth), rim = toFix(c.rim);
	if (x < ONE) {
		if (!c.complex) // a bowl
			return -depth + mul(depth + rim, mul(x, x));
		// a flat floor, a central peak, walls up to the rim
		if (x < HALF) {
			const fix p = std::max<fix>(0, ONE - div(x, ratio(15, 100)));
			return -depth + mul(toFix(c.peak), mul(p, p));
		}
		return -depth + mul(depth + rim, smooth((x - HALF) * 2));
	}
	// the ejecta blanket: the rim's height falling as (r / R)⁻³, to nothing a
	// diameter out
	const fix inv = div(ONE, x);
	fix out = mul(rim, div(mul(mul(inv, inv), inv) - ONE / 8, ratio(7, 8)));
	if (c.basin) // a ring outside the rim
		out += mul(rim * 2 / 5, std::max<fix>(0, ONE - div(std::abs(x - ratio(16, 10)), ratio(1, 10))));
	return out;
}

fix Craters::height(const Place &at, int from, Age age) const
{
	fix sum = 0;
	Crater c;
	for (int k = std::max(0, from); k < (int)m_classes.size(); k++) {
		const int64_t L = m_classes[k].lattice;
		// a crater reaches a diameter (at most 2D = L) from its middle, which
		// lies up to half a cell off the surface
		const int64_t reach = L + L / 2;
		const int32_t i0 = floorDiv(at.x - reach, L), i1 = floorDiv(at.x + reach, L);
		const int32_t j0 = floorDiv(at.y - reach, L), j1 = floorDiv(at.y + reach, L);
		const int32_t l0 = floorDiv(at.z - reach, L), l1 = floorDiv(at.z + reach, L);
		for (int32_t i = i0; i <= i1; i++)
			for (int32_t j = j0; j <= j1; j++)
				for (int32_t l = l0; l <= l1; l++)
					if (candidate(k, i, j, l, c))
						sum += shape(c, at);
	}
	return sum;
}

} // namespace terragen
