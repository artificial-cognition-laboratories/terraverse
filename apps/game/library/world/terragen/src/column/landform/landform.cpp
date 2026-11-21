// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/column/landform/landform.h"
#include "library/world/terragen/src/base/noise.h"
#include "library/world/terragen/src/grid/climate.h"
#include "library/world/terragen/src/grid/grid.h"
#include <algorithm>

namespace terragen {

namespace {

// Noise streams: each landform's from FORMS + 8 · its row (grid.h has the others)
const uint32_t FORMS = 571;
// The limestone platforms' patches (as World::karst's: the KARST stream)
const int32_t KARST_WAVES = 20;
// How large a landform's patches are: this many to a cell's noise wave
const int32_t PATCH = 2;
// A scarp's ridges: bands to its beds' wave, and the share of each that's the dip slope
const int32_t SCARP_BANDS = 3;
const fix DIP = ratio(85, 100);
// A mesa's cliff: how much of its noise's swing it rises over
const fix CLIFF = ratio(5, 100);

// Each shape's profile at a place, 0..1 (waves: its noise's round the planet)
using Profile = fix (*)(const Place &at, uint32_t seed, int32_t waves, fix share);
fix dome(const Place &at, uint32_t seed, int32_t waves, fix share);
fix cone(const Place &at, uint32_t seed, int32_t waves, fix share);
fix tower(const Place &at, uint32_t seed, int32_t waves, fix share);
fix table(const Place &at, uint32_t seed, int32_t waves, fix share);
fix scarp(const Place &at, uint32_t seed, int32_t waves, fix share);
fix ridged(const Place &at, uint32_t seed, int32_t waves, fix share);
const Profile PROFILES[(int)Shape::Count] = { dome, cone, tower, table, scarp, ridged };

// How far x is within a band, eased over an eighth of its width at each end
fix within(fix x, const Band &b);

} // namespace

Landforms::Landforms(Opts o) :
	m_grid(o.grid),
	m_seed(o.code.seed_hash),
	m_radius(o.radius),
	m_meander(o.meander)
{}

int64_t Landforms::height(const Site &s) const
{
	int64_t sum = 0;
	for (int k = 0; k < (int)Landform::Count; k++) {
		const LandformRow &row = LANDFORMS[k];
		// (none finer than the far view's tiles can show)
		if (s.tile * 2 >= toFix(row.wave))
			continue;
		const fix where = strength((Landform)k, s);
		if (where <= 0)
			continue;
		const int32_t waves = (int32_t)std::min<int64_t>(INT32_MAX / 4, m_radius * 6283 / 1000 / row.wave);
		const fix shape = PROFILES[(int)row.shape](s.at, m_seed + FORMS + 8 * (uint32_t)k, waves, row.share);
		sum += (int64_t)mul(mul(where, shape), toFix(row.high));
	}
	return sum;
}

fix Landforms::strength(Landform form, const Site &s) const
{
	const LandformRow &row = LANDFORMS[(int)form];
	const Sphere::Spot spot = gridSphere().spot(s.at.dir());
	// the share of the place in cells of its settings, and their climate
	fix where = 0, rain = 0, heat = 0;
	for (int k = 0; k < 3; k++) {
		const int32_t i = spot.cells[k];
		if (row.settings >> (int)m_grid.setting[i] & 1)
			where += spot.weights[k];
		rain += mul(m_grid.rain(i), spot.weights[k]);
		heat += mul(m_grid.heat[i], spot.weights[k]);
	}
	if (where <= 0)
		return 0;
	const fix celsius = celsiusOf(heat - mul(std::max<fix>(0, s.elevation), LAPSE));
	where = mul(mul(mul(where, within(s.hard, row.hard)), within(rain, row.rain)), within(celsius, row.celsius));
	if (where <= 0)
		return 0;
	if (row.limestone)
		where = mul(where, smooth(clamp(fbm(s.at, m_seed + KARST, KARST_WAVES, 2, HALF) * 4 - HALF, 0, ONE)));
	// in patches, not over all of its ground
	const uint32_t seed = m_seed + FORMS + 8 * (uint32_t)form + 1;
	return mul(where, clamp(HALF + fbm(s.at, seed, std::max(1, m_meander / PATCH), 2, HALF) * 3, 0, ONE));
}

namespace {

// (a value noise's peaks: where it's over a level that leaves about `share`
// of the ground above it, how far over, 0..1)
fix peak(const Place &at, uint32_t seed, int32_t waves, fix share)
{
	const fix level = std::max<fix>(0, ONE - share * 4);
	const fix n = noise(at, seed, waves);
	return n <= level ? 0 : div(n - level, ONE - level);
}

fix dome(const Place &at, uint32_t seed, int32_t waves, fix share)
{
	// steep at its foot, rounded over its top: 1 − (1 − x)³
	const fix x = ONE - peak(at, seed, waves, share);
	return ONE - mul(mul(x, x), x);
}

fix cone(const Place &at, uint32_t seed, int32_t waves, fix share)
{
	return peak(at, seed, waves, share);
}

fix tower(const Place &at, uint32_t seed, int32_t waves, fix share)
{
	// sheer walls: all of its height within its first third
	return smooth(std::min<fix>(ONE, peak(at, seed, waves, share) * 3));
}

fix table(const Place &at, uint32_t seed, int32_t waves, fix share)
{
	// a hard bed's top where a broad noise is over its level, the cliff round
	// it over a narrow swing of it
	const fix level = HALF - share;
	const fix f = fbm(at, seed, waves, 3, HALF) * 2;
	return smooth(clamp(div(f - level, CLIFF), 0, ONE));
}

fix scarp(const Place &at, uint32_t seed, int32_t waves, fix share)
{
	// contours of the beds, a band to each ridge: up the dip to the crest,
	// then down the scarp (a broad noise's, so the ridges wander and bend)
	const fix beds = fbm(at, seed, std::max(1, waves / SCARP_BANDS), 2, HALF);
	const fix u = (fix)((beds * SCARP_BANDS * 4) & (ONE - 1));
	const fix up = u < DIP ? smooth(div(u, DIP)) : ONE - div(u - DIP, ONE - DIP);
	return mul(up, std::min<fix>(ONE, share * 2));
}

fix ridged(const Place &at, uint32_t seed, int32_t waves, fix share)
{
	// knife-edged where a noise crosses zero, gullied between
	const fix r = clamp(ONE - std::abs(fbm(at, seed, waves, 3, HALF)) * 3, 0, ONE);
	return mul(mul(mul(r, r), r), std::min<fix>(ONE, share * 2));
}

fix within(fix x, const Band &b)
{
	const fix edge = std::max<fix>(1, (b.to - b.from) / 8);
	return std::min(clamp(div(x - b.from, edge), 0, ONE), clamp(div(b.to - x, edge), 0, ONE));
}

} // namespace

} // namespace terragen
