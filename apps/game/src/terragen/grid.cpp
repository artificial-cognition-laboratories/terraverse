// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "grid.h"
#include "craters.h"
#include "maria.h"
#include "climate.h"
#include "drainage.h"
#include "noise.h"
#include "world.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <queue>

namespace terragen {

static int octavesFor(const WorldCode &c) { return 3 + roundInt(c.roughness * 3); }

const Sphere &gridSphere()
{
	static const Sphere sphere(GRID_FREQUENCY);
	return sphere;
}

fix landmass(const WorldCode &c, const Dir &d)
{
	fix persistence = ratio(35, 100) + mul(c.roughness, ratio(25, 100));
	// continents, with the coastline broken up by finer noise
	return fbm(d, c.seed_hash + CONTINENT, c.continents, octavesFor(c), persistence)
			+ mul(fbm(d, c.seed_hash + COAST, c.continents * 7, 3, HALF), mul(c.coast, ratio(22, 100)));
}

namespace {

// Erosion: how hard running water bites, and how many rounds it gets.
const fix ERODIBILITY = ratio(6, 100);
const int EROSION_ROUNDS = 12;
// How much a stream carries, as a multiple of its power to cut (F · drop):
// past that, it lays its load down.
const fix CARRY = toFix(2);
// Heavy grains (gold, tin) settle this many times as readily as the rest of
// a river's load: they gather where it first slows, below the mountains.
const int64_t HEAVY = 2;
// Each round, where the plates collide the land is pushed up by this much
// (times the belt, 0..1), against which rivers wear it: mountains stand
// where the two balance, not as the noise made them.
const fix UPLIFT = ratio(1, 200);
// Hillslopes creep: each round a land cell trades this share of its height
// difference with each land neighbour (κ ∇²h, explicit; stable below 1/6),
// which rounds ridges and fills the cells' sharpest notches.
const fix CREEP = ratio(1, 50);
// Water deeper than a block (at 160 blocks per unit) counts as a lake…
const fix LAKE_DEPTH = ratio(1, 160);
// Glacial scour (see build): how deep, and below what yearly mean (°C).
const fix SCOUR = ratio(24, 160);
const fix SCOUR_BELOW = toFix(4);
// …if its basin is big and deep enough; smaller hollows fill with sediment,
// and so do basins wider than this share of the land (they'd be inland seas).
// (On the sphere's cells, about two thirds of the flat map's in area, hollows
// come out shallower: 15 cells and 4 blocks deep keep about as many lakes.)
const int32_t LAKE_MIN_CELLS = 15;
const fix LAKE_MIN_DEEPEST = ratio(4, 160);
const int32_t TARN_MIN_CELLS = 6;
const fix TARN_MIN_DEEPEST = ratio(8, 160);
const fix LAKE_MAX_SHARE = ratio(3, 100);
// Steepness toward a neighbour is drop ÷ distance; compared as drop · SPAN ÷
// distance (fixed, unit sphere) so a one-unit drop never rounds away.
const int64_t SPAN = (int64_t)1 << 24;

} // namespace

fix jitter(int32_t i)
{
	uint32_t h = (uint32_t)i * 0x9e3779b1u;
	h ^= h >> 15;
	h *= 0x85ebca77u;
	return (fix)((h >> 16) & 63);
}

/*
	Priority-flood (Barnes, Lehman & Mulla 2014), with an epsilon so every
	land cell ends strictly above the cell that reached it: water can always
	run downhill to the sea. Seeds are the sea cells; with no sea at all, the
	lowest cell. Ties pop by index, so the result is deterministic.
*/
std::vector<fix> fill(const std::vector<fix> &h)
{
	const Sphere &sphere = gridSphere();
	using Entry = std::pair<fix, int32_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
	std::vector<fix> out = h;
	std::vector<uint8_t> seen(h.size(), 0);

	int32_t lowest = -1;
	for (int32_t i = 0; i < (int32_t)h.size(); i++) {
		if (lowest < 0 || h[i] < h[lowest])
			lowest = i;
		if (h[i] < 0) {
			seen[i] = 1;
			open.push({ h[i], i });
		}
	}
	if (open.empty() && lowest >= 0) {
		seen[lowest] = 1;
		open.push({ h[lowest], lowest });
	}

	while (!open.empty()) {
		auto [level, c] = open.top();
		open.pop();
		for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++) {
			int32_t n = *p;
			if (seen[n])
				continue;
			seen[n] = 1;
			// the step up is jittered per cell (under a fifth of a block), so
			// water crossing a filled flat meanders instead of running ruler-straight
			out[n] = std::max(h[n], level + 1 + jitter(n));
			open.push({ out[n], n });
		}
	}
	return out;
}

