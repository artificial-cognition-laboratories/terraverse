// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "grid.h"
#include "climate.h"
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

fix elevation(const WorldCode &c, const Dir &d, fix sea_offset, fix belt, fix relief)
{
	fix land = landmass(c, d) - sea_offset;
	if (land < 0) // ridges and trenches shape the sea floor, never above the sea
		return std::min<fix>(land + relief, -ratio(1, 160));
	// mountain ridges, sharp where their noise crosses zero, rising from the coast inland
	fix ridge = ONE - fabsx(fbm(d, c.seed_hash + RIDGE, c.continents * 4, octavesFor(c), HALF));
	fix inland = clamp(div(land, ratio(25, 100)), 0, ONE);
	// plains level off inland (land / (land + 0.3) · 0.4 → at most ~0.4):
	// height comes from mountain ranges, not from distance to the coast
	fix plains = div(mul(land, ratio(4, 10)), land + ratio(3, 10));
	// ranges run along the plates' collisions: the belt scales them (×1.6 at
	// a boundary, low in plate interiors); rifts sink, never below the sea
	fix range = mul(mul(mul(mul(ridge, ridge), c.mountains), inland), mul(belt, ratio(16, 10)));
	return std::max<fix>(plains + range + relief, ratio(1, 160));
}

namespace {

// Erosion: how hard running water bites, and how many rounds it gets.
const fix ERODIBILITY = ratio(6, 100);
const int EROSION_ROUNDS = 12;
// How much a stream carries, as a multiple of its power to cut (F · drop):
// past that, it lays its load down.
const fix CARRY = toFix(2);
// Water deeper than a block (at 160 blocks per unit) counts as a lake…
const fix LAKE_DEPTH = ratio(1, 160);
// …if its basin is big and deep enough; smaller hollows fill with sediment,
// and so do basins wider than this share of the land (they'd be inland seas).
// (On the sphere's cells, about two thirds of the flat map's in area, hollows
// come out shallower: 15 cells and 4 blocks deep keep about as many lakes.)
const int32_t LAKE_MIN_CELLS = 15;
const fix LAKE_MIN_DEEPEST = ratio(4, 160);
const fix LAKE_MAX_SHARE = ratio(3, 100);
// Steepness toward a neighbour is drop ÷ distance; compared as drop · SPAN ÷
// distance (fixed, unit sphere) so a one-unit drop never rounds away.
const int64_t SPAN = (int64_t)1 << 24;

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
		bool real = (int32_t)basin.size() >= LAKE_MIN_CELLS && (int32_t)basin.size() <= max_cells &&
				deepest >= LAKE_MIN_DEEPEST;
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
		const Dir &d = sphere.centre(i);
		fix u = World::u(d), v = World::v(d);
		fix sun = sunlight(code, v, std::nullopt);
		fix supply = SUPPLY[(int)g.province[i]];
		// rivers spread silt over the low land they cross
		fix flood = e < ratio(12, 100) && g.river_threshold != INT64_MAX
				? mul(ratio(35, 100), (fix)std::min<int64_t>(ONE, ((g.drained[i] << FRAC) / g.river_threshold)))
				: 0;
		fix heat = heatAt(code, u, v, sun, e) + g.sea_heat[i];
		// leaching grows with wetness (log₂ of the rain, like the biomes') and warmth
		fix wet = clamp(log2(maxx(g.rain(i), ratio(1, 100))) / 2, -ONE, ONE);
		fix leached = mul(mul(maxx(0, wet + ratio(5, 10)), maxx(0, heat + ratio(3, 10))), ratio(45, 100));
		out[i] = clamp(supply + flood - leached, 0, ONE);
	}
	return out;
}

// The climate grid's cells (CLIMATE², row 0 north, columns wrapping) round a
// place, and how much each counts: the climate is read between its centres.
void climateAt(fix u, fix v, int32_t cells[4], fix weights[4])
{
	fix x = mul(u, toFix(CLIMATE)) - HALF, z = clamp(mul(v, toFix(CLIMATE)) - HALF, 0, toFix(CLIMATE - 1));
	int32_t x0 = floorInt(x), z0 = floorInt(z), z1 = std::min(z0 + 1, CLIMATE - 1);
	auto at = [&](int32_t ax, int32_t az) { return az * CLIMATE + ((ax % CLIMATE) + CLIMATE) % CLIMATE; };
	fix tx = fraction(x), tz = fraction(z);
	cells[0] = at(x0, z0);
	cells[1] = at(x0 + 1, z0);
	cells[2] = at(x0, z1);
	cells[3] = at(x0 + 1, z1);
	weights[0] = mul(ONE - tx, ONE - tz);
	weights[1] = mul(tx, ONE - tz);
	weights[2] = mul(ONE - tx, tz);
	weights[3] = ONE - weights[0] - weights[1] - weights[2];
}

// A grid field read at a place, between the three cells round it.
fix read(const std::vector<fix> &field, const Dir &d)
{
	const Sphere::Spot spot = gridSphere().spot(d);
	int64_t sum = 0;
	for (int k = 0; k < 3; k++)
		sum += (int64_t)field[spot.cells[k]] * spot.weights[k];
	return (fix)(sum >> FRAC);
}

