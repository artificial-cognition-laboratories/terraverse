// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/surface/craters.h"
#include "library/world/terragen/src/surface/maria.h"
#include "library/world/terragen/src/surface/lid.h"
#include "library/world/terragen/src/grid/climate.h"
#include "library/world/terragen/src/grid/drainage.h"
#include "library/world/terragen/src/grid/erosion.h"
#include "library/world/terragen/src/base/noise.h"
#include "library/world/terragen/src/world.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <tuple>

#include "library/world/terragen/src/base/parallel.h"
#include "library/world/terragen/src/base/progress.h"

namespace terragen {

static int octavesFor(const WorldCode &c) { return 3 + roundInt(c.roughness * 3); }

const Sphere &gridSphere()
{
	static const Sphere sphere(GRID_FREQUENCY);
	return sphere;
}

std::vector<int32_t> lowestFirst(const std::vector<fix> &height)
{
	// each a height over its index in one number: sorted without looking anything up
	std::vector<uint64_t> keys(height.size());
	for (size_t i = 0; i < height.size(); i++)
		keys[i] = (uint64_t)((int64_t)height[i] - INT32_MIN) << 32 | (uint32_t)i;
	std::sort(keys.begin(), keys.end());
	std::vector<int32_t> by(height.size());
	for (size_t i = 0; i < height.size(); i++)
		by[i] = (int32_t)(uint32_t)keys[i];
	return by;
}

fix landmass(const WorldCode &c, const Dir &d)
{
	fix persistence = ratio(35, 100) + mul(c.roughness, ratio(25, 100));
	// continents, with the coastline broken up by finer noise
	return fbm(d, c.seed_hash + CONTINENT, c.continents, octavesFor(c), persistence)
			+ mul(fbm(d, c.seed_hash + COAST, c.continents * 7, 3, HALF), mul(c.coast, ratio(22, 100)));
}

namespace {

// Erosion: how hard running water bites, and how many rounds it gets: long
// enough for the rising belts to stand where uplift and erosion balance (a
// dry body's ancient seas, DRY_ROUNDS at most). Each round is STEP of the
// old ones: uplift, wear and creep all scale with it, so where they balance
// doesn't move, and the implicit stream power is stable at any step (creep,
// explicit, holds below a sixth)
const int STEP = 4;
const fix ERODIBILITY = ratio(6, 100) * STEP;
const int EROSION_ROUNDS = 300, DRY_ROUNDS = 12;
// The glacial buzzsaw (Brozović, Burbank & Meigs 1997; Egholm et al. 2009):
// above the snowline ice wears the land far faster than rivers, so ranges
// stand little over it however fast they rise. Erodibility grows to
// BUZZSAW more where the yearly mean is ICE_FULL under freezing
const fix BUZZSAW = toFix(8), ICE_FULL = toFix(12);
// The crust floats (erosion.h): land worn away rises back by this share,
// spread over this many rounds of neighbour averaging
const fix REBOUND = ratio(82, 100);
const int FLEX_PASSES = 3;
// What a stream lays down (the ξ–q model: Davy & Lague 2009; SPACE, Shobe
// et al. 2017, Landlab's SpaceLargeScaleEroder, MIT): crossing a cell it
// passes on Qs / (1 + G·a/q) of its load, a the cell's area, q the water
// through it (rain-weighted, in cells), and lays the rest. A small stream
// lays down most of what it carries, a big river little; where it lays more
// than it wears the bed builds up and steepens until it carries on.
const fix DEPOSIT = ratio(25, 100);
// Heavy grains (gold, tin) settle this many times as readily as the rest of
// a river's load: they gather where it first slows, below the mountains.
const int64_t HEAVY = 2;
// Each round, where the plates collide the land is pushed up by this much
// (times the belt, 0..1: the history's recent uplift), against which rivers
// wear it: a range stands where the two balance, not as the history left it.
const fix UPLIFT = ratio(11, 2000) * STEP;
// Hillslopes creep (erosion.h): each round a land cell trades this share of
// its height difference with each land neighbour, which fills the cells'
// sharpest notches. Soil creep is a hillside's process: across a cell's
// tens of kilometres it barely acts, so it reaches about a cell over the
// whole run (√(4κ·rounds)), whatever the rounds and their STEP.
const fix CREEP = ratio(1, 1250);
// Water deeper than a block (at 160 blocks per unit) counts as a lake…
const fix LAKE_DEPTH = ratio(1, 160);
// Glacial scour (see build): how deep, and below what yearly mean (°C).
const fix SCOUR = ratio(24, 160);
const fix SCOUR_BELOW = toFix(4);
// (shallower, it's a hollow that sediment fills)
// a closed basin's salt pan: where its water stands in the wet years, when
// this many times its mean inflow comes in (the floor its lake spreads over
// and dries back from; Bonneville's flats were its ice-age lake's floor)
const int64_t PAN_WET = 3;
// Snow on ground whose yearly mean (°C, at its height) is under MELT_FROM
// melts less of it each summer, and none under MELT_NONE: an ice sheet's,
// which flows away as ice, not in rivers (Greenland's, Antarctica's)
const fix MELT_FROM = -toFix(5), MELT_NONE = -toFix(15);
// Sinking basins (see build): this many cells in from the sea at the least,
// in noise this many waves round, where it's over BASIN_RARITY (of an fbm's
// typical swing, 1), sagging up to BASIN_DEPTH once BASIN_RISE over it
const int32_t BASIN_INLAND = 3, BASIN_WAVES = 9;
const fix BASIN_RARITY = ratio(40, 100), BASIN_RISE = ratio(35, 100), BASIN_DEPTH = ratio(30, 160), BASIN_SINK = ratio(5, 100);

} // namespace

namespace {

// Open water's evaporation a year, in rain's units, by the yearly heat:
// little where it's cold, several times the mean rain in a hot desert
fix evaporation(fix heat)
{
	const fix celsius = heat * 37 + toFix(11);
	return clamp(ratio(30, 100) + mul(celsius, ratio(7, 100)), ratio(15, 100), ratio(250, 100));
}

// What rain runs off the land, the rest evaporating or taken up by plants
// on the way: the Turc–Pike curve (a Budyko curve), ET = P / √(1 + (P/E)²),
// E what open water would give back. Where rain is much more than that,
// most runs off; where it's much less, nearly none.
fix runoff(fix rain, fix heat)
{
	const fix e = evaporation(heat);
	const fix wet = div(rain, e);
	return rain - div(rain, sqrtWide64((int64_t)ONE + mul(wet, wet)));
}

/*
	The water on the finished land, by the routing and balance the erosion
	ran on (drainage.h): lakes where the water stands a block or more over
	the ground; shallower, it's a hollow, which fills with sediment (the land
	rises to the water). A closed basin's floor, as far as its water spreads
	in the wet years (PAN_WET times its inflow), beyond its standing lake, is
	a salt pan (Bonneville's flats were its ice-age lake's floor). `filled`
	is the water's surface with a hair of fall along every way, so water on
	it always runs downhill.
*/
void standWater(Grid &g, const std::vector<fix> &rain, const std::vector<fix> &melted)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = (int32_t)g.eroded.size();
	std::vector<fix> gives(n), sheds(n);
	for (int32_t i = 0; i < n; i++) {
		const fix heat = g.heat[i] - mul(std::max<fix>(0, g.eroded[i]), LAPSE);
		gives[i] = evaporation(heat);
		sheds[i] = runoff(melted[i], heat);
	}
	const Balance balance{ sheds, gives };
	Routing way = route(g.eroded, &balance);
	bool hollows = false;
	for (int32_t i = 0; i < n; i++) {
		const fix deep = way.surface[i] - g.eroded[i];
		if (g.eroded[i] >= 0 && deep > 0 && deep < LAKE_DEPTH) {
			// (not carried in by the rivers' balance: counted apart)
			g.sediment[i] += deep;
			g.hollows += (int64_t)deep * sphere.area(i);
			g.eroded[i] = way.surface[i];
			hollows = true;
		}
	}
	if (hollows) // (they drain on, filled)
		way = route(g.eroded, &balance);

