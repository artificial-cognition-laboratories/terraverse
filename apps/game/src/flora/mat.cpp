// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "grow.h"

/*
	The mat generator: a cover, not a plant. A patch of ground in sixteenths,
	each column as high as the cover stands there: smooth swelling cushions
	(moss), or separate blades of uneven height with the odd seed head over
	them (turf). Thinner toward its edge.
*/
namespace flora {

namespace {

const int32_t SWELL = 7;        // sixteenths across a cushion's swell
const fix HOLLOW = R(35);       // a cushion's hollows, of its swells' height
const fix EDGE = R(30);         // the outer share of a patch, where it thins out
const fix BLADES = R(55);       // of a turf's columns, at its closest
const fix SHORT = R(35);        // its shortest blades, of its tallest
const uint32_t HEADS = 48;      // a seed head to this many blades
const int32_t HEADS_FROM = 6;   // sixteenths: shorter turf doesn't seed
const fix CRUST_UNDER = R(150); // a cover standing lower than this (sixteenths) is a crust: one deep, in splotches
const fix SPLOTCH = R(45);      // of the ground under a crust, bare

uint32_t roll(int32_t x, int32_t z, uint32_t seed) { return hash3(x, 0, z, seed) >> 16; } // 0..65535: a fix under 1

// Smooth 0..1 over the patch.
fix swell(int32_t x, int32_t z, uint32_t seed)
{
	const int32_t ix = x >= 0 ? x / SWELL : -((-x + SWELL - 1) / SWELL), iz = z >= 0 ? z / SWELL : -((-z + SWELL - 1) / SWELL);
	const fix tx = smooth(ratio(x - ix * SWELL, SWELL)), tz = smooth(ratio(z - iz * SWELL, SWELL));
	const auto at = [&](int32_t cx, int32_t cz) { return (fix)roll(cx, cz, seed ^ 0x5bd1e995u); };
	return lerp(lerp(at(ix, iz), at(ix + 1, iz), tx), lerp(at(ix, iz + 1), at(ix + 1, iz + 1), tx), tz);
}

} // namespace

Plant growMat(const Traits &t, uint32_t seed)
{
	Canvas canvas(t.grain);
	const int32_t radius = std::max(2, roundInt(t.patch * t.grain));
	const fix tall = maxx(t.gene(Gene::Height) * t.grain, ONE);
	const bool blades = t.cover.tufted > HALF, crust = !blades && tall < CRUST_UNDER;
	const uint32_t flowers = (uint32_t)clamp(t.cover.flowers, 0, ONE - 1);
	for (int32_t z = -radius; z <= radius; z++)
		for (int32_t x = -radius; x <= radius; x++) {
			const fix out = div((fix)isqrt((uint64_t)((int64_t)toFix(x) * toFix(x) + (int64_t)toFix(z) * toFix(z))), toFix(radius));
			if (out >= ONE)
				continue;
			const fix thins = std::min(ONE, div(ONE - out, EDGE)); // 1 inside, to 0 at the edge
			if (blades) {
				if ((fix)roll(x, z, seed) >= mul(mul(BLADES, mul(R(90), t.leafing)), thins))
					continue;
				const int32_t high = std::max(1, roundInt(mul(tall, lerp(SHORT, ONE, (fix)roll(x, z, seed ^ 0x9e3779b9u)))));
				const bool head = roundInt(tall) >= HEADS_FROM && hash3(x, 1, z, seed) % HEADS == 0;
				for (int32_t y = 0; y < high + (head ? high / 3 : 0); y++)
					canvas.set(x, y, z, Part::Leaf);
				if (head)
					canvas.set(x, high + high / 3, z, Part::Flower);
				else if (roll(x, z, seed ^ 0x7f4a7c15u) < flowers) // a meadow's
					canvas.set(x, high, z, Part::Flower);
			} else if (crust) {
				if (swell(x, z, seed) > SPLOTCH && (fix)roll(x, z, seed) < thins)
					canvas.set(x, 0, z, Part::Leaf);
			} else {
				const int32_t high = roundInt(mul(mul(tall, lerp(HOLLOW, ONE, swell(x, z, seed))), thins));
				for (int32_t y = 0; y < high; y++)
					canvas.set(x, y, z, Part::Leaf);
			}
		}
	return canvas.finish();
}

} // namespace flora