std::shared_ptr<Grid> build(const WorldCode &code)
{
	const Sphere &sphere = gridSphere();
	auto g = std::make_shared<Grid>();
	size_t n = (size_t)sphere.size();
	std::vector<fix> mass(n), rain(n);
	for (size_t i = 0; i < n; i++)
		mass[i] = landmass(code, sphere.centre((int32_t)i));

	// The sea, by quantile of the landmass field: exactly the Land share of
	// the planet's area is above it.
	{
		std::vector<int32_t> by(n);
		for (size_t i = 0; i < n; i++)
			by[i] = (int32_t)i;
		std::sort(by.begin(), by.end(), [&](int32_t a, int32_t b) { return mass[a] != mass[b] ? mass[a] < mass[b] : a < b; });
		const int64_t sea = (int64_t)n * (ONE - code.land); // of the planet's n·ONE (each cell's area()), fixed
		int64_t under = 0;
		size_t k = 0;
		while (k + 1 < n && under + sphere.area(by[k]) <= sea)
			under += sphere.area(by[k++]);
		g->sea_offset = mass[by[k]];
	}
	// the plates, before the height: mountains follow their collisions
	g->tectonics = tectonics(code, mass, g->sea_offset);
	g->base.resize(n);
	for (size_t i = 0; i < n; i++)
		g->base[i] = elevation(code, sphere.centre((int32_t)i), g->sea_offset, g->tectonics.belt[i], g->tectonics.relief[i]);

	// The climate over the land as made, on its own grid in latitude and
	// longitude (smooth and banded by latitude, so its rows suit it; the
	// energy-balance climate replaces it): where rain falls, which wears the
	// land and feeds the rivers (a little everywhere, so every slope runs)
	{
		std::vector<fix> coarse((size_t)CLIMATE * CLIMATE);
		for (int32_t cz = 0; cz < CLIMATE; cz++)
			for (int32_t cx = 0; cx < CLIMATE; cx++) {
				// the land under a climate cell: four points across it
				int64_t sum = 0;
				for (int32_t s4 = 0; s4 < 4; s4++) {
					fix u = ratio(4 * cx + 1 + 2 * (s4 & 1), 4 * CLIMATE), v = ratio(4 * cz + 1 + 2 * (s4 >> 1), 4 * CLIMATE);
					sum += read(g->base, World::place(u, v).dir());
				}
				coarse[cz * CLIMATE + cx] = (fix)(sum / 4);
			}
		Climate cl = climate(code, coarse);
		// back onto the cells: each centre, between the climate's
		auto onto = [&](const std::vector<fix> &from, std::vector<fix> &to) {
			to.resize(n);
			for (size_t i = 0; i < n; i++) {
				const Dir &d = sphere.centre((int32_t)i);
				int32_t cells[4];
				fix weights[4];
				climateAt(World::u(d), World::v(d), cells, weights);
				int64_t sum = 0;
				for (int k = 0; k < 4; k++)
					sum += (int64_t)from[cells[k]] * weights[k];
				to[i] = (fix)(sum >> FRAC);
			}
		};
		onto(cl.rain[0], g->rain_season[0]);
		onto(cl.rain[1], g->rain_season[1]);
		onto(cl.sea, g->sea_heat);
		for (size_t i = 0; i < n; i++)
			rain[i] = clamp(g->rain((int32_t)i), ratio(15, 100), toFix(3));
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
	for (int round = 0; round < EROSION_ROUNDS; round++) {
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
		std::vector<int64_t> load(n, 0);
		std::vector<fix> lowest_donor(n, INT32_MAX);
		for (size_t k = order.size(); k-- > 0;) {
			int32_t c = order[k];
			const int64_t a = sphere.area(c);
			load[c] += (int64_t)(before[c] - h[c]) * a;
			fix keep; // the most this cell may be raised
			if (h[c] < 0 && down[c] < 0)
				keep = -1 - h[c];                // the sea: a shelf up to just under its level
			else if (surface[c] > h[c])
				keep = (surface[c] - h[c]) / 2;  // a lake: silts up, but stays a lake
			else
				keep = load[c] / a > power[c] ? (fix)(load[c] / a - power[c]) : 0; // past what it can carry
			keep = std::min(keep, lowest_donor[c] - 1 - h[c]);
			if (keep > 0) {
				const fix laid = (fix)std::min<int64_t>(keep, load[c] / a);
				h[c] += laid;
				g->sediment[c] += laid;
				load[c] -= (int64_t)laid * a;
			}
			int32_t r = down[c];
			if (r >= 0) {
				load[r] += load[c];
				lowest_donor[r] = std::min(lowest_donor[r], h[c]);
			} else {
				g->to_deep += load[c];
			}
		}
	}
	g->eroded = h;

	// The final water: fill, keep the basins that make real lakes, drain, accumulate.
	g->filled = fill(g->eroded);
	g->lake.resize(n);
	g->lake_level.resize(n);
	keepRealLakes(*g);
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
	if (land_drained.empty()) {
		g->river_threshold = INT64_MAX;
	} else {
		size_t at = land_drained.size() - 1 - std::min(land_drained.size() - 1, (size_t)(((int64_t)land_drained.size() * share) >> FRAC));
		g->river_threshold = std::max<int64_t>(land_drained[at], toFix(8));
	}

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
