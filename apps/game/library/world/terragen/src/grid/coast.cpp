// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/coast.h"
#include "library/world/terragen/src/base/noise.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/grid/past.h"
#include "library/world/terragen/src/grid/plates.h"
#include "library/world/terragen/src/grid/setting.h"
#include "library/world/terragen/src/base/parallel.h"
#include <algorithm>

namespace terragen::coast {

using namespace plates;

namespace {

// Noise stream for the rock's strength along the coasts (grid.h and plates.h have the others)
const uint32_t ROCK = 569;
// Rounds of the waves' wearing: each can take a ring of cells
const int ROUNDS = 40;
// The waves' strength on an open, straight coast, against rock from 0 (the
// weakest) to 1: from the least (the Coast setting at 0) to the most
const fix LEAST = ratio(20, 100), MOST = ratio(80, 100);
// Of a cell's strength: how much is its own (beds and joints, from cell to
// cell), how much its terrane's (a whole old piece of crust stronger or
// weaker than the next: the rest), and how much the lie of the rock over
// several cells
const fix OWN = ratio(30, 100), TERRANE = ratio(40, 100);
// How much a cell's height adds to its strength: this high (of 5 km) stands
// twice what a cell at the sea does (a low plain drowns, a cliff coast stands)
const fix HIGH = ratio(8, 100);
// Land lower than this (of 5 km) the sea comes in over, where its setting
// lets it all the way (SettingRow::sea) and the sea reaches it over more
const fix FLOOD = ratio(4, 100);
// What the sea leaves where it took the land: a shelf this deep
const fix SHELF = ratio(6, 160);
// The least land or sea a cell is (as the grid's base keeps)
const fix SHORE = ratio(1, 160);

// Cells out that a coast's shelter is judged over (a bay a few hundred km
// across shelters its head)
const int32_t SHELTER = 4;

// How open to the sea each coastal land cell is: the share of the cells
// within SHELTER that are sea, against a straight coast's half (so 1 on
// one, under 1 in a bay, over on a headland)
fix openness(const std::vector<fix> &h, int32_t i);
// The heights with the sea's level moved so `area` of the planet is above
// it, exactly: the cells above are the highest by rank, so a run of equal
// heights at the shore is split, not taken whole
std::vector<fix> levelled(const std::vector<fix> &h, int64_t area);

} // namespace

std::vector<fix> cut(const WorldCode &c, const Grid &g)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	std::vector<fix> h = g.base;
	if (!c.processes.water || c.processes.regime == Regime::Lid)
		return h;
	const std::vector<Setting> made = settings(c, g, h);
	const uint32_t seed = c.seed_hash + ROCK;
	const past::Terranes pieces = past::terranes(c);
	// each land cell's strength against the waves
	std::vector<fix> strength(n, 0);
	int64_t land = 0;
	for (int32_t i = 0; i < n; i++) {
		if (h[i] < 0)
			continue;
		land += sphere.area(i);
		const fix own = unit(mix((uint32_t)i, 1, seed));
		const fix lie = clamp(fbm(sphere.centre(i), seed + 1, c.continents * 6, 3, HALF) * 2 + HALF, 0, ONE);
		const fix piece = unit(mix(pieces.of[i], 2, seed));
		const fix rock = div(mul(own, OWN) + mul(piece, TERRANE) + mul(lie, ONE - OWN - TERRANE), SETTINGS[(int)made[i]].hard);
		strength[i] = mul(rock, ONE + div(h[i], HIGH));
	}
	// The sea in over low sinking ground it reaches (a foreland's trough, a
	// sag, a rift's floor): gulfs and inland seas
	std::vector<int32_t> open;
	for (int32_t i = 0; i < n; i++)
		if (h[i] < 0)
			open.push_back(i);
	for (size_t k = 0; k < open.size(); k++)
		for (const int32_t *p = sphere.neighboursBegin(open[k]); p != sphere.neighboursEnd(open[k]); p++)
			if (h[*p] >= 0 && h[*p] < mul(FLOOD, SETTINGS[(int)made[*p]].sea)) {
				h[*p] = std::min(h[*p] - mul(FLOOD, SETTINGS[(int)made[*p]].sea), -SHORE);
				open.push_back(*p);
			}
	// The waves on what's left
	const fix waves = LEAST + mul(MOST - LEAST, c.coast);
	for (int round = 0; round < ROUNDS; round++) {
		std::vector<int32_t> taken;
		for (int32_t i = 0; i < n; i++) {
			if (h[i] < 0)
				continue;
			bool coast = false;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i) && !coast; p++)
				coast = h[*p] < 0;
			if (coast && strength[i] < mul(waves, openness(h, i)))
				taken.push_back(i);
		}
		if (taken.empty())
			break;
		for (int32_t i : taken)
			h[i] = -SHELF;
	}
	// (the sea's level moves to keep the land's share: what the waves took
	// comes back as a little more coastal plain round the rest)
	return levelled(h, land);
}

