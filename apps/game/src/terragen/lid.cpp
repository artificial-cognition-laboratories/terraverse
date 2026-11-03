// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lid.h"
#include "grid.h"
#include "noise.h"
#include "plates.h"
#include <algorithm>

namespace terragen {

using plates::mix;
using plates::scattered;
using plates::unit;

namespace {

enum LidStream : uint32_t { RISE = 331, RIFT = 337, MASSIF = 347 };

// Elevation is 5 km to the unit. The thin side lies 5 km under the thick (Mars's
// dichotomy), the rise stands 6 km over its land, ~40° wide; canyons run 7 km deep
const fix DICHOTOMY = ONE, RISE_UP = ratio(12, 10), RISE_REACH = ratio(70, 100), CANYON_DEEP = ratio(14, 10);

fix dotOf(const Dir &a, const Dir &b) { return mul(a.x, b.x) + mul(a.y, b.y) + mul(a.z, b.z); }

Dir crossOf(const Dir &a, const Dir &b)
{
	return normalise((int64_t)mul(a.y, b.z) - mul(a.z, b.y), (int64_t)mul(a.z, b.x) - mul(a.x, b.z), (int64_t)mul(a.x, b.y) - mul(a.y, b.x));
}

void smoothOver(std::vector<fix> &f, int passes)
{
	const Sphere &sphere = gridSphere();
	for (int pass = 0; pass < passes; pass++) {
		const std::vector<fix> was = f;
		for (int32_t i = 0; i < sphere.size(); i++) {
			int64_t sum = was[i];
			int32_t count = 1;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, count++)
				sum += was[*p];
			f[i] = (fix)(sum / count);
		}
	}
}

void plumes(const WorldCode &code, Grid &g)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	const uint32_t seed = code.seed_hash;
	// the rise: on the dichotomy's edge, where the crust thins
	int32_t at = 0;
	fix nearest = INT32_MAX;
	for (int32_t k = 0; k < n; k++) {
		const fix off = std::abs(g.thin[k] - HALF) + unit(mix(k, 1, seed + RISE)) / 64;
		if (off < nearest) {
			nearest = off;
			at = k;
		}
	}
	g.rise = sphere.centre(at);
	for (int32_t i = 0; i < n; i++) {
		g.base[i] -= mul(DICHOTOMY, smooth(clamp(div(g.thin[i] - ratio(35, 100), ratio(30, 100)), 0, ONE)));
	}
	// canyons split from the rise's flank: a long main one, and two beside it
	const Dir toward = scattered(mix(1, 1, seed + RIFT), mix(1, 2, seed + RIFT), mix(1, 3, seed + RIFT));
	const Dir pole = crossOf(g.rise, toward);
	const Dir way = crossOf(pole, g.rise);
	static const struct { fix turn, from, to, half, depth; } SPLITS[] = {
		{ 0, ratio(30, 100), ratio(150, 100), ratio(25, 1000), ONE },
		{ ratio(10, 100), ratio(35, 100), ratio(90, 100), ratio(12, 1000), ratio(70, 100) },
		{ -ratio(9, 100), ratio(32, 100), ratio(80, 100), ratio(10, 1000), ratio(60, 100) },
	};
	for (const auto &s : SPLITS) {
		const Dir w = normalise((int64_t)way.x + mul(pole.x, s.turn), (int64_t)way.y + mul(pole.y, s.turn), (int64_t)way.z + mul(pole.z, s.turn));
		g.canyons.push_back({ w, crossOf(g.rise, w), s.from, s.to, s.half, mul(CANYON_DEEP, s.depth) });
	}
}

void eruptions(const WorldCode &code, Grid &g)
{
	const Sphere &sphere = gridSphere();
	const uint32_t seed = code.seed_hash + MASSIF;
	// lava buries it all: a quarter of the relief, every scarp gone
	for (fix &h : g.base)
		h /= 4;
	smoothOver(g.base, 40);
	// the crust, forever sinking under what's piled on it, is squeezed and
	// thrust up in lone tilted blocks (Io's mountains: 5–15 km, Schenk et al.
	// 2001), most far from the vents
	for (int k = 0; k < 48; k++) {
		const uint32_t a = mix(k, 1, seed), b = mix(k, 2, seed), c = mix(k, 3, seed), d = mix(k, 4, seed);
		const Dir middle = scattered(a, b, c);
		const Dir long_way = crossOf(middle, scattered(d, a, b));
		const Dir short_way = crossOf(middle, long_way);
		const fix length = ratio(5, 100) + mul(unit(d), ratio(9, 100));
		const fix wide = mul(length, ratio(35, 100) + mul(unit(a ^ d), ratio(35, 100)));
		const fix height = ONE + 2 * unit(b ^ d);
		for (int32_t i = 0; i < sphere.size(); i++) {
			const Dir &p = sphere.centre(i);
			if (dotOf(p, middle) < ratio(95, 100))
				continue;
			const fix x = div(dotOf(p, long_way), length), y = div(dotOf(p, short_way), wide);
			// (broken outlines: blocks, not ellipses)
			const fix r = sqrt(std::min<fix>(toFix(4), mul(x, x) + mul(y, y))) + fbm(p, seed + k, 40, 3, HALF) * 3 / 4;
			if (r >= ONE)
				continue;
			const fix tilted = mul(height, ratio(65, 100) + mul(clamp(HALF + x / 2, 0, ONE), ratio(35, 100)));
			g.base[i] += mul(tilted, smooth(std::min<fix>(ONE, (ONE - r) * 3)));
			g.tectonics.belt[i] = std::max<fix>(g.tectonics.belt[i], ratio(80, 100));
		}
	}
}

} // namespace

void raiseRise(const WorldCode &code, Grid &g)
{
	if (code.processes.plumes <= 0)
		return;
	const Sphere &sphere = gridSphere();
	const fix edge = cos(RISE_REACH);
	for (int32_t i = 0; i < sphere.size(); i++) {
		const fix up = mul(RISE_UP, smooth(clamp(div(dotOf(sphere.centre(i), g.rise) - edge, ONE - edge), 0, ONE)));
		g.base[i] += up;
		g.mare_level[i] += up; // (its lava lifted with it)
	}
}

void shapeLid(const WorldCode &code, Grid &g)
{
	if (code.processes.plumes > 0)
		plumes(code, g);
	if (code.processes.eruptions > 0)
		eruptions(code, g);
}

} // namespace terragen