// Where each cell drains: its steepest strictly-lower neighbour on `surface`;
// −1 for the sea and (on an unfilled surface) pits.
std::vector<int32_t> receivers(const std::vector<fix> &surface)
{
	const Sphere &sphere = gridSphere();
	std::vector<int32_t> out(surface.size(), -1);
	for (int32_t i = 0; i < (int32_t)surface.size(); i++) {
		if (surface[i] < 0)
			continue;
		int64_t best = 0;
		const fix *gap = sphere.distancesBegin(i);
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, gap++) {
			int64_t steep = (int64_t)(surface[i] - surface[*p]) * SPAN / *gap;
			if (steep > best) {
				best = steep;
				out[i] = *p;
			}
		}
	}
	return out;
}

// Cells ordered so every cell comes after the cell it drains into: from each
// outlet upstream, breadth first, outlets in index order.
std::vector<int32_t> upstreamOrder(const std::vector<int32_t> &receiver)
{
	size_t n = receiver.size();
	std::vector<int32_t> start(n + 1, 0), donors(n);
	for (int32_t r : receiver)
		if (r >= 0)
			start[r + 1]++;
	for (size_t i = 0; i < n; i++)
		start[i + 1] += start[i];
	std::vector<int32_t> fillpos(start.begin(), start.end() - 1);
	for (int32_t i = 0; i < (int32_t)n; i++)
		if (receiver[i] >= 0)
			donors[fillpos[receiver[i]]++] = i;

	std::vector<int32_t> order;
	order.reserve(n);
	for (int32_t i = 0; i < (int32_t)n; i++)
		if (receiver[i] < 0)
			order.push_back(i);
	for (size_t head = 0; head < order.size(); head++) {
		int32_t c = order[head];
		for (int32_t d = start[c]; d < start[c + 1]; d++)
			order.push_back(donors[d]);
	}
	return order;
}

// Rain-weighted area draining through each cell (each cell's own by its area).
std::vector<int64_t> accumulate(const std::vector<int32_t> &receiver, const std::vector<int32_t> &order,
		const std::vector<fix> &rain)
{
	const Sphere &sphere = gridSphere();
	std::vector<int64_t> area(receiver.size());
	for (size_t i = 0; i < area.size(); i++)
		area[i] = mul(rain[i], sphere.area((int32_t)i));
	for (size_t k = order.size(); k-- > 0;) {
		int32_t c = order[k];
		if (receiver[c] >= 0)
			area[receiver[c]] += area[c];
	}
	return area;
}