namespace {

fix openness(const std::vector<fix> &h, int32_t i)
{
	const Sphere &sphere = gridSphere();
	// within SHELTER cells: dot ≥ cos(SHELTER · spacing)
	const int64_t near = (int64_t)cos(SHELTER * sphere.spacing()) * ONE;
	const Dir &at = sphere.centre(i);
	int64_t sea = 0, all = 0;
	for (int32_t k : sphere.around(i, [&](int32_t k) { return dot(sphere.centre(k), at) >= near; })) {
		all += sphere.area(k);
		if (h[k] < 0)
			sea += sphere.area(k);
	}
	return (fix)(sea * 2 * ONE / all);
}

std::vector<fix> levelled(const std::vector<fix> &h, int64_t area)
{
	const Sphere &sphere = gridSphere();
	const std::vector<int32_t> by = lowestFirst(h);
	const int64_t below = (int64_t)h.size() * ONE - area;
	int64_t under = 0;
	size_t k = 0;
	while (k + 1 < h.size() && under + sphere.area(by[k]) <= below)
		under += sphere.area(by[k++]);
	const fix level = h[by[k]];
	std::vector<fix> out(h.size());
	for (size_t r = 0; r < by.size(); r++) {
		const fix v = h[by[r]] - level;
		out[by[r]] = r >= k ? std::max(v, SHORE) : std::min(v, -SHORE);
	}
	return out;
}

} // namespace

// Waves on the coasts (sprint 112): a wave's height grows with the fetch,
// the open water the wind blows over to reach the shore. A coast cell looks
// upwind FETCH steps of a cell's spacing; each step over the sea adds to
// its waves. Swell from far storms comes from every side: SWELL of a coast's
// waves come from how open it is (its neighbours' sea), a bay's least. A
// wind half the sea's median or less raises small waves.
const int32_t FETCH = 8;
const fix SWELL = ratio(35, 100);

fix windAt(const Grid &g, int32_t i)
{
	return sqrt(mul(g.wind[0][i], g.wind[0][i]) + mul(g.wind[1][i], g.wind[1][i]) + mul(g.wind[2][i], g.wind[2][i]));
}

std::vector<fix> surf(const WorldCode &code, const Grid &g)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = (int32_t)g.base.size();
	std::vector<fix> out(n, 0);
	if (!code.processes.water)
		return out;
	std::vector<fix> strong;
	for (int32_t i = 0; i < n; i++)
		if (g.base[i] < 0)
			strong.push_back(windAt(g, i));
	if (strong.empty())
		return out;
	std::nth_element(strong.begin(), strong.begin() + strong.size() / 2, strong.end());
	const fix median = std::max<fix>(1, strong[strong.size() / 2]);
	parallelFor(n, [&](int32_t i) {
		if (g.base[i] < 0)
			return;
		int32_t sea = 0, around = 0;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, around++)
			sea += g.base[*p] < 0;
		if (sea == 0)
			return;
		const fix w = windAt(g, i);
		int32_t open = 0;
		if (w > 0) {
			// upwind: against the wind, a spacing a step
			const Dir &c = sphere.centre(i);
			const int64_t step = sphere.spacing();
			for (int32_t k = 1; k <= FETCH; k++) {
				const int64_t back = step * k;
				const Dir at = normalise((int64_t)c.x - (int64_t)div(g.wind[0][i], w) * back / ONE,
						(int64_t)c.y - (int64_t)div(g.wind[1][i], w) * back / ONE,
						(int64_t)c.z - (int64_t)div(g.wind[2][i], w) * back / ONE);
				if (g.base[sphere.cell(at)] < 0)
					open++;
			}
		}
		const fix strength = clamp(div(w, median) - HALF, 0, ONE);
		const fix fetch = mul((fix)(open * ONE / FETCH), strength);
		out[i] = mul(ONE - SWELL, fetch) + mul(SWELL, (fix)(sea * ONE / around));
	});
	return out;
}

} // namespace terragen::coast
