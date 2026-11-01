// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "noise.h"

namespace terragen {

static uint32_t hash(int32_t x, int32_t y, uint32_t seed)
{
	uint32_t h = seed ^ ((uint32_t)x * 0x27d4eb2du) ^ ((uint32_t)y * 0x165667b1u);
	h = (h ^ (h >> 15)) * 0x2c1b3c6du;
	h = (h ^ (h >> 12)) * 0x297a2d39u;
	return h ^ (h >> 15);
}

static uint32_t hash3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
	return hash(x, y, seed ^ ((uint32_t)z * 0x9e3779b1u));
}

// A lattice value in [−1, 1)
static fix lattice3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
	return (fix)((hash3(x, y, z, seed) >> 15) & 0x1FFFF) - ONE;
}

// Trilinear between lattice points, smoothed: at (ix + tx, iy + ty, iz + tz).
static fix trilinear(int32_t ix, int32_t iy, int32_t iz, fix tx, fix ty, fix tz, uint32_t seed)
{
	tx = smooth(tx);
	ty = smooth(ty);
	tz = smooth(tz);
	auto plane = [&](int32_t yy) {
		fix a = lerp(lattice3(ix, yy, iz, seed), lattice3(ix + 1, yy, iz, seed), tx);
		fix b = lerp(lattice3(ix, yy, iz + 1, seed), lattice3(ix + 1, yy, iz + 1, seed), tx);
		return lerp(a, b, tz);
	};
	return lerp(plane(iy), plane(iy + 1), ty);
}

fix noise(const Dir &d, uint32_t seed, int32_t frequency)
{
	// cells round the equator → cells per unit of radius
	const int64_t k = ((int64_t)frequency << (2 * FRAC)) / TWO_PI; // fixed, in 64 bits
	// (c · k) >> FRAC without the product overflowing 64 bits on a large
	// planet's fine noise (k ~ cells round × 2³², c up to 2¹⁶): k's whole and
	// fractional halves apart, exactly the same bits
	const int64_t whole = k >> FRAC, part = k & (ONE - 1);
	const auto scaled = [&](fix c) { return c * whole + ((c * part) >> FRAC); };
	const int64_t x = scaled(d.x), y = scaled(d.y), z = scaled(d.z);
	return trilinear((int32_t)(x >> FRAC), (int32_t)(y >> FRAC), (int32_t)(z >> FRAC),
			(fix)(x & (ONE - 1)), (fix)(y & (ONE - 1)), (fix)(z & (ONE - 1)), seed);
}

fix noise(const Place &p, uint32_t seed, int32_t frequency)
{
	// cells per unit of radius (fixed, 64 bits) times the place's components
	// (30 bits): lattice coordinates in fixed, past 64 bits on the way
	const int64_t k = ((int64_t)frequency << (2 * FRAC)) / TWO_PI;
	const int64_t x = mulShift(p.x, k, PLACE_FRAC), y = mulShift(p.y, k, PLACE_FRAC), z = mulShift(p.z, k, PLACE_FRAC);
	return trilinear((int32_t)(x >> FRAC), (int32_t)(y >> FRAC), (int32_t)(z >> FRAC),
			(fix)(x & (ONE - 1)), (fix)(y & (ONE - 1)), (fix)(z & (ONE - 1)), seed);
}

fix fbm(const Place &p, uint32_t seed, int32_t frequency, int octaves, fix persistence)
{
	fix sum = 0, amp = ONE, norm = 0;
	for (int o = 0; o < octaves; o++) {
		sum += mul(amp, noise(p, seed + (uint32_t)o * 1013u, frequency));
		norm += amp;
		amp = mul(amp, persistence);
		frequency *= 2;
	}
	return div(sum, norm);
}

fix fbm(const Dir &d, uint32_t seed, int32_t frequency, int octaves, fix persistence)
{
	fix sum = 0, amp = ONE, norm = 0;
	for (int o = 0; o < octaves; o++) {
		sum += mul(amp, noise(d, seed + (uint32_t)o * 1013u, frequency));
		norm += amp;
		amp = mul(amp, persistence);
		frequency *= 2;
	}
	return div(sum, norm);
}

// floor division, and the fraction as a fixed value
static int32_t cellOf(int32_t v, int32_t cell, fix &t)
{
	int32_t i = v >= 0 ? v / cell : -((-v + cell - 1) / cell);
	t = (fix)(((int64_t)(v - i * cell) << FRAC) / cell);
	return i;
}

fix value3(int32_t x, int32_t y, int32_t z, uint32_t seed, int32_t cell)
{
	fix tx, ty, tz;
	int32_t ix = cellOf(x, cell, tx), iy = cellOf(y, cell, ty), iz = cellOf(z, cell, tz);
	return trilinear(ix, iy, iz, tx, ty, tz, seed);
}

} // namespace terragen