	g.receiver = way.receiver;
	g.drained = accumulate(way.receiver, way.order, rain);
	g.runoff = accumulate(way.receiver, way.order, melted);
	g.discharge = accumulate(way.receiver, way.order, sheds);
	g.lake.assign(n, 0);
	g.pan.assign(n, 0);
	g.lake_level.assign(n, 0);
	g.filled.resize(n);
	std::vector<int32_t> end(n);
	for (int32_t c : way.order) {
		const int32_t r = way.receiver[c];
		g.filled[c] = r < 0 ? way.surface[c] : std::max(way.surface[c], g.filled[r] + 1);
		end[c] = r < 0 ? c : end[r];
		if (g.eroded[c] >= 0 && way.surface[c] - g.eroded[c] >= LAKE_DEPTH) {
			g.lake[c] = 1;
			g.lake_level[c] = way.surface[c];
		}
	}

	// each closed basin's floor, flooded lowest first till its water would
	// give back PAN_WET times what comes in
	std::vector<std::vector<int32_t>> basin(n);
	for (int32_t i = 0; i < n; i++)
		if (way.receiver[end[i]] < 0 && g.eroded[end[i]] >= 0)
			basin[end[i]].push_back(i);
	for (int32_t pit = 0; pit < n; pit++) {
		std::vector<int32_t> &cells = basin[pit];
		if (cells.empty())
			continue;
		// (the pit first, where the water ends, whatever ties it on a flat)
		std::sort(cells.begin(), cells.end(), [&](int32_t x, int32_t y) {
			return std::make_tuple(x != pit, g.eroded[x], x) < std::make_tuple(y != pit, g.eroded[y], y);
		});
		int64_t inflow = 0, given = 0;
		for (int32_t c : cells)
			inflow += mul(sheds[c], sphere.area(c));
		std::vector<int32_t> wet;
		fix stood = g.eroded[cells[0]];
		for (int32_t c : cells) {
			if (!wet.empty() && given >= inflow * PAN_WET)
				break;
			wet.push_back(c);
			stood = std::max(stood, g.eroded[c]);
			given += mul(gives[c], sphere.area(c));
		}
		// (a pan's level: as high as its water stood, whose shore the salt follows)
		for (int32_t c : wet)
			if (!g.lake[c]) {
				g.pan[c] = 1;
				g.lake_level[c] = stood;
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

// A plateau's height: some 4.5 km (a unit is 5 km)
const fix PLATEAU = ratio(90, 100);

// The last few results of a stage and what they were made from. Tweaking a
// setting on the create screen builds world after world that differ only
// downstream of it: the plates needn't move again for a change of rivers.
template <typename Key, typename Value>
class Kept
{
public:
	template <typename Make>
	std::shared_ptr<const Value> get(const Key &key, Make &&make)
	{
		{
			std::lock_guard<std::mutex> hold(m_lock);
			for (const auto &e : m_recent)
				if (e.first == key)
					return e.second;
		}
		auto made = std::make_shared<const Value>(make());
		std::lock_guard<std::mutex> hold(m_lock);
		m_recent.insert(m_recent.begin(), { key, made });
		if (m_recent.size() > 3)
			m_recent.pop_back();
		return made;
	}

private:
	std::mutex m_lock;
	std::vector<std::pair<Key, std::shared_ptr<const Value>>> m_recent;
};

// (exactly what history() reads of a code, and landmass() for it)
std::shared_ptr<const History> historyKept(const WorldCode &c)
{
	static Kept<std::vector<int64_t>, History> kept;
	return kept.get({ c.seed_hash, c.continents, c.land, c.coast, c.roughness, c.mountains,
			(int64_t)c.processes.regime, c.processes.water }, [&] { return history(c); });
}

// Waves on the coasts (sprint 112): a wave's height grows with the fetch,
// the open water the wind blows over to reach the shore. A coast cell looks
// upwind FETCH steps of a cell's spacing; each step over the sea adds to
// its waves. Swell from far storms comes from every side: SWELL of a coast's
// waves come from how open it is (its neighbours' sea), a bay's least. A
// wind half the sea's median or less raises small waves.
static const int32_t FETCH = 8;
static const fix SWELL = ratio(35, 100);

static fix windAt(const Grid &g, int32_t i)
{
	return sqrt(mul(g.wind[0][i], g.wind[0][i]) + mul(g.wind[1][i], g.wind[1][i]) + mul(g.wind[2][i], g.wind[2][i]));
}

static std::vector<fix> surf(const WorldCode &code, const Grid &g)
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

// (exactly what climate() reads: the land, and the sky's settings)
std::shared_ptr<const Climate> climateKept(const WorldCode &c, const std::vector<fix> &land)
{
	static Kept<std::pair<std::vector<int64_t>, std::vector<fix>>, Climate> kept;
	return kept.get({ { c.processes.warming, c.temperature, c.humidity, c.tilt }, land }, [&] { return climate(c, land); });
}

// Rivers: where enough rain has gathered, so a wet land has many and a
// desert few (drained counts cells of rain, a cell of average rain one).
// The Rivers setting says how much makes one: from RIVER_FROM cells' rain
// down to RIVER_TO, as the cube of what's left of the setting, so most of
// its range is the dense end (the default, half way, about 6.5 cells: a
// network that branches to every valley; the old line put it at 14, a few
// big rivers and little feeding them). (Streams are rivers for now: what's
// smaller is a dry valley till water can follow the fine ground.)
int64_t riverThreshold(const WorldCode &code)
{
	static const fix RIVER_FROM = toFix(24), RIVER_TO = toFix(4);
	if (!code.processes.water)
		return INT64_MAX;
	const fix left = ONE - code.rivers;
	return (int64_t)(RIVER_TO + mul(RIVER_FROM - RIVER_TO, mul(mul(left, left), left)));
}

std::shared_ptr<Grid> build(const WorldCode &code, const std::function<void(const ErosionRound &)> *watch = nullptr)
{
	const Sphere &sphere = gridSphere();
	auto g = std::make_shared<Grid>();
	size_t n = (size_t)sphere.size();
	std::vector<fix> rain(n);

	// The plates' history makes the land (history.cpp); the coasts are
	// broken up by finer noise (the Coast setting), the sea placed by
	// quantile so exactly the Land share of the planet's area is above it
	// (each part timed and counted for whoever's waiting: progress.h)
	std::optional<Step> step;
	step.emplace(code.text, "plates");
	const std::shared_ptr<const History> kept_past = historyKept(code);
	const History &past = *kept_past;
	g->tectonics = tectonics(code, past);
	g->history = past.events;
	step.reset();
	step.emplace(code.text, "crust and climate");
	std::vector<fix> raw(n);
	parallelFor((int32_t)n, [&](int32_t i) {
		raw[i] = past.cells[i].height + mul(fbm(sphere.centre(i), code.seed_hash + COAST, code.continents * 7, 3, HALF),
				mul(code.coast, ratio(12, 100)));
	});
	// the ocean floor slopes down from the continents over a few cells (the
	// continental slope), not a cliff from shelf to abyss in one; and floor
	// handed over a step at a time leaves no lines where its ages meet
	for (int pass = 0; pass < 6; pass++) {
		const std::vector<fix> was = raw;
		parallelFor((int32_t)n, [&](int32_t i) {
			if (past.cells[i].continental)
				return;
			int64_t sum = was[i];
			int32_t count = 1;
			for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, count++)
				sum += was[*p];
			raw[i] = std::min<fix>(was[i] + ratio(10, 100), (fix)(sum / count));
		});
	}
	{
		const std::vector<int32_t> by = lowestFirst(raw);
		const int64_t sea = (int64_t)n * (ONE - code.land); // of the planet's n·ONE (each cell's area()), fixed
		int64_t under = 0;
		size_t k = 0;
		while (k + 1 < n && under + sphere.area(by[k]) <= sea)
			under += sphere.area(by[k++]);
		g->sea_offset = raw[by[k]];
	}
	// (the continents' platforms and old shields stand low by the erosion and
	// the crust's floating, not by remapping the history's heights: below)
	g->base.resize(n);
	parallelFor((int32_t)n, [&](int32_t i) {
		const fix land = raw[i] - g->sea_offset;
		const fix relief = g->tectonics.relief[i];
		// trenches and abyssal hills shape the sea floor, never above the sea;
		// rifts sink the land, never below it
		// (on a dry body there's no waterline to hold to: the land just goes on down)
		if (!code.processes.water) {
			g->base[i] = mul(land + relief, code.processes.stiff);
			return;
		}
		g->base[i] = land < 0 ? std::min<fix>(land + relief + fbm(sphere.centre(i), code.seed_hash + DETAIL, 24, 3, HALF) / 25, -ratio(1, 160))
				: std::max<fix>(land + relief, ratio(1, 160));
	});

	// An ice crust flows: over the ages its broad relief sags, the harder its
	// tides warm it the more, and no scarp stands (the old continents' edges)
	if (code.processes.stiff < ONE) {
		const fix sag = div(ONE, ONE + code.processes.flexing);
		for (size_t i = 0; i < n; i++)
			g->base[i] = mul(g->base[i], sag);
		for (int pass = 0; pass < 16; pass++) {
			const std::vector<fix> was = g->base;
			for (size_t i = 0; i < n; i++) {
				int64_t sum = was[i];
				int32_t count = 1;
				for (const int32_t *p = sphere.neighboursBegin((int32_t)i); p != sphere.neighboursEnd((int32_t)i); p++, count++)
					sum += was[*p];
				g->base[i] = (fix)(sum / count);
			}
		}
	}

	// Crust thickened behind a collision stands as a high plain (Tibet): level,
	// and dry in its range's shadow, so it wears slowly (the erosion below)
	{
		std::vector<fix> level = g->base;
		for (int pass = 0; pass < 32; pass++) {
			const std::vector<fix> was = level;
			parallelFor((int32_t)n, [&](int32_t i) {
				int64_t sum = was[i];
				int32_t count = 1;
				for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++, count++)
					sum += was[*p];
				level[i] = (fix)(sum / count);
			});
		}
		for (size_t i = 0; i < n; i++)
			if (g->tectonics.plateau[i] > 0 && g->base[i] >= 0)
				g->base[i] += mul(std::max(level[i], PLATEAU) - g->base[i], g->tectonics.plateau[i]);
	}


	thinCrust(code, *g);
	shapeLid(code, *g);

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
		raiseRise(code, *g);
		for (size_t i = 0; i < n; i++)
			g->base[i] += g->crater_young[i];
	}