namespace {

/*
	Water standing over the eroded land, grouped into basins (by neighbours). A basin becomes a lake only if it is big and deep
	enough; smaller hollows fill with sediment — the land rises to the water.
*/
void keepRealLakes(Grid &g)
{
	const Sphere &sphere = gridSphere();
	size_t n = g.filled.size();
	int64_t land = 0;
	for (size_t i = 0; i < n; i++)
		land += g.base[i] >= 0;
	int32_t max_cells = (int32_t)((land * LAKE_MAX_SHARE) >> FRAC);
	std::vector<uint8_t> wet(n), seen(n);
	for (size_t i = 0; i < n; i++)
		wet[i] = g.filled[i] >= 0 && g.filled[i] - g.eroded[i] >= LAKE_DEPTH;

	std::vector<int32_t> basin;
	for (int32_t start = 0; start < (int32_t)n; start++) {
		if (!wet[start] || seen[start])
			continue;
		basin.clear();
		basin.push_back(start);
		seen[start] = 1;
		fix deepest = 0;
		for (size_t head = 0; head < basin.size(); head++) {
			int32_t c = basin[head];
			deepest = std::max(deepest, g.filled[c] - g.eroded[c]);
			for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++) {
				int32_t nb = *p;
				if (wet[nb] && !seen[nb]) {
					seen[nb] = 1;
					basin.push_back(nb);
				}
			}
		}
		// a broad basin, or a small deep one (a tarn, a scoured glacial lake)
		bool real = (int32_t)basin.size() <= max_cells &&
				(((int32_t)basin.size() >= LAKE_MIN_CELLS && deepest >= LAKE_MIN_DEEPEST) ||
						((int32_t)basin.size() >= TARN_MIN_CELLS && deepest >= TARN_MIN_DEEPEST));
		fix outlet = g.filled[start];
		for (int32_t c : basin)
			outlet = std::min(outlet, g.filled[c]);
		for (int32_t c : basin) {
			if (real) {
				g.lake[c] = 1;
				g.lake_level[c] = outlet;
			} else {
				// sediment fills the hollow (not carried in by the rivers' balance: counted apart)
				g.sediment[c] += g.filled[c] - g.eroded[c];
				g.hollows += (int64_t)(g.filled[c] - g.eroded[c]) * gridSphere().area(c);
				g.eroded[c] = g.filled[c];
			}
		}
	}
}

/*
	How fertile the ground is: what the rock under it supplies (fresh volcanic
	ash and young mountains give minerals; old shields little), what rivers
	lay down on their plains, less what warm rain washes out of it. Tropical
	rainforest soils come out poor (their life is in the plants, not the
	ground), volcanic slopes and floodplains rich.
*/
const fix SUPPLY[(int)Province::Count] = {
	ratio(20, 100), // shield
	ratio(45, 100), // orogen
	ratio(80, 100), // arc
	ratio(65, 100), // rift
	ratio(55, 100), // basin
	ONE,            // volcanic
	0,              // ocean floor
	ratio(30, 100), // highland: anorthosite, poor in what plants need
	ratio(70, 100), // mare: basalt, rich in iron and magnesium
};

std::vector<fix> fertility(const WorldCode &code, const Grid &g)
{
	const Sphere &sphere = gridSphere();
	size_t n = (size_t)sphere.size();
	std::vector<fix> out(n, 0);
	for (int32_t i = 0; i < (int32_t)n; i++) {
		fix e = g.eroded[i];
		if (e < 0)
			continue;
		fix supply = SUPPLY[(int)g.province[i]];
		// rivers spread silt over the low land they cross
		fix flood = e < ratio(12, 100) && g.river_threshold != INT64_MAX
				? mul(ratio(35, 100), (fix)std::min<int64_t>(ONE, ((g.drained[i] << FRAC) / g.river_threshold)))
				: 0;
		// and what they laid down here is fresh ground (ten blocks of it: as rich as it gets)
		flood = std::max(flood, mul(ratio(35, 100), alluvium(g.sediment[i])));
		fix heat = g.heat[i] - mul(e, LAPSE);
		// leaching grows with wetness (log₂ of the rain, like the biomes') and warmth
		fix wet = clamp(log2(maxx(g.rain(i), ratio(1, 100))) / 2, -ONE, ONE);
		fix leached = mul(mul(maxx(0, wet + ratio(5, 10)), maxx(0, heat + ratio(3, 10))), ratio(45, 100));
		out[i] = clamp(supply + flood - leached, 0, ONE);
	}
	return out;
}

