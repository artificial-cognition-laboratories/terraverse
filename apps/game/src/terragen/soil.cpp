// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "climate.h"
#include "noise.h"
#include "parallel.h"
#include "world.h"
#include <algorithm>

/*
	Soil (sprint 111): what the ground is made of, made the way real ground
	is. Nothing is chosen by kind; each process moves grains by size:

	- weathering: the rock under it falls apart into its crystals and its
	  minerals change, each at its own rate (feldspar to clay, olivine and
	  calcite into the water, quartz never). How far it's gone is the
	  climate's rate (twice as fast every 10 °C, as fast as the rain) times
	  how long the ground has stood: old, stable crust weathers deep (the
	  tropics' red clays); young belts, steep slopes and ground the ice
	  scraped stay stony and sandy;
	- slopes: the fines wash off steep ground and ridges and gather in
	  hollows;
	- rivers sort it: a channel's bed keeps what its current can't move
	  (Shields: the largest grain moved grows with depth × slope), its banks
	  the next finer, its floodplain silt, and clay furthest out where
	  floods stand still; lakes lay clay;
	- ice leaves till: every size at once;
	- plants leave organic matter, which rots fast where it's warm and
	  barely where it's cold and waterlogged (peat).

	The sizes are matter's bands (matter/grains.h) and the minerals its
	data, mirrored below.
*/
namespace terragen {

namespace {

const fix ORGANIC_LEVELS[SOIL_ORGANIC] = { 0, ratio(8, 100), ratio(40, 100) }; // mineral, ordinary topsoil, peat

const int32_t TRIANGLE = 66; // sand and clay in tenths with sand + clay ≤ 10

int32_t triangle(int32_t sand, int32_t clay)
{
	return sand * 11 - sand * (sand - 1) / 2 + clay;
}


enum : int { STONE, GRAVEL, SAND, SILT, CLAY };

// how far weathering goes at 15 °C, the median rain, crust of middling age:
// a granite there is a sandy loam (TestMatter's mild weathering)
const fix EXTENT = ratio(35, 10);

// Goldich's order (substances.json `weathers`): rate, and kg kept a kg
// (half a mole of kaolinite a mole of feldspar, by their molar masses)
const fix OLIVINE = ONE, PYROXENE = ratio(60, 100), PLAGIOCLASE = ratio(40, 100), FELDSPAR = ratio(15, 100),
		MICA = ratio(8, 100), CALCITE = ONE, LASTS = 0;
const fix KEEP_PLAGIOCLASE = ratio(492, 1000), KEEP_FELDSPAR = ratio(464, 1000), KEEP_MICA = ratio(972, 1000);

#define MINERAL(id, share, rate, keep) ParentMineral{ id, ratio(share, 100), rate, keep, -1 }
#define QUARTZ(share) MINERAL("silica", share, LASTS, 0)
#define K_FELDSPAR(share) MINERAL("feldspar", share, FELDSPAR, KEEP_FELDSPAR)
#define NA_FELDSPAR(share) MINERAL("plagioclase", share, PLAGIOCLASE, KEEP_PLAGIOCLASE)
#define MICAS(share) MINERAL("mica", share, MICA, KEEP_MICA)
#define PYROXENES(share) MINERAL("pyroxene", share, PYROXENE, 0)
#define OLIVINES(share) MINERAL("olivine", share, OLIVINE, 0)
#define CALCITES(share) MINERAL("calcite", share, CALCITE, 0)
#define KAOLINITE(share) ParentMineral{ "kaolinite", ratio(share, 100), LASTS, 0, CLAY }
const ParentRock PARENTS[(int)Rock::Count] = {
	{ { QUARTZ(30), K_FELDSPAR(55), MICAS(15) }, SAND, ONE },                    // granite
	{ { K_FELDSPAR(50), QUARTZ(30), MICAS(20) }, SAND, ratio(8, 10) },           // gneiss
	{ { MICAS(50), QUARTZ(30), KAOLINITE(20) }, SILT, ratio(7, 10) },            // slate
	{ { PYROXENES(45), NA_FELDSPAR(45), OLIVINES(10) }, SAND, ratio(7, 10) },    // basalt
	{ { NA_FELDSPAR(60), PYROXENES(30), QUARTZ(10) }, SAND, ratio(8, 10) },      // andesite
	{ { QUARTZ(60), K_FELDSPAR(30), NA_FELDSPAR(10) }, SILT, toFix(3) },         // tuff
	{ { QUARTZ(75), K_FELDSPAR(25) }, SAND, HALF },                              // obsidian
	{ { QUARTZ(90), K_FELDSPAR(10) }, SAND, toFix(3) },                          // sandstone
	{ { KAOLINITE(60), QUARTZ(30), CALCITES(10) }, SILT, toFix(4) },             // shale
	{ { CALCITES(90), KAOLINITE(6), QUARTZ(4) }, SILT, ONE },                    // limestone
	{ { NA_FELDSPAR(90), PYROXENES(10) }, SAND, ONE },                           // anorthosite
	{ { NA_FELDSPAR(60), PYROXENES(25), OLIVINES(10), QUARTZ(5) }, GRAVEL, toFix(2) }, // breccia
	{ {}, STONE, ONE },                                                          // ice: nothing to weather
};

// A rock weathered by `extent`: each band's mass, as shares of the rock's
// (what dissolved is gone). Its minerals change by e^(−extent·rate); what's
// left falls apart into its crystals by 1 − e^(−extent·crumbles), the rest still
// stones (matter::Grains::weather, in fixed point).
void weathered(const ParentRock &p, fix extent, fix bands[GRAIN_BANDS])
{
	for (int b = 0; b < GRAIN_BANDS; b++)
		bands[b] = 0;
	const fix apart = ONE - exp(-mul(extent, p.crumbles));
	for (const ParentMineral &m : p.minerals) {
		if (!m.id)
			continue;
		fix left = m.share;
		if (m.rate > 0) {
			left = mul(m.share, exp(-mul(extent, m.rate)));
			bands[CLAY] += mul(m.share - left, m.keep);
		}
		bands[std::max(p.crystals, m.finest)] += mul(left, apart);
		bands[GRAVEL] += left - mul(left, apart);
	}
}

// What water leaves where it drops grains of a band: mostly that band,
// some of the next finer (sorting's never perfect), as shares by band.
void deposit(int band, fix out[GRAIN_BANDS])
{
	static const fix MIX[GRAIN_BANDS][GRAIN_BANDS] = {
		{ ratio(45, 100), ratio(30, 100), ratio(20, 100), ratio(5, 100), 0 },  // boulders and cobbles
		{ ratio(10, 100), ratio(50, 100), ratio(32, 100), ratio(8, 100), 0 },  // a gravel bed
		{ 0, ratio(5, 100), ratio(83, 100), ratio(10, 100), ratio(2, 100) },  // a sand bed, a bar
		{ 0, 0, ratio(20, 100), ratio(60, 100), ratio(20, 100) },             // silt: the levees, the plain
		{ 0, 0, ratio(5, 100), ratio(35, 100), ratio(60, 100) },              // clay: backswamps, lake floors
	};
	for (int b = 0; b < GRAIN_BANDS; b++)
		out[b] = MIX[band][b];
}

// log₂ of a positive 64-bit fixed value, as a fix
fix log2Wide(int64_t v)
{
	int top = 63 - __builtin_clzll((uint64_t)v);
	const fix m = (fix)(top >= FRAC ? v >> (top - FRAC) : v << (FRAC - top)); // 1..2
	return log2(m) + toFix(top - FRAC);
}

/*
	The band a channel's bed keeps, from the largest grain it moves (Shields): a grain
	moves once the flow's drag, ρ·g·depth·slope, passes 0.045 of its weight
	in water, so D ≈ 13.5 · depth · slope (metres). The depth is the river's
	at its usual flow (Leopold: 0.27 · Q^0.4, Q from what drains to it at a
	hundredth of a cubic metre a second a square kilometre of rain), the
	area from its width (World::riverWidth), all in the planet's own metres.
*/
struct Current
{
	int bed;       // the band its bed keeps
	int64_t flood; // how high its floods stand over its bed: twice its depth (blocks, fixed)
};
Current currentOf(int64_t width, fix slope, fix rain, int64_t radius, fix metre)
{
	// R/R⊕, as log₂: how many of our blocks a real metre across is
	const fix scale = log2Wide(radius) - log2Wide((int64_t)EARTH_RADIUS_M << FRAC);
	// √area (blocks) = width · blocks-per-metre / 0.0012
	const fix root = log2Wide(std::max<int64_t>(width, ONE)) + log2(metre) - log2(ratio(12, 10000));
	const fix area_km2 = 2 * (root - scale) - log2Wide((int64_t)1000000 << FRAC);
	const fix flow = area_km2 + log2(ratio(1, 100)) + log2(clamp(rain, ratio(5, 100), toFix(8)));
	const fix depth = log2(ratio(27, 100)) + mul(flow, ratio(4, 10));
	// the slope in real terms: its rise in metres (blocks / metre) over its
	// run in metres (blocks · R⊕/R)
	const fix real = log2(std::max<fix>(slope, 1)) + scale - log2(metre);
	const fix mm = log2(ratio(135, 10)) + depth + real + log2(toFix(1000));
	// What it can move isn't all it keeps: a river wears its pebbles to sand
	// on the way down, and where it can't move pebbles of 16–32 mm its
	// gravel's gone (the gravel–sand transition), a sand bed even under a
	// current that could still roll gravel. Cobbles and boulders where it
	// moves grains of 128 mm; silt only where it barely moves sand.
	const int bed = mm >= toFix(7) ? STONE : mm >= toFix(5) ? GRAVEL : mm >= -ratio(432, 100) ? SAND : SILT;
	return { bed, (int64_t)mul(exp2(std::min(depth, toFix(8))), 2 * metre) };
}
} // namespace

bool braids(int64_t width, fix slope, fix rain, int64_t radius, fix metre, fix glacial)
{
	// (Q and the slope as currentOf has them, log₂)
	const fix scale = log2Wide(radius) - log2Wide((int64_t)EARTH_RADIUS_M << FRAC);
	const fix root = log2Wide(std::max<int64_t>(width, ONE)) + log2(metre) - log2(ratio(12, 10000));
	const fix area_km2 = 2 * (root - scale) - log2Wide((int64_t)1000000 << FRAC);
	// (its floods' flow, a few times its usual, is what shapes its bed)
	const fix flow = area_km2 + log2(ratio(5, 100)) + log2(clamp(rain, ratio(5, 100), toFix(8)));
	const fix real = log2(std::max<fix>(slope, 1)) + scale - log2(metre);
	// (the line ten times Leopold and Wolman's: the grid's runs fall over a
	// cell, steeper than a reach of river does, and at theirs half of every
	// world's rivers braided; at this about one in nine, mostly below ranges
	// and the ice, as on Earth)
	const fix line = log2(ratio(120, 1000)) - mul(flow, ratio(44, 100)) - mul(glacial, log2(toFix(3)));
	return real > line;
}

const char *const SOIL_PARENT_NAMES[(int)SoilParent::Count] = { "granitic", "basaltic", "limy", "shaly" };
const char *const SOIL_STONY_NAMES[SOIL_STONY] = { "", "stony", "gravelly", "gravel" };

SoilParent soilParent(Rock rock)
{
	switch (rock) {
	case Rock::Basalt:
	case Rock::Andesite:
	case Rock::Breccia: return SoilParent::Basaltic;
	case Rock::Limestone: return SoilParent::Limy;
	case Rock::Shale:
	case Rock::Slate: return SoilParent::Shaly;
	default: return SoilParent::Granitic; // granite, gneiss, sandstone, tuff, obsidian, anorthosite: quartz and feldspar
	}
}

int soilStony(fix stones)
{
	return stones < ratio(15, 100) ? 0 : stones < ratio(35, 100) ? 1 : stones < ratio(60, 100) ? 2 : 3;
}

fix soilStonesOf(int stony)
{
	static const fix SHARE[SOIL_STONY] = { ratio(5, 100), ratio(25, 100), ratio(47, 100), ratio(75, 100) };
	if (stony < 0 || stony >= SOIL_STONY)
		throw std::invalid_argument("no stony class " + std::to_string(stony));
	return SHARE[stony];
}

Soil soilLayer(const Soil &column, int32_t below)
{
	Soil out = column;
	if (below <= 0)
		return out;
	const bool peat = column.organic >= ratio(30, 100);
	if (!peat)
		out.organic = column.organic / 4;
	if (column.laid)
		return out;
	if (below >= column.depth - 1 && column.depth >= 3) {
		// over the rock: the rock breaking up
		out.stones = maxx(column.stones, ratio(50, 100));
		out.organic = 0;
		return out;
	}
	// the clay rain carries down out of the topsoil
	const fix moved = std::min(ratio(6, 100), column.sand + column.silt);
	const fix from_sand = column.sand + column.silt > 0 ? (fix)((int64_t)moved * column.sand / (column.sand + column.silt)) : 0;
	out.clay += moved;
	out.sand -= from_sand;
	out.silt -= moved - from_sand;
	return out;
}

const ParentRock &parentRock(Rock rock)
{
	return PARENTS[(int)rock];
}

uint8_t soilIndex(const Soil &s)
{
	fix mineral = s.sand + s.silt + s.clay;
	if (mineral <= 0)
		mineral = ONE;
	int32_t sand = std::clamp(roundInt(div(s.sand, mineral) * 10), 0, 10);
	int32_t clay = std::clamp(roundInt(div(s.clay, mineral) * 10), 0, 10 - sand);
	int32_t level = 0;
	for (int32_t k = 1; k < SOIL_ORGANIC; k++)
		if (fabsx(s.organic - ORGANIC_LEVELS[k]) < fabsx(s.organic - ORGANIC_LEVELS[level]))
			level = k;
	return (uint8_t)(level * TRIANGLE + triangle(sand, clay));
}

Soil soilPoint(uint8_t index)
{
	if (index >= SOIL_POINTS)
		throw std::invalid_argument("no soil point " + std::to_string(index));
	int32_t level = index / TRIANGLE, rest = index % TRIANGLE, sand = 0;
	while (triangle(sand + 1, 0) <= rest)
		sand++;
	int32_t clay = rest - triangle(sand, 0);
	return { ratio(sand, 10), ratio(10 - sand - clay, 10), ratio(clay, 10), ORGANIC_LEVELS[level] };
}

Soil World::soil(const Place &at, const Ground &ground, const Strata &strata) const
{
	// the channel nearest, if the drainage is drawn below the grid
	return soilWith(at, ground, strata, m_channels ? &channelsAt(at).read : nullptr);
}

const World::RegionalSoil &World::regionalSoil() const
{
	std::call_once(m_regional_soiled, [&] {
		const Grid &g = *m_grid;
		const int32_t n = g.size();
		RegionalSoil &r = m_regional_soil;
		for (std::vector<fix> *f : { &r.sand, &r.silt, &r.clay, &r.organic, &r.stones })
			f->assign((size_t)n, 0);
		parallelFor(n, [&](int32_t c) {
			if (g.base[c] < 0)
				return;
			// the cell's centre as the grid has it: its height, its climate, a lake's water
			const Place at(gridSphere().centre(c));
			Ground ground{};
			ground.column = column(at, sunlight(at, std::nullopt));
			ground.terrain = (int32_t)(gridHeight(at) >> FRAC);
			ground.water = ground.terrain;
			ground.kind = g.lake[c] ? Water::Lake : Water::None;
			const Soil s = soilWith(at, ground, strata(at, ground), nullptr);
			r.sand[c] = s.sand;
			r.silt[c] = s.silt;
			r.clay[c] = s.clay;
			r.organic[c] = s.organic;
			r.stones[c] = s.stones;
		});
	});
	return m_regional_soil;
}

Soil World::soilWith(const Place &at, const Ground &ground, const Strata &strata, const Channels::Read *read) const
{
	const Column &c = ground.column;
	const Cover cover = this->cover(at, ground);
	Soil out{};
	out.parent = rock(strata, 0, ground.terrain);
	const int32_t cell = this->cell(at);
	const fix wobble = noise(at, m_code.seed_hash + SOIL_NOISE, m_code.size / 48); // −1..1

	const fix slope = read ? read->slope : cover.slope;
	const fix relief = read ? read->relief : 0; // −1 valley floor .. 1 ridge

	// how far weathering has gone: the climate's rate, the time it's had
	const fix celsius = std::max(celsiusOf(c.heat), toFix(-25));
	const fix rate = mul(exp2((celsius - toFix(15)) / 10), clamp(c.rain, ratio(5, 100), toFix(4)));
	fix time = HALF + 2 * between(m_grid->tectonics.stable, at);
	time = mul(time, ONE - mul(ratio(8, 10), glacial(cell, (int64_t)ground.terrain << FRAC)));
	time = div(time, ONE + 2 * std::min(slope, toFix(4)));
	const fix extent = mul(mul(EXTENT, rate), mul(time, ONE + wobble / 4));
	fix bands[GRAIN_BANDS];
	weathered(parentRock(out.parent), extent, bands);

	// slopes: steep ground and ridges lose their fines, hollows gather them
	const fix washed = clamp(mul(slope - ratio(25, 100), ratio(6, 10)) + mul(maxx(relief, 0), ratio(2, 10)), 0, ratio(8, 10));
	const fix gathered = mul(maxx(-relief, 0), HALF);
	for (int b = SILT; b <= CLAY; b++)
		bands[b] = mul(bands[b], mul(ONE - washed, ONE + gathered));
	bands[SAND] = mul(bands[SAND], ONE - washed / 2);

	// water: what the rivers and lakes laid here, sorted by size. The
	// nearest water says how wide it is, how far off its line this is, how
	// high over it, and how steeply it runs: a grid river's (ground()'s) or
	// a stream's below the grid (the channels'), whichever's bank is nearer
	fix laid = 0;
	fix sorted[GRAIN_BANDS] = {};
	int64_t width = 0, gap = INT64_MAX, clear = INT64_MAX;
	fix grade = slope;
	if (ground.river_width > 0) {
		width = ground.river_width;
		gap = ground.river_dist;
		clear = (int64_t)(ground.terrain - ground.river_level) << FRAC;
		grade = ground.river_slope;
	}
	const bool in_river = width > 0 && gap <= width / 2; // a grid river's own channel: it's what runs here
	if (read && read->water_width > 0 && !in_river && (width == 0 || read->water_gap - read->water_width / 2 < gap - width / 2)) {
		width = read->water_width;
		gap = read->water_gap;
		clear = read->clear;
		grade = read->slope;
	}
	const bool lake = ground.kind == Water::Lake || (read && read->pond);
	const bool channel = ground.kind == Water::River || (read && read->wash) || (width > 0 && gap <= width / 2);
	if (channel && width == 0) // a world too small for channels below the grid: the cell's river
		width = riverWidth((m_grid->drained[cell] * (m_cell_blocks >> FRAC) >> FRAC) * (m_cell_blocks >> FRAC));
	const Current flow = currentOf(std::max<int64_t>(width, ONE), grade, c.rain, m_radius, m_metre);
	const int bed = flow.bed;
	const int64_t half = width / 2;
	const int64_t bank = half + std::max<int64_t>(width / 2, 2 * ONE);
	const int64_t flood = std::max<int64_t>(ONE / 2, flow.flood);
	const int64_t plain = half + 6 * std::max<int64_t>(width, 8 * ONE);
	if (lake) {
		deposit(CLAY, sorted);
		laid = ONE;
	} else if (channel) {
		deposit(bed, sorted);
		laid = ONE;
	} else if (ground.river_bar || (width > 0 && gap <= bank && (ground.river_bend > 0 || ground.river_braided))) {
		// the bar inside a bend (and a braided river's banks, bars all
		// across): what drops first as the water slows, a size finer than
		// the bed (sand by a gravel bed). Outside a bend the water cuts
		// the bank: what's there is the floodplain it cuts into, below
		deposit(std::min(bed + 1, (int)CLAY), sorted);
		laid = ONE;
	} else if (width > 0 && clear <= flood && gap <= plain) {
		// (the cut bank's too: the floodplain's silt, cut through)
		// the floodplain: silt by the river (the levee), clay out where floods stand still
		const fix out_t = (fix)std::clamp<int64_t>(((gap - bank) << FRAC) / std::max<int64_t>(1, plain - bank), 0, ONE);
		fix silt[GRAIN_BANDS] = {}, clay[GRAIN_BANDS] = {};
		deposit(SILT, silt);
		deposit(CLAY, clay);
		for (int b = 0; b < GRAIN_BANDS; b++)
			sorted[b] = lerp(silt[b], clay[b], out_t);
		laid = ONE;
	} else if (!read && cover.bank && c.elevation < ratio(15, 100)) {
		// low ground in a river's or lake's cell, with nothing finer to say: its plain, on average
		deposit(SILT, sorted);
		laid = HALF;
	}
	// till: what the ice dropped, unsorted
	if (read && read->ice > 0 && laid == 0) {
		const fix till[GRAIN_BANDS] = { ratio(15, 100), ratio(20, 100), ratio(30, 100), ratio(25, 100), ratio(10, 100) };
		for (int b = 0; b < GRAIN_BANDS; b++)
			sorted[b] = till[b];
		laid = mul(read->ice, ratio(7, 10));
	}
	// and the grid's sediment where nothing finer says (fans, filled lakes, deltas)
	const fix fan = alluvium(between(m_grid->sediment, at));
	if (laid == 0 && fan > 0) {
		deposit(SILT, sorted);
		laid = fan / 2;
	}
	if (laid > 0) {
		// weathered grains are shares of the rock (some dissolved): compare like with like
		fix total = 0;
		for (int b = 0; b < GRAIN_BANDS; b++)
			total += bands[b];
		for (int b = 0; b < GRAIN_BANDS; b++)
			bands[b] = lerp(total > 0 ? div(bands[b], total) : 0, sorted[b], laid);
	}

	fix fine = bands[SAND] + bands[SILT] + bands[CLAY], all = fine + bands[STONE] + bands[GRAVEL];
	if (fine > 0) {
		out.sand = div(bands[SAND], fine);
		out.silt = div(bands[SILT], fine);
		out.clay = ONE - out.sand - out.silt;
	}
	out.stones = all > 0 ? div(bands[STONE] + bands[GRAVEL], all) : ONE;
	out.laid = laid >= HALF;

	// organic matter: what plants leave, over how fast it rots (twice as fast
	// every 10 °C, nearly stopping as the ground freezes; barely at all under
	// water)
	const fix rots = celsius > 0 ? exp2((celsius - toFix(10)) / 10) : std::max(exp2(-ONE + celsius / 4), ratio(2, 100));
	const bool waterlogged = c.wet > 0 && laid == ONE && slope < ratio(1, 10) && sorted[CLAY] >= HALF;
	fix organic = div(mul(cover.density, ratio(8, 100)), rots);
	if (waterlogged || c.biome == Biome::Marsh)
		organic += div(ratio(30, 100), rots);
	out.organic = clamp(organic, 0, ratio(60, 100));

	// depth: deep where weathering's gone far and on what water laid, thin on
	// slopes and ridges
	const fix made = ONE - exp(-extent / 6);
	fix depth = div(ONE + 5 * made, ONE + 2 * std::min(slope, toFix(4)));
	depth = mul(depth, ONE - mul(maxx(relief, 0), ratio(6, 10)) + gathered);
	depth = lerp(depth, toFix(6), laid);
	out.depth = std::clamp(roundInt(depth), 0, 8);
	return out;
}

uint8_t World::blend(const Place &d, int32_t y) const
{
	// smooth patches a few dozen blocks across, through depth too
	const std::array<int32_t, 3> p = solid(d, y);
	fix n = value3(p[0], p[1], p[2], m_code.seed_hash + BLEND, 24); // −1..1
	return (uint8_t)std::clamp(floorInt(mul(n + ONE, toFix(8))), 0, ROCK_BLENDS - 1);
}

} // namespace terragen