	// The climate over the land as made (climate.h): where rain falls, which
	// wears the land and feeds the rivers (a little everywhere, so every slope runs)
	{
		const std::shared_ptr<const Climate> kept_climate = climateKept(code, g->base);
		const Climate &cl = *kept_climate;
		g->rain_season[0] = cl.rain[0];
		g->rain_season[1] = cl.rain[1];
		g->sea_heat = cl.sea;
		for (int k = 0; k < 3; k++)
			g->wind[k] = cl.wind[k];
		g->heat = cl.heat;
		g->heat_gain = cl.gain;
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
		// (on a worn-flat shield a hollow as deep would join its neighbours into one inland sea)
		const fix depth = mul(mul(mul(SCOUR, patches), std::min<fix>(ONE, cold * 4)), ONE - g->tectonics.stable[i] * 2 / 3);
		g->base[i] = std::max<fix>(g->base[i] - depth, ratio(1, 160));
	}

	// Erosion (erosion.h): rounds of uplift, stream power over the land's
	// drainage with its pits routed to the sea (drainage.h), and creep. Lakes
	// don't erode their beds; the sea stays put. What it wears comes off the
	// cell's loose sediment first, then its rock, and is carried down the same
	// receivers, laid by DEPOSIT where streams are small (floodplains, fans);
	// a lake takes all that reaches it till it's full, and the sea builds a
	// shelf at the coast. Nothing is made or lost but what goes out to the
	// deep sea (Grid::sediment, testSediment).
	std::vector<fix> h = g->base;
	g->sediment.assign(n, 0);
	// What's rising now starts from the crust it stands on and rises under
	// rivers already running (antecedent rivers cut gorges through ranges
	// that rose across them): its height over that comes off, and uplift
	// builds the range back over the rounds (not a plateau's: thick crust
	// floats high, risen long since)
	if (code.processes.water)
		for (size_t i = 0; i < n; i++)
			if (h[i] > 0) {
				const fix range = mul(mul(h[i], clamp(g->tectonics.belt[i], 0, ONE)), ONE - g->tectonics.plateau[i]);
				h[i] -= range;
				g->uplifted -= (int64_t)range * sphere.area((int32_t)i); // (the sediment's balance holds)
			}
	// Sinking basins: away from the coasts and the rising ranges, some of a
	// continent's interior sags (the crust stretched or loaded beside a range:
	// the Great Basin, the Caspian's, the Tarim, Lake Eyre's), all the time
	// the rivers work, BASIN_SINK of BASIN_DEPTH a round. Where it sinks
	// faster than its rivers fill it or cut in from outside, it's a closed
	// hollow, and the water's balance says what it holds: an inland sea where
	// rain is plenty, a salt lake or a dry pan where it isn't; where it
	// doesn't, it's a plain of what they laid.
	std::vector<fix> sinks(n, 0);
	if (code.processes.water) {
		// rings of cells in from the sea
		std::vector<int32_t> inland(n, INT32_MAX);
		std::vector<int32_t> ring;
		for (size_t i = 0; i < n; i++)
			if (g->base[i] < 0) {
				inland[i] = 0;
				ring.push_back((int32_t)i);
			}
		for (size_t head = 0; head < ring.size(); head++) {
			const int32_t c = ring[head];
			for (const int32_t *p = sphere.neighboursBegin(c); p != sphere.neighboursEnd(c); p++)
				if (inland[*p] == INT32_MAX) {
					inland[*p] = inland[c] + 1;
					ring.push_back(*p);
				}
		}
		for (size_t i = 0; i < n; i++) {
			if (g->base[i] <= 0 || inland[i] < BASIN_INLAND)
				continue;
			const fix where = fbm(sphere.centre((int32_t)i), code.seed_hash + BASINS, BASIN_WAVES, 3, HALF) * 3 - BASIN_RARITY;
			if (where <= 0)
				continue;
			// (none in a rising range; deepest well in from the coast)
			// (a shield's slower rise isn't more sinking: read against a young land's)
			const fix away = ONE - clamp(std::max<fix>(g->tectonics.belt[i], ratio(18, 100)) * 2, 0, ONE);
			const fix in = clamp((fix)((int64_t)(inland[i] - BASIN_INLAND + 1) * ONE / BASIN_INLAND), 0, ONE);
			sinks[i] = mul(mul(mul(mul(smooth(clamp(div(where, BASIN_RISE), 0, ONE)), away), in), BASIN_DEPTH), BASIN_SINK);
		}
	}
	// (a plateau's thick crust barely wears: a twentieth of the rest; and
	// what's frozen wears more, by its height each round: the buzzsaw)
	// (a dry body's ancient seas wore it a round at a time, not STEP: its
	// few rounds are the old ones)
	const fix pace = code.processes.water ? ONE : ONE / STEP;
	std::vector<fix> erodibility(n), hard(n);
	for (size_t i = 0; i < n; i++)
		hard[i] = mul(mul(ERODIBILITY, pace), ONE - g->tectonics.plateau[i] * 19 / 20);
	std::vector<int64_t> heavy_laid(n, 0); // heavy grains in each cell's sediment, as height × area
	// the last round's, per cell: what it wore, and what it laid (and of the
	// heavy grains): where it wore at least as much, that's load in transit
	std::vector<fix> worn_last(n, 0), laid_last(n, 0);
	std::vector<int64_t> dropped_last(n, 0), ran(n, 0);
	std::vector<int64_t> crumbs(n, 0); // creep's remainders below a unit of height (× area), kept for the next round
	// (no water, no erosion: the land stays as the history left it. Seas long
	// gone wore it as long as they lasted, against their own level, with no
	// belts rising to renew it: the old valleys)
	const int rounds = code.processes.water ? EROSION_ROUNDS : std::min<int>(DRY_ROUNDS, floorInt(DRY_ROUNDS * code.processes.ancient_water) * 2 / 9);
	fix old_sea = 0;
	if (!code.processes.water && rounds > 0) {
		std::vector<fix> low = h;
		std::nth_element(low.begin(), low.begin() + (ptrdiff_t)(n * 3 / 10), low.end());
		old_sea = low[n * 3 / 10];
		for (fix &v : h)
			v -= old_sea;
	}
	if (watch) {
		const Routing first = route(h);
		(*watch)({ h, g->sediment, accumulate(first.receiver, first.order, rain) });
	}
	step.reset();
	step.emplace(code.text, "erosion", rounds);
	for (int round = 0; round < rounds; round++) {
		if (round > 0)
			step->advance();
		for (size_t i = 0; i < n && code.processes.water; i++)
			if (h[i] >= 0) {
				// (a plateau stands as high as thick crust floats: it has stopped rising)
				// (× the Mountains setting, 0.5..1.5: the history's belt is capped,
				// so how hard the plates push shows here, in how high ranges stand)
				const fix up = mul(mul(mul(UPLIFT, HALF + code.mountains), g->tectonics.belt[i]), ONE - g->tectonics.plateau[i]);
				h[i] += up;
				g->uplifted += (int64_t)up * sphere.area((int32_t)i);
				// (sinking is the crust's doing, not the rivers': uplift's
				// opposite, so the sediment's balance still holds)
				if (sinks[i] > 0) {
					const fix was = h[i];
					h[i] = std::max<fix>(h[i] - sinks[i], std::min<fix>(was, ratio(1, 160)));
					g->uplifted -= (int64_t)(was - h[i]) * sphere.area((int32_t)i);
				}
			}
		for (size_t i = 0; i < n; i++) {
			const fix frozen = clamp(div(-celsiusOf(g->heat[i] - mul(std::max<fix>(0, h[i]), LAPSE)), ICE_FULL), 0, ONE);
			erodibility[i] = mul(hard[i], ONE + mul(BUZZSAW, frozen));
		}
		// (which basins spill: rain against what open water gives back, warmer
		// the lower it stands)
		std::vector<fix> gives(n);
		for (size_t i = 0; i < n; i++)
			gives[i] = evaporation(g->heat[i] - mul(std::max<fix>(0, h[i]), LAPSE));
		std::vector<fix> sheds(n);
		for (size_t i = 0; i < n; i++)
			sheds[i] = runoff(rain[i], g->heat[i] - mul(std::max<fix>(0, h[i]), LAPSE));
		const Balance balance{ sheds, gives };
		const Routing way = route(h, code.processes.water ? &balance : nullptr);
		const std::vector<int32_t> &down = way.receiver, &order = way.order;
		const std::vector<fix> &surface = way.surface;
		const std::vector<int64_t> area = accumulate(down, order, rain);
		ran = area;
		const std::vector<fix> before = h;
		const std::vector<fix> floated = h; // (what the crust answers: all this round takes off or lays on)
		streamPower(h, way, area, erodibility);
		// downstream, heads first: each cell's load is its donors' plus what it
		// wore, in height × area (fixed); none is raised to its lowest donor (a
		// river never dams itself), nor a lake past its surface (and the heavy
		// grains in it, worn out of the mountain belts' cores, where veins and
		// plutons are: gold, tin; they settle first)
		std::vector<int64_t> load(n, 0), heavy(n, 0);
		std::vector<fix> lowest_donor(n, INT32_MAX);
		for (size_t k = order.size(); k-- > 0;) {
			const int32_t c = order[k];
			const int64_t a = sphere.area(c);
			const fix worn = before[c] - h[c];
			worn_last[c] = worn;
			laid_last[c] = 0;
			dropped_last[c] = 0;
			if (worn > 0) {
				// the loose sediment first, and its heavy grains with it
				const fix loose = std::min(worn, g->sediment[c]);
				if (loose > 0) {
					const int64_t gone = heavy_laid[c] * loose / g->sediment[c];
					heavy[c] += gone;
					heavy_laid[c] -= gone;
					g->sediment[c] -= loose;
				}
				// (heavy grains are a small share of the rock: up to 1/64, deepest in the core)
				if (worn > loose && g->tectonics.belt[c] > VEIN_CORE)
					heavy[c] += ((int64_t)(worn - loose) * a >> FRAC) * div(g->tectonics.belt[c] - VEIN_CORE, ONE - VEIN_CORE) / 64;
				load[c] += (int64_t)worn * a;
			}
			const int32_t r = down[c];
			int64_t keep; // the most this cell may be raised, and of its load the share laid (fixed)
			int64_t share;
			if (h[c] < 0 && r < 0) {
				keep = -1 - h[c]; // the sea: a shelf up to just under its level
				share = ONE;
			} else if (r < 0) {
				keep = INT32_MAX; // a closed basin's lowest: all that reaches it stays (a salt flat's fill)
				share = ONE;
			} else if (surface[c] > before[c]) {
				keep = surface[c] - h[c]; // a lake (as the round began): all of it settles, till it's full
				share = ONE;
			} else {
				keep = (int64_t)lowest_donor[c] - 1 - h[c]; // (64 bits: with no donor that's INT32_MAX less a height)
				const int64_t settles = mulShift(DEPOSIT, a, FRAC);
				share = (settles << FRAC) / std::max<int64_t>(area[c] + settles, 1);
			}
			const fix laid = (fix)std::clamp<int64_t>(std::min(keep, mulShift(load[c], share, FRAC) / a), 0, INT32_MAX);
			if (laid > 0) {
				// heavy grains drop out HEAVY times as readily as the rest
				const int64_t part = std::min<int64_t>(ONE, ((int64_t)laid * a << FRAC) / std::max<int64_t>(load[c], 1) * HEAVY);
				const int64_t dropped = mulShift(heavy[c], part, FRAC);
				h[c] += laid;
				g->sediment[c] += laid;
				heavy_laid[c] += dropped;
				laid_last[c] = laid;
				dropped_last[c] = dropped;
				load[c] -= (int64_t)laid * a;
				heavy[c] -= dropped;
			}
			if (r >= 0) {
				load[r] += load[c];
				heavy[r] += heavy[c];
				lowest_donor[r] = std::min(lowest_donor[r], h[c]);
			} else {
				g->to_deep += load[c];
			}
		}
		// creep, between land cells (the coast stays where the rivers left it)
		creep(h, mul(CREEP, pace), crumbs);
		g->uplifted += rebound(h, floated, REBOUND, FLEX_PASSES);
		if (watch)
			(*watch)({ h, g->sediment, area });
	}
	for (size_t i = 0; i < n; i++)
		g->to_deep += crumbs[i]; // (under a unit of height a cell: counted as gone, so the balance is exact)
	for (fix &v : h)
		v += old_sea;
	g->eroded = h;
	step.reset();
	step.emplace(code.text, "lakes and rivers");
	g->worn.resize(n);
	for (size_t i = 0; i < n; i++)
		g->worn[i] = g->base[i] >= 0 ? g->eroded[i] - g->base[i] : 0;
	// placers: the share of what was laid that's heavy grains, ×12 (0..1: rich gravel is a few percent gold and tin)
	// What lies on the land: off the rivers, a gully that wears as fast as it
	// lays (steep, at its balance) only carries its load across: the bed it
	// lays one round it wears the next, so its last round's is in transit,
	// not alluvium. A river reworks its bed and floodplain the same way, but
	// always floors its valley with what it carries: that stays.
	const int64_t river = riverThreshold(code);
	for (size_t i = 0; i < n; i++)
		if (laid_last[i] > 0 && worn_last[i] >= laid_last[i] && ran[i] < river) {
			g->sediment[i] -= laid_last[i];
			heavy_laid[i] -= dropped_last[i];
		}
	g->placer.assign(n, 0); // (heavy_laid: what's in the sediment now)
	for (size_t i = 0; i < n; i++)
		// (below the source rock: within it, the grains are still in their veins)
		if (g->sediment[i] > 0 && g->tectonics.belt[i] <= VEIN_CORE)
			g->placer[i] = (fix)std::min<int64_t>(ONE, (heavy_laid[i] / sphere.area((int32_t)i) * 12 << FRAC) / g->sediment[i]);