std::shared_ptr<Grid> build(const WorldCode &code)
{
	const Sphere &sphere = gridSphere();
	auto g = std::make_shared<Grid>();
	size_t n = (size_t)sphere.size();
	std::vector<fix> rain(n);

	// The plates' history makes the land (history.cpp); the coasts are
	// broken up by finer noise (the Coast setting), the sea placed by
	// quantile so exactly the Land share of the planet's area is above it
	const History past = history(code);
	g->tectonics = tectonics(code, past);
	g->history = past.events;
	std::vector<fix> raw(n);
	for (size_t i = 0; i < n; i++)
		raw[i] = past.cells[i].height + mul(fbm(sphere.centre((int32_t)i), code.seed_hash + COAST, code.continents * 7, 3, HALF),
				mul(code.coast, ratio(12, 100)));
	// the ocean floor slopes down from the continents over a few cells (the
	// continental slope), not a cliff from shelf to abyss in one; and floor
	// handed over a step at a time leaves no lines where its ages meet
	for (int pass = 0; pass < 6; pass++) {
		const std::vector<fix> was = raw;
		for (size_t i = 0; i < n; i++) {
			if (past.cells[i].continental)
				continue;
			int64_t sum = was[i];
			int32_t count = 1;
			for (const int32_t *p = sphere.neighboursBegin((int32_t)i); p != sphere.neighboursEnd((int32_t)i); p++, count++)
				sum += was[*p];
			raw[i] = std::min<fix>(was[i] + ratio(10, 100), (fix)(sum / count));
		}
	}
	{
		std::vector<int32_t> by(n);
		for (size_t i = 0; i < n; i++)
			by[i] = (int32_t)i;
		std::sort(by.begin(), by.end(), [&](int32_t a, int32_t b) { return raw[a] != raw[b] ? raw[a] < raw[b] : a < b; });
		const int64_t sea = (int64_t)n * (ONE - code.land); // of the planet's n·ONE (each cell's area()), fixed
		int64_t under = 0;
		size_t k = 0;
		while (k + 1 < n && under + sphere.area(by[k]) <= sea)
			under += sphere.area(by[k++]);
		g->sea_offset = raw[by[k]];
	}
	g->base.resize(n);
	for (size_t i = 0; i < n; i++) {
		const fix land = raw[i] - g->sea_offset, relief = g->tectonics.relief[i];
		// trenches and abyssal hills shape the sea floor, never above the sea;
		// rifts sink the land, never below it
		g->base[i] = land < 0 ? std::min<fix>(land + relief + fbm(sphere.centre((int32_t)i), code.seed_hash + DETAIL, 24, 3, HALF) / 25, -ratio(1, 160))
				: std::max<fix>(land + relief, ratio(1, 160));
	}

	// Impacts: the craters wider than two cells, into the land (the rest are
	// the column's, too small for the grid to hold); the old first, then the
	// maria's lava over them, then the young on top
	{
		const Craters craters(code, World::radiusOf(code), blocksPerMetre(code));
		const int64_t cell = World::cellBlocksOf(code) >> FRAC;
		const fix scale = heightScale(code);
		g->crater_stamp.assign(n, 0);
		g->crater_young.assign(n, 0);
		std::vector<std::vector<uint32_t>> reach(n);
		while (g->crater_from < craters.classes() && craters.widest(g->crater_from) >= 2 * cell) {
			for (const Crater &c : craters.all(g->crater_from)) {
				const uint32_t k = (uint32_t)g->craters.size();
				g->craters.push_back(c);
				std::vector<fix> &stamp = c.young ? g->crater_young : g->crater_stamp;
				// the cells within its ejecta (a diameter out) and a cell more
				const int64_t within = 2 * c.radius + (PLACE_ONE >> FRAC) * sphere.spacing();
				for (int32_t i : sphere.around(sphere.cell(c.at.dir()), [&](int32_t i) {
						return chord(Place(sphere.centre(i)), c.at) <= within; })) {
					reach[i].push_back(k);
					stamp[i] += div(Craters::shape(c, Place(sphere.centre(i))), scale);
				}
			}
			g->crater_from++;
		}
		g->crater_start.assign(n + 1, 0);
		for (size_t i = 0; i < n; i++) {
			g->crater_start[i + 1] = g->crater_start[i] + (uint32_t)reach[i].size();
			g->crater_list.insert(g->crater_list.end(), reach[i].begin(), reach[i].end());
			g->base[i] += g->crater_stamp[i];
		}
		flood(code, *g);
		for (size_t i = 0; i < n; i++)
			g->base[i] += g->crater_young[i];
	}

	// The climate over the land as made (climate.h): where rain falls, which
	// wears the land and feeds the rivers (a little everywhere, so every slope runs)
	{
		Climate cl = climate(code, g->base);
		g->rain_season[0] = std::move(cl.rain[0]);
		g->rain_season[1] = std::move(cl.rain[1]);
		g->sea_heat = std::move(cl.sea);
		g->heat = std::move(cl.heat);
		g->heat_gain = std::move(cl.gain);
		g->ice = cl.ice;
		if (!code.processes.water) // nothing to rain
			for (std::vector<fix> &season : g->rain_season)
				std::fill(season.begin(), season.end(), 0);
		for (size_t i = 0; i < n; i++)
			rain[i] = clamp(g->rain((int32_t)i), ratio(15, 100), toFix(3));
	}

	// Glacial scour: ice ages' sheets ground hollows into the land that's
	// cold now (and colder then), which hold the lakes of the north (Canada's,
	// Finland's): up to SCOUR deep, in patches, where the yearly mean is
	// below SCOUR_BELOW
	for (size_t i = 0; i < n && code.processes.water; i++) {
		if (g->base[i] < ratio(2, 160))
			continue;
		const fix cold = heatOfCelsius(SCOUR_BELOW) - (g->heat[i] - mul(g->base[i], LAPSE));
		if (cold <= 0)
			continue;
		const fix patches = clamp(fbm(sphere.centre((int32_t)i), code.seed_hash + SCOURED, 30, 3, HALF) * 2, 0, ONE);
		const fix depth = mul(mul(SCOUR, patches), std::min<fix>(ONE, cold * 4));
		g->base[i] = std::max<fix>(g->base[i] - depth, ratio(1, 160));
	}

	// Erosion, implicit stream power (Braun & Willett 2013), n = 1: from each
	// outlet upstream, h ← (h + F·h_down) / (1 + F), F = K·√area. Lakes don't
	// erode their beds; the sea stays put.
	// Then what it wore away is carried down the same receivers (after Yuan et
	// al. 2019): a stream carries as much as its power allows, CARRY · F · drop,
	// and lays the rest down where it slows (floodplains, fans); lakes take what
	// reaches them, and the sea builds a shelf at the coast. Nothing is made or
	// lost but what goes out to the deep sea (Grid::sediment, testSediment).
	std::vector<fix> h = g->base;
	g->sediment.assign(n, 0);
	std::vector<int64_t> heavy_laid(n, 0); // heavy grains laid, as sediment × area
	std::vector<int64_t> crumbs(n, 0); // creep's remainders below a unit of height (× area), kept for the next round
	// (no water, no erosion: the land stays as the history left it)
	for (int round = 0; round < (code.processes.water ? EROSION_ROUNDS : 0); round++) {
		for (size_t i = 0; i < n; i++)
			if (h[i] >= 0) {
				const fix up = mul(UPLIFT, g->tectonics.belt[i]);
				h[i] += up;
				g->uplifted += (int64_t)up * sphere.area((int32_t)i);
			}
		std::vector<fix> surface = fill(h);
		std::vector<int32_t> down = receivers(surface);
		std::vector<int32_t> order = upstreamOrder(down);
		std::vector<int64_t> area = accumulate(down, order, rain);
		std::vector<fix> before = h, power(n, 0);
		for (int32_t c : order) {
			int32_t r = down[c];
			if (r < 0 || surface[c] > h[c])
				continue;
			fix F = mul(ERODIBILITY, sqrtWide64(area[c]));
			// the receiver's surface (its bed, or its lake); land never wears below the sea
			fix floor = std::max<fix>(surface[r], 0);
			fix next = div(h[c] + mul(F, floor), ONE + F);
			h[c] = std::max(next, floor + 1);
			power[c] = mul(CARRY, mul(F, h[c] - floor));
		}
		// downstream, heads first: each cell's load is its donors' plus what it
		// lost, in height × area (fixed); none is raised to its lowest donor
		// (and the heavy grains in it, worn out of the mountain belts' cores,
		// where veins and plutons are: gold, tin; they settle first)
		std::vector<int64_t> load(n, 0), heavy(n, 0);
		std::vector<fix> lowest_donor(n, INT32_MAX);
		for (size_t k = order.size(); k-- > 0;) {
			int32_t c = order[k];
			const int64_t a = sphere.area(c);
			load[c] += (int64_t)(before[c] - h[c]) * a;
			if (before[c] > h[c] && g->tectonics.belt[c] > VEIN_CORE)
				// (heavy grains are a small share of the rock: up to 1/64, deepest in the core)
				heavy[c] += ((int64_t)(before[c] - h[c]) * a >> FRAC) * div(g->tectonics.belt[c] - VEIN_CORE, ONE - VEIN_CORE) / 64;
			fix keep; // the most this cell may be raised
			if (h[c] < 0 && down[c] < 0)
				keep = -1 - h[c];                // the sea: a shelf up to just under its level
			else if (surface[c] > h[c])
				keep = (surface[c] - h[c]) / 8;  // a lake: silts up a little each round (a half compounded over the rounds filled all but the deepest)
			else
				keep = load[c] / a > power[c] ? (fix)(load[c] / a - power[c]) : 0; // past what it can carry
			keep = std::min(keep, lowest_donor[c] - 1 - h[c]);
			if (keep > 0) {
				const fix laid = (fix)std::min<int64_t>(keep, load[c] / a);
				// heavy grains drop out HEAVY times as readily as the rest
				const int64_t share = std::min<int64_t>(ONE, ((int64_t)laid * a << FRAC) / std::max<int64_t>(load[c], 1) * HEAVY);
				const int64_t dropped = mulShift(heavy[c], share, FRAC);
				h[c] += laid;
				g->sediment[c] += laid;
				heavy_laid[c] += dropped;
				load[c] -= (int64_t)laid * a;
				heavy[c] -= dropped;
			}
			int32_t r = down[c];
			if (r >= 0) {
				load[r] += load[c];
				heavy[r] += heavy[c];
				lowest_donor[r] = std::min(lowest_donor[r], h[c]);
			} else {
				g->to_deep += load[c];
			}
		}
		// creep, between land cells (the coast stays where the rivers left it)
		std::vector<int64_t> moved(crumbs);
		for (int32_t i = 0; i < (int32_t)n; i++) {
			if (h[i] < 0)
				continue;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++) {
				const int32_t k = *p;
				if (k < i || h[k] < 0)
					continue;
				const int64_t q = (int64_t)mul(CREEP, h[k] - h[i]) * std::min(sphere.area(i), sphere.area(k));
				moved[i] += q;
				moved[k] -= q;
			}
		}
		for (size_t i = 0; i < n; i++) {
			const int64_t a = sphere.area((int32_t)i);
			const int64_t whole = moved[i] >= 0 ? moved[i] / a : -((-moved[i] + a - 1) / a);
			h[i] += (fix)whole;
			crumbs[i] = moved[i] - whole * a;
		}
	}
	for (size_t i = 0; i < n; i++)
		g->to_deep += crumbs[i]; // (under a unit of height a cell: counted as gone, so the balance is exact)
	g->eroded = h;
	g->worn.resize(n);
	for (size_t i = 0; i < n; i++)
		g->worn[i] = g->base[i] >= 0 ? g->eroded[i] - g->base[i] : 0;
	// placers: the share of what was laid that's heavy grains, ×12 (0..1: rich gravel is a few percent gold and tin)
	g->placer.assign(n, 0);
	for (size_t i = 0; i < n; i++)
		// (below the source rock: within it, the grains are still in their veins)
		if (g->sediment[i] > 0 && g->tectonics.belt[i] <= VEIN_CORE)
			g->placer[i] = (fix)std::min<int64_t>(ONE, (heavy_laid[i] / sphere.area((int32_t)i) * 12 << FRAC) / g->sediment[i]);

	// The final water: fill, keep the basins that make real lakes, drain, accumulate.
	g->filled = fill(g->eroded);
	g->lake.resize(n);
	g->lake_level.resize(n);
	if (code.processes.water)
		keepRealLakes(*g);
	else
		g->filled = g->eroded; // (basins stand dry)
	g->receiver = receivers(g->filled);
	g->drained = accumulate(g->receiver, upstreamOrder(g->receiver), rain);

	// Rivers: the Rivers setting picks what share of the land carries one
	// (0.4% to 4%), so every world gets its proportion, whatever its shape.
	std::vector<int64_t> land_drained;
	for (size_t i = 0; i < n; i++)
		if (g->base[i] >= 0)
			land_drained.push_back(g->drained[i]);
	std::sort(land_drained.begin(), land_drained.end());
	fix share = ratio(4, 1000) + mul(code.rivers, ratio(36, 1000));
	// (streams: four times the share, the land between the rivers drained too)
	const auto threshold = [&](fix part, int32_t least) {
		if (land_drained.empty() || !code.processes.water)
			return INT64_MAX;
		size_t at = land_drained.size() - 1 - std::min(land_drained.size() - 1, (size_t)(((int64_t)land_drained.size() * part) >> FRAC));
		return std::max<int64_t>(land_drained[at], toFix(least));
	};
	// (a river drains at least eight cells, a stream two)
	g->river_threshold = threshold(share, 8);
	g->stream_threshold = std::min(g->river_threshold, threshold(share * 4, 2));

	// geology on the finished land: volcanoes, which cells their slopes
	// reach, then provinces (lava fields need the volcanoes)
	g->volcanoes = volcanoes(code, *g);
	{
		const int64_t radius = World::radiusOf(code), cell = World::cellBlocksOf(code);
		std::vector<std::vector<uint16_t>> reach(n);
		for (size_t k = 0; k < g->volcanoes.size(); k++) {
			const Volcano &v = g->volcanoes[k];
			// the cells within its slopes and two cells more, out from its vent's
			const int64_t within = (int64_t)v.radius * ONE + 2 * cell;
			for (int32_t c : sphere.around(sphere.cell(v.at), [&](int32_t c) {
					return World::blocksOf(chord(sphere.centre(c), v.at), radius) <= within; }))
				reach[c].push_back((uint16_t)k);
		}
		g->volcano_start.assign(n + 1, 0);
		for (size_t i = 0; i < n; i++) {
			g->volcano_start[i + 1] = g->volcano_start[i] + (uint32_t)reach[i].size();
			g->volcano_list.insert(g->volcano_list.end(), reach[i].begin(), reach[i].end());
		}
	}
	g->province = provinces(code, *g);
	g->fertility = fertility(code, *g);
	return g;
}

} // namespace

std::shared_ptr<const Grid> gridFor(const WorldCode &code)
{
	// A few recent worlds: the map generator's threads share one, and the
	// preview reuses it while only the date changes.
	static std::mutex lock;
	static std::vector<std::pair<std::string, std::shared_ptr<const Grid>>> recent;
	{
		std::lock_guard<std::mutex> guard(lock);
		for (auto &entry : recent)
			if (entry.first == code.text)
				return entry.second;
	}
	std::shared_ptr<const Grid> built = build(code);
	std::lock_guard<std::mutex> guard(lock);
	for (auto &entry : recent)
		if (entry.first == code.text)
			return entry.second; // another thread finished first: use theirs
	recent.insert(recent.begin(), { code.text, built });
	if (recent.size() > 4)
		recent.pop_back();
	return built;
}

} // namespace terragen