	// The final water: lakes, closed basins' pans, the rivers' ways
	// what runs off as water: the rain, less the snow that never melts (an
	// ice sheet's, which feeds no river and fills no lake; Grid::runoff)
	std::vector<fix> melted(n);
	for (size_t i = 0; i < n; i++) {
		const fix celsius = celsiusOf(g->heat[i] - mul(std::max<fix>(0, g->eroded[i]), LAPSE));
		melted[i] = mul(rain[i], clamp(div(celsius - MELT_NONE, MELT_FROM - MELT_NONE), 0, ONE));
	}
	if (code.processes.water)
		standWater(*g, rain, melted);
	else {
		// (basins stand dry: water, if any, ends in each)
		g->filled = g->eroded;
		g->lake.assign(n, 0);
		g->pan.assign(n, 0);
		g->lake_level.assign(n, 0);
		g->receiver = receivers(g->filled);
		const std::vector<int32_t> order = upstreamOrder(g->receiver);
		g->drained = accumulate(g->receiver, order, rain);
		g->runoff = accumulate(g->receiver, order, melted);
		std::vector<fix> sheds(n);
		for (size_t i = 0; i < n; i++)
			sheds[i] = runoff(melted[i], g->heat[i] - mul(std::max<fix>(0, g->eroded[i]), LAPSE));
		g->discharge = accumulate(g->receiver, order, sheds);
	}

	g->river_threshold = g->stream_threshold = riverThreshold(code);

	// geology on the finished land: volcanoes, which cells their slopes
	// reach, then provinces (lava fields need the volcanoes)
	step.reset();
	step.emplace(code.text, "geology");
	g->volcanoes = volcanoes(code, *g);
	{
		const int64_t radius = World::radiusOf(code), cell = World::cellBlocksOf(code);
		std::vector<std::vector<uint16_t>> reach(n);
		for (size_t k = 0; k < g->volcanoes.size(); k++) {
			const Volcano &v = g->volcanoes[k];
			// the cells within its slopes and two cells more, out from its vent's
			const int64_t within = (v.kind == VolcanoKind::Patera ? (int64_t)v.radius * ONE * 3 / 2 : floods(v) ? lavaReach(v) * ONE : (int64_t)v.radius * ONE) + 2 * cell; // (a patera's outline wanders out; a young shield's plains run far)
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
	g->surf = surf(code, *g);
	g->province = provinces(code, *g);
	g->fertility = fertility(code, *g);
	return g;
}

} // namespace

// (grid_file.cpp) none: not kept yet
std::shared_ptr<const Grid> loadGrid(const std::string &code);
void saveGrid(const std::string &code, const Grid &grid);

std::shared_ptr<const Grid> watchErosion(const WorldCode &code, const std::function<void(const ErosionRound &)> &watch)
{
	return build(code, &watch);
}

std::shared_ptr<const Grid> gridFor(const WorldCode &code)
{
	// A few recent worlds: the map generator's threads share one, and the
	// preview reuses it while only the date changes.
	static std::mutex lock;
	static std::vector<std::pair<std::string, std::shared_ptr<const Grid>>> recent;
	{
		std::lock_guard<std::mutex> guard(lock);
		for (auto &entry : recent)
			if (entry.first == code.grid_key)
				return entry.second;
	}
	std::shared_ptr<const Grid> built;
	{
		Step step(code.text, "grid kept");
		built = loadGrid(code.grid_key);
		if (built)
			step.kept();
	}
	if (!built) {
		built = build(code);
		saveGrid(code.grid_key, *built);
	}
	std::lock_guard<std::mutex> guard(lock);
	for (auto &entry : recent)
		if (entry.first == code.grid_key)
			return entry.second; // another thread finished first: use theirs
	recent.insert(recent.begin(), { code.grid_key, built });
	if (recent.size() > 4)
		recent.pop_back();
	return built;
}

} // namespace terragen
