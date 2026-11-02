// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world.h"
#include "climate.h"
#include "planet.h"
#include "plates.h"
#include "noise.h"
#include "sun.h"
#include <algorithm>
#include <cmath>

namespace terragen {

enum MareStream : uint32_t { WRINKLE = 239, RILLE = 241, DOME = 251, RAY = 257 };

const char *const BIOME_NAMES[(int)Biome::Count] = {
	"deep", "ocean", "seaice", "beach", "desert", "savanna", "grassland",
	"forest", "jungle", "taiga", "tundra", "rock", "snow", "monsoon", "mediterranean", "regolith", "mare",
};

// Real land's roughness: amplitude ∝ wavelength^0.8, so each octave (half
// the wavelength) 2^−0.8 of the last
static const fix DETAIL_SLOPE = ratio(8, 10);
static const fix DETAIL_PERSISTENCE = ratio(574, 1000);
// (an fbm's typical swing is a third of its range: this is its range)
static const int32_t DETAIL_M = 1500;

// Each octave of the detail is read through one more turn of this rotation
// (a radian about (1, 2, 3), at 2³⁰), so the octaves' lattices never line
// up: alone they share the axes, and their cells show as squares
static const int64_t TURN[3][3] = {
	{ 615402086, -653915903, 588723848 },
	{ 794943514, 721172795, -29935152 },
	{ -377182430, 453017987, 897457310 },
};

static Place turned(const Place &p)
{
	auto row = [&](int k) { return (int32_t)((TURN[k][0] * p.x + TURN[k][1] * p.y + TURN[k][2] * p.z) >> 30); };
	return Place(row(0), row(1), row(2));
}

// The detail's noise, −1..1: rolling (fbm) and ridged (each octave folded at
// its middle: sharp crests, V-shaped valleys), mixed `ridges` of the way
static fix detailNoise(Place d, uint32_t seed, int32_t frequency, int octaves, fix ridges)
{
	fix sum = 0, amp = ONE, norm = 0;
	for (int o = 0; o < octaves; o++) {
		const fix n = noise(d, seed + (uint32_t)o * 1013u, frequency);
		sum += mul(amp, ridges > 0 ? n + mul(ridges, ONE - 2 * std::abs(n) - n) : n);
		norm += amp;
		amp = mul(amp, DETAIL_PERSISTENCE);
		frequency *= 2;
		d = turned(d);
	}
	return div(sum, norm);
}

World::World(const WorldCode &code) :
	m_code(code), m_grid(gridFor(code)), m_radius(radiusOf(code)),
	m_cell_blocks(cellBlocksOf(code)),
	// rivers' width: by the planet's size against the default's (16 384 round), never narrower
	m_river_scale(std::max<int64_t>(ONE, ((int64_t)code.size << FRAC) / 16384)),
	// meanders: about a wave a cell, counted round the equator as noise is
	m_meander((int32_t)std::max<int64_t>(8, ((int64_t)TWO_PI << FRAC) / gridSphere().spacing() >> FRAC)),
	m_height(heightScale(code)),
	m_metre(blocksPerMetre(code)),
	m_craters(code, m_radius, blocksPerMetre(code))
{
	// The detail between cells: real land's roughness falls as wavelength^0.8,
	// DETAIL_M at 4 km in a rising range; here from a cell's wavelength (in the
	// body's metres) down to 16 blocks, in blocks
	const fix s = blocksPerMetre(code);
	const int64_t cell_m = (m_cell_blocks << FRAC) / s; // fixed, 64 bits
	const fix top = pow((fix)std::max<int64_t>(1, cell_m / 4000), DETAIL_SLOPE);
	m_detail = mul(mul(toFix(DETAIL_M), top), s);
	m_detail_octaves = 1;
	for (int64_t w = m_cell_blocks >> FRAC; w > 32; w /= 2)
		m_detail_octaves++;
	// rays fade within a billion years or so (space weathering darkens them):
	// of the craters made after the lava, the wide ones a third of the time
	for (size_t k = 0; k < m_grid->craters.size(); k++) {
		const Crater &c = m_grid->craters[k];
		if (c.young && c.complex && plates::mix((uint32_t)k, RAY, m_code.seed_hash) % 3 == 0)
			m_rayed.push_back(c);
	}
}

fix blocksPerMetre(const WorldCode &code)
{
	// √ of R / R⊕, with 16 bits more than a fix holds (the smallest world's
	// ratio is 27 / 65 536 as a fix)
	const int64_t ratio = std::min<int64_t>((int64_t)ONE << FRAC, (World::radiusOf(code) << FRAC) / EARTH_RADIUS_M);
	return sqrtWide64(ratio) >> 8;
}

int64_t World::radiusOf(const WorldCode &code)
{
	return ((int64_t)code.size << (2 * FRAC)) / TWO_PI;
}

int64_t World::cellBlocksOf(const WorldCode &code)
{
	return blocksOf(gridSphere().spacing(), radiusOf(code));
}

Place World::place(fix u, fix v)
{
	return dirAt(mul(HALF - v, PI), mul(u - HALF, TWO_PI));
}

fix World::u(const Dir &d)
{
	fix u = div(longitudeOf(d), TWO_PI) + HALF;
	return u >= ONE ? u - ONE : u < 0 ? u + ONE : u;
}

fix World::v(const Dir &d)
{
	return HALF - div(latitudeOf(d), PI);
}

int32_t World::blocksBetween(const Place &a, const Place &b) const
{
	return (int32_t)(blocksFine(chord(a, b)) >> FRAC);
}

std::array<int32_t, 3> World::solid(const Place &d, int32_t y) const
{
	const int64_t r = m_radius + toFix(y); // blocks from the centre, fixed
	// rounded: (c · r) / 2^(30 + 16), half up
	auto at = [&](int32_t c) { return (int32_t)((mulShift(c, r, PLACE_FRAC + FRAC - 1) + 1) >> 1); };
	return { at(d.x), at(d.y), at(d.z) };
}

fix World::sunlight(const Place &d, std::optional<fix> date) const
{
	return terragen::sunlight(m_code, v(d.dir()), date);
}

/*
	Land biomes by climate, as data: the first niche a column fits. Heat
	(−1..1), wetness (−1..1, from the year's rain) and seasonality (1: the
	rain all falls in summer, −1: all in winter). Monsoon forest needs hot
	land whose rain comes in summer; Mediterranean scrub, mild land whose
	rain comes in winter. Hot land with a dry season and less rain is savanna.
*/
struct Niche
{
	fix heat_lo, heat_hi, wet_lo, wet_hi, season_lo, season_hi;
	Biome biome;
};

const fix ANY = toFix(9);

const Niche NICHES[] = {
	{ -ANY, -ratio(30, 100), 0, ANY, -ANY, ANY, Biome::Taiga },
	{ -ANY, -ratio(30, 100), -ANY, ANY, -ANY, ANY, Biome::Tundra },
	{ ratio(20, 100), ANY, ratio(5, 100), ANY, ratio(55, 100), ANY, Biome::Monsoon },
	{ ratio(35, 100), ANY, ratio(10, 100), ANY, -ANY, ratio(55, 100), Biome::Jungle },
	{ ratio(30, 100), ANY, -ratio(55, 100), ANY, ratio(25, 100), ANY, Biome::Savanna }, // a wet season, then a dry one
	{ ratio(19, 100), ANY, -ANY, -ratio(15, 100), -ANY, ANY, Biome::Desert }, // hot desert from an 18 °C year (Köppen's BWh)
	{ ratio(35, 100), ANY, -ANY, ANY, -ANY, ANY, Biome::Savanna },
	{ 0, ratio(35, 100), -ratio(60, 100), ratio(40, 100), -ANY, -ratio(30, 100), Biome::Mediterranean },
	{ -ANY, ANY, ratio(5, 100), ANY, -ANY, ANY, Biome::Forest },
	{ -ANY, ANY, -ANY, ANY, -ANY, ANY, Biome::Grassland },
};

// A lifeless body's one biome, by its bare ground (Surface; Soil never bare)
static const Biome BARE[] = { Biome::Rock, Biome::Rock, Biome::Desert, Biome::Snow, Biome::Regolith };
// …and on the maria's lava (dark regolith where it's ground to dust)
static const Biome FLOODED[] = { Biome::Rock, Biome::Rock, Biome::Desert, Biome::Snow, Biome::Mare };

static Biome classify(fix e, fix heat, fix wet, fix season)
{
	if (e < 0)
		return heat < -ratio(75, 100) ? Biome::SeaIce : e < -ratio(18, 100) ? Biome::Deep : Biome::Ocean;
	if (e < ratio(3, 100))
		return heat < -ratio(60, 100) ? Biome::Snow : Biome::Beach;
	if (heat < -ratio(60, 100))
		return Biome::Snow;
	if (e > ratio(50, 100))
		return heat < -ratio(10, 100) ? Biome::Snow : Biome::Rock;
	for (const Niche &n : NICHES)
		if (heat >= n.heat_lo && heat < n.heat_hi && wet >= n.wet_lo && wet < n.wet_hi && season >= n.season_lo && season < n.season_hi)
			return n.biome;
	return Biome::Grassland; // the last niche takes everything
}

// Coasts by their rock (sprint 97): how hard each province's rock is to the
// sea. Old shields and ranges' cores stand as cliffs; sediment basins, rifts
// and young volcanic ground give way to beaches and coves.
static const fix HARD[(int)Province::Count] = {
	ONE,               // shield
	ratio(85, 100),    // orogen
	ratio(60, 100),    // arc
	ratio(30, 100),    // rift
	0,                 // basin
	ratio(45, 100),    // volcanic
	0,                 // ocean floor
	ratio(70, 100),    // highland
	ratio(50, 100),    // mare
};

fix World::hardness(const Dir &d) const
{
	const Grid &g = *m_grid;
	const Sphere::Spot spot = gridSphere().spot(d);
	fix h = 0;
	for (int k = 0; k < 3; k++)
		h += mul(HARD[(int)g.province[spot.cells[k]]], spot.weights[k]);
	// sand and silt laid along it bury the rock
	return mul(h, ONE - alluvium(between(g.sediment, d)));
}

int32_t World::cell(const Dir &d) const
{
	return gridSphere().cell(d);
}

void World::volcanic(const Place &at, int32_t &raise, int32_t &lava, int32_t &floor) const
{
	const Grid &g = *m_grid;
	raise = 0;
	lava = INT32_MIN;
	floor = INT32_MIN;
	int32_t i = cell(at);
	for (uint32_t k = g.volcano_start[i]; k < g.volcano_start[i + 1]; k++) {
		const Volcano &vo = g.volcanoes[g.volcano_list[k]];
		const int64_t far = blocksFine(chord(at, Place(vo.at))); // blocks, fixed
		if (far >= toFix(vo.radius))
			continue;
		const fix d = (fix)far; // within its slopes: fits
		// the cone: (1 − d/r)^1.5, steeper near the top
		fix t = ONE - d / vo.radius;
		int32_t cone = floorInt(mul(toFix(vo.height), mul(t, sqrt(t))));
		if (d < toFix(vo.crater)) {
			// the crater: a bowl sunk below the rim
			fix r = div(d, toFix(vo.crater));
			fix rim_t = ONE - div(toFix(vo.crater), toFix(vo.radius));
			int32_t rim = floorInt(mul(toFix(vo.height), mul(rim_t, sqrt(rim_t))));
			int32_t depth = vo.crater * 3 / 4;
			// built from the vent's ground, so the rim is level and a lava lake sits flat in it
			floor = std::max(floor, vo.base + rim - floorInt(mul(toFix(depth), ONE - mul(r, r))));
			if (vo.active)
				lava = std::max(lava, vo.base + rim - depth / 2);
			continue;
		}
		raise = std::max(raise, cone);
	}
}

fix World::between(const std::vector<fix> &f, const Dir &d) const
{
	const Sphere::Spot spot = gridSphere().spot(d);
	int64_t sum = 0;
	for (int k = 0; k < 3; k++)
		sum += (int64_t)f[spot.cells[k]] * spot.weights[k];
	return (fix)(sum >> FRAC);
}

Column World::column(const Place &p, fix sun) const
{
	// the climate and the grid's fields: at cell scale
	const Dir d = p.dir();
	const WorldCode &c = m_code;
	const fix v = this->v(d);
	fix e = between(m_grid->base, d);
	if (e >= 0) // only the land wears down; the sea floor stays as made
		e = std::max<fix>(e + between(m_grid->worn, d), 0);
	// the year's heat here, the season's on top (by the sunlight above the
	// year's), cooler with height
	fix heat = between(m_grid->heat, d) + mul(sun - terragen::sunlight(c, v, std::nullopt), between(m_grid->heat_gain, d))
			- mul(maxx(0, e), LAPSE);
	// the climate's rain here, in each season; the hemisphere decides which is summer
	fix north = between(m_grid->rain_season[0], d), south = between(m_grid->rain_season[1], d);
	fix rain = (north + south) / 2;
	fix summer = v < HALF ? north : south, winter = v < HALF ? south : north;
	// (a dry body has no rain in either season, and no seasonality of it)
	fix season = summer + winter > 0 ? div(summer - winter, summer + winter) : 0;
	// as the grid reads it (grid.cpp): the seasons' fields are interpolated and
	// can dip to nothing in the driest places; below 1/100 it's as dry as dry
	// gets (−1) either way
	fix wet = clamp(log2(maxx(rain, ratio(1, 100))) / 2, -ONE, ONE);

	Biome biome = classify(e, heat, wet, season);
	// a beach only where sand gathers or the rock is soft; a hard coast is
	// cliffs (ground()), its top the land's biome
	if (biome == Biome::Beach && c.processes.water && hardness(d) > HALF)
		biome = classify(ratio(3, 100), heat, wet, season);
	// a lifeless body: one bare biome over all of it, low ground and poles too
	if (!c.processes.life)
		biome = (between(m_grid->mare, d) > 0 ? FLOODED : BARE)[(int)c.processes.bare];
	return { e, heat, wet, rain, season, biome };
}


// The daily swing either side of the day's mean, in °C: 6 where the air is
// average, 3 where soaked, 9 where arid; half that over water.
static fix dailySwing(const Column &c)
{
	fix swing = toFix(6) - c.wet * 3;
	return c.elevation < 0 ? swing / 2 : swing;
}

fix World::temperature(const Place &d, fix date, fix hour) const
{
	Column c = column(d, sunlight(d, date));
	// warmest at 15:00, coldest at 03:00
	fix daily = mul(dailySwing(c), cos(mul(TWO_PI, hour - ratio(5, 8))));
	return celsiusOf(c.heat) + daily;
}

int32_t World::surface(fix e, const Place &d) const
{
	// The coast follows the land between cell centres, which alone would
	// draw it in the cells' straight edges: it wanders by noise about a cell
	// long (a few blocks of height, so flat coasts wander far, cliffs not).
	fix shore = fbm(d, m_code.seed_hash + SHORE, m_meander, 5, HALF);
	fix base = mul(e, m_height) + shore * 3;
	// The fine detail fades to nothing at the waterline (never more than half
	// the height over or under it), so it never flips land and sea column by
	// column: the coastline is the base's, a smooth line.
	// (rougher where ranges are rising and with the Roughness setting;
	// gentler under the sea)
	// plains rolling (a tenth of a range's), rising ranges ridged
	const fix belt = between(m_grid->tectonics.belt, d);
	const fix range = clamp(div(belt - ratio(18, 100), ratio(62, 100)), 0, ONE);
	fix detail = detailNoise(d, m_code.seed_hash + DETAIL, m_meander, m_detail_octaves, range);
	fix rugged = mul(ratio(1, 10) + mul(range, ratio(9, 10)), HALF + m_code.roughness);
	if (base < 0)
		rugged = rugged * 2 / 5;
	// lava set level: the maria are smooth plains (deeper than 125 m, a tenth)
	const fix lava = clamp(div(between(m_grid->mare, d), ratio(1, 40)), 0, ONE);
	rugged = mul(rugged, ONE - mul(lava, ratio(9, 10)));
	fix amplitude = std::min<fix>(mul(m_detail, rugged), std::abs(base) / 2);
	return floorInt(base + mul(detail, amplitude));
}


// Rivers: segments from each river cell's centre to the centre of the cell it
// drains into. These say how wide, and how high the banks stand over the water.
static const int32_t BANK = 6;   // blocks beside the channel kept above the water
static const fix MAX_WIDTH = toFix(24);

fix World::craters(const Place &at, Age age) const
{
	const Grid &g = *m_grid;
	const int32_t i = cell(at);
	fix sum = m_craters.height(at, g.crater_from, age);
	for (uint32_t k = g.crater_start[i]; k < g.crater_start[i + 1]; k++) {
		const Crater &c = g.craters[g.crater_list[k]];
		if (c.young == (age == Age::Young))
			sum += Craters::shape(c, at);
	}
	return sum;
}

// The lava's own relief (sprint 96; Watters & Johnson 2010, Head & Gifford
// 1980; papers only), each a share of the mares' noise: wrinkle ridges where
// the lava, cooling and sinking, buckled (sinuous, a few hundred metres
// high, a cell or so long); rilles, channels its flows cut (narrow, winding,
// a few tens of metres deep); domes, low shields over late vents.
static const int32_t CLIFF_M = 80, CLIFF_MIN = 6;
static const int32_t BEACH_TOP = 3; // blocks over the sea a beach reaches // a sea cliff's height: metres, and blocks at the least
static const int32_t WRINKLE_M = 300, RILLE_M = 250, DOME_M = 200;

int32_t World::mareRelief(const Place &at) const
{
	const uint32_t seed = m_code.seed_hash;
	fix out = 0;
	// a ridge along the zero line of a noise, where a slower one lets it
	const fix w = fbm(at, seed + WRINKLE, m_meander / 8, 4, HALF);
	if (noise(at, seed + WRINKLE + 1, m_meander / 16) > 0) {
		const fix x = std::max<fix>(0, ONE - div(std::abs(w), ratio(4, 100)));
		out += mul(toFix(WRINKLE_M), mul(x, x));
	}
	// a rille: narrower, longer, rarer, cut down
	const fix r = fbm(at, seed + RILLE, m_meander / 8, 4, HALF);
	if (noise(at, seed + RILLE + 1, m_meander / 16) > ratio(35, 100)) {
		const fix x = std::max<fix>(0, ONE - div(std::abs(r), ratio(1, 100)));
		out -= mul(toFix(RILLE_M), smooth(x));
	}
	// a dome where a fine noise peaks, in a few fields of them
	const fix d = noise(at, seed + DOME, m_meander / 2);
	if (d > ratio(70, 100) && noise(at, seed + DOME + 1, m_meander / 8) > ratio(20, 100)) {
		const fix x = div(d - ratio(70, 100), ratio(30, 100));
		out += mul(toFix(DOME_M), smooth(x));
	}
	return floorInt(mul(out, m_metre));
}

bool World::rayed(const Place &at) const
{
	for (size_t k = 0; k < m_rayed.size(); k++) {
		const Crater &c = m_rayed[k];
		const int64_t d = chord(at, c.at);
		if (d >= 10 * c.radius || d == 0)
			continue;
		if (d < c.radius * 3 / 2)
			return true; // its bright ejecta, all round
		// streaks: a noise of the way out from it, so a streak runs straight
		// out; fewer the further, out to ten radii
		const Place way = placeOf((int64_t)at.x - c.at.x, (int64_t)at.y - c.at.y, (int64_t)at.z - c.at.z);
		const fix r = (fix)((d << FRAC) / c.radius);
		const fix streak = fbm(way, m_code.seed_hash + RAY + (uint32_t)k, 40, 3, HALF);
		if (streak > ratio(30, 100) + div(r - ratio(15, 10), toFix(30)))
			return true;
	}
	return false;
}

int32_t World::terrain(const Place &at, fix elevation, int32_t &mare) const
{
	const Grid &g = *m_grid;
	mare = 0;
	if (g.crater_stamp.empty())
		return surface(elevation, at);
	// the pool's level, flat, from the flooded cells round the place (as a
	// lake's): read between cells it would slope and its edge would be the
	// cells' triangles
	const Sphere::Spot spot = gridSphere().spot(at.dir());
	fix level = INT32_MIN;
	for (int32_t i : spot.cells)
		if (g.mare[i] > 0)
			level = std::max(level, g.mare_level[i]);
	// the land as it was before the lava (the grid's cells hold it raised to
	// the level), without the young craters the grid holds, and the old
	// exactly, less what its cells say of them
	const fix lava = level == INT32_MIN ? 0 : between(g.mare, at);
	const fix young = between(g.crater_young, at);
	int32_t t = surface(elevation - lava - young, at) + floorInt(craters(at, Age::Old) - mul(between(g.crater_stamp, at), m_height));
	// the lava filled what was lower than its level, so its shore follows the land
	if (level != INT32_MIN) {
		const int32_t top = floorInt(mul(level, m_height));
		if (top > t) {
			mare = top - t;
			t = top;
			// and its own relief, fading in from the shore (rilles don't cut
			// through to the ground under it)
			const int32_t relief = std::max(mareRelief(at) * std::min(mare, 8) / 8, 1 - mare);
			mare += relief;
			t += relief;
		}
	}
	return t + floorInt(craters(at, Age::Young));
}

Ground World::ground(const Place &at, fix sun) const
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	Column c = column(at, sun);
	int32_t mare;
	Ground out{ terrain(at, c.elevation, mare), 0, Water::None, c, 0 };
	out.mare = mare;
	// a lifeless body's dust is dark where lava lies (the column's own guess is the cells')
	if (!m_code.processes.life)
		out.column.biome = (mare > 0 && !rayed(at) ? FLOODED : BARE)[(int)m_code.processes.bare];
	// volcanoes stand on the land (or rise from the sea floor to islands)
	int32_t raise, lava, floor;
	volcanic(at, raise, lava, floor);
	if (raise > 0)
		out.terrain += raise;
	if (floor != INT32_MIN) {
		out.terrain = std::max(out.terrain, floor);
		if (lava != INT32_MIN && lava > out.terrain && lava > 1) {
			out.water = lava; // an active crater: a lava lake
			out.kind = Water::Lava;
			return out;
		}
	}
	if (!m_code.processes.water)
		return out; // no water: its basins stand dry
	// a hard coast rises as a cliff: the land's first metres stood up three
	// times as steep, to CLIFF_M over the sea (so inland it's only lifted by
	// that, and the lift fades where the rock softens: no steps)
	if (out.terrain >= 1) {
		const int32_t cliff = std::max(CLIFF_MIN, floorInt(mul(toFix(CLIFF_M), m_metre)));
		const fix hard = clamp(div(hardness(at.dir()) - ratio(40, 100), ratio(40, 100)), 0, ONE);
		out.terrain += floorInt(hard * std::min(out.terrain * 3, cliff));
	}
	// the beach is the strand: a coastal plain past a few blocks over the
	// sea is the land's biome (rivers' and lakes' strands come below)
	if (out.column.biome == Biome::Beach && out.terrain > BEACH_TOP && m_code.processes.life)
		out.column.biome = classify(ratio(3, 100), out.column.heat, out.column.wet, out.column.season);
	if (out.terrain < 0) {
		out.kind = Water::Sea;
		out.water = 0;
	}

	// Lakes: the grid's lake mask, between cell centres. Inside the outline the
	// floor drops under the lake's level; on the rim the ground holds it in.
	// Lakes and rivers are read at a place a little way off, by smooth noise
	// (no steeper than this: steeper, the warp folds and a wide river breaks
	// into pools): alone, lakes' outlines would
	// be the cells' triangles and rivers the straight runs between centres.
	// Both use the same warp, so rivers still run into their lakes.
	constexpr int NEAR = 8; // fractional bits of a river position here (below)
	std::array<int64_t, 3> wander; // the warp in blocks, to 1/256
	Place warped = at;
	{
		std::array<int64_t, 3> w;
		for (int k = 0; k < 3; k++) {
			// a third of a cell in waves two cells long, a sixteenth in waves half a cell long
			const fix f = fbm(at, m_code.seed_hash + MEANDER + (uint32_t)k, m_meander / 2, 2, HALF) / 3
					+ fbm(at, m_code.seed_hash + MEANDER + 3 + (uint32_t)k, m_meander * 2, 2, HALF) / 16;
			w[k] = (int64_t)f * sphere.spacing();
			wander[k] = mulShift(f, m_cell_blocks, FRAC) >> (FRAC - NEAR);
		}
		warped = placeOf((int64_t)at.x + w[0], (int64_t)at.y + w[1], (int64_t)at.z + w[2]);
	}
	const Sphere::Spot spot = sphere.spot(warped.dir());
	fix mask = 0;
	for (int k = 0; k < 3; k++)
		mask += g.lake[spot.cells[k]] ? spot.weights[k] : 0;
	if (mask > 0)
		mask += fbm(at, m_code.seed_hash + SHORE + 1, m_meander * 4, 3, HALF) / 4;
	if (mask > 0) {
		int32_t level = 0;
		for (int32_t i : spot.cells)
			if (g.lake[i])
				level = std::max(level, waterBlocks(g.lake_level[i]));
		if (mask >= HALF) {
			int32_t depth = 1 + roundInt(mul(std::min(mask, ONE) - HALF, toFix(6)));
			out.terrain = std::min(out.terrain, level - depth);
			out.water = level;
			out.kind = Water::Lake;
		} else {
			out.terrain = std::max(out.terrain, level);
			// a soft shore is a strand of sand round the water (sprint 97)
			if (m_code.processes.life && out.terrain <= level + 1 && hardness(at.dir()) < HALF)
				out.column.biome = Biome::Beach;
		}
	}

	// Rivers: the nearest river segment within reach of this column, from
	// the cells within three spacings of it. Positions are blocks from this
	// column, straight through the planet (this near, the same as along it),
	// to 1/256 of a block: three spacings on a large planet are tens of
	// thousands of blocks, whose squares wouldn't fit 64 bits at 1/65 536.
	const int64_t reach = 3 * m_cell_blocks + toFix(4);
	const auto offset = [&](const Dir &d) {
		const Place c(d);
		const auto along = [&](int64_t u) { return blocksFine(u) >> (FRAC - NEAR); };
		return std::array<int64_t, 3>{ along((int64_t)c.x - at.x) - wander[0], along((int64_t)c.y - at.y) - wander[1],
			along((int64_t)c.z - at.z) - wander[2] };
	};
	int64_t best_gap = INT64_MAX; // blocks, fixed (64 bits: a far segment's gap doesn't fit 32)
	int32_t best_level = 0;
	int64_t best_half = 0;
	int64_t best_dist = 0;
	int64_t best_bar = 0; // how wide the point bar is here (blocks, fixed)
	int best_bend = 0; // the column's side of the bend: 1 inside (a bar), −1 outside (a cut bank), 0 on a straight
	// (in a lake or the sea, the river is the water there: no channel, no banks)
	const bool open = out.kind != Water::Lake && out.kind != Water::Sea;
	for (int32_t i : sphere.around(cell(at), [&](int32_t k) {
			return open && blocksFine(chord(Place(sphere.centre(k)), at)) <= reach; })) {
		int32_t r = g.receiver[i];
		// (a run from one lake cell to another is under the lake: none)
		if (!open || !g.stream(i) || r < 0 || (g.lake[i] && g.lake[r]))
			continue;
		const std::array<int64_t, 3> a = offset(sphere.centre(i)), b = offset(sphere.centre(r));
		const int64_t sx = b[0] - a[0], sy = b[1] - a[1], sz = b[2] - a[2];
		const int64_t len2 = (sx * sx + sy * sy + sz * sz) >> NEAR;
		if (len2 == 0)
			continue;
		// closest point on the segment to the origin (this column)
		const int64_t along = -((a[0] * sx + a[1] * sy + a[2] * sz) >> NEAR);
		fix t = (fix)std::clamp<int64_t>((along << FRAC) / len2, 0, ONE);
		const int64_t px = a[0] + ((sx * t) >> FRAC), py = a[1] + ((sy * t) >> FRAC), pz = a[2] + ((sz * t) >> FRAC);
		// A river's full width grows as √ of the flow (3 + 2√flow blocks on
		// the default planet); a larger planet's rivers drain more (its cells
		// are larger), so with the planet's size (in 64 bits; never wider
		// than a cell, the land it drains between).
		// A stream (below a river's flow) is one to five blocks across, on any
		// planet: what drains a hillside, not a cell. A river grows from
		// there to its full width over its first few times the threshold,
		// so the channel doesn't jump where one becomes the other.
		const int64_t flow = (g.drained[i] << FRAC) / g.river_threshold; // 1 = a river
		int64_t width = toFix(1) + 4 * std::min<int64_t>(flow, ONE);
		if (g.river(i)) {
			const int64_t full = std::min({ mulShift(toFix(3) + 2 * sqrtWide64(flow), m_river_scale, FRAC),
					mulShift(MAX_WIDTH, m_river_scale, FRAC), m_cell_blocks });
			width += mulShift(std::max<int64_t>(0, full - width), std::min<int64_t>(ONE, (flow - ONE) / 3), FRAC);
		}
		const int64_t half = width / 2;
		int bend = 0;
		int64_t bar = 0;
		int64_t dist = (int64_t)isqrt((uint64_t)(px * px + py * py + pz * pz)) << (FRAC - NEAR);
		if (t > 0 && t < ONE) {
			// Meanders: the channel swings to one side and back, in loops
			// about eleven widths long (as rivers do), a whole number of them
			// to the run so it's on the line at both ends and runs join. The
			// column's distance is then across the run, from the swung line.
			const int64_t len = (int64_t)isqrt((uint64_t)(len2 << NEAR)); // 1/256 blocks
			const int64_t loops = std::max<int64_t>(1, (len << (FRAC - NEAR)) / std::max<int64_t>(11 * width, 1));
			// (a fraction of a width and the run, so the bends never fold)
			const int64_t reach_side = std::min<int64_t>(3 * width / 2, (len << (FRAC - NEAR)) / (4 * loops));
			uint32_t hash = ((uint32_t)i * 0x9e3779b1u) ^ m_code.seed_hash;
			hash ^= hash >> 15;
			hash *= 0x85ebca77u;
			hash ^= hash >> 13;
			const int64_t swing = (reach_side * (int64_t)(32768 + (hash & 32767)) >> 16) * ((hash >> 16) & 1 ? 1 : -1);
			const int64_t side = mulShift(swing, sin((fix)((int64_t)PI * loops * t >> FRAC)), FRAC);
			// across: p against the run's side (run × up, the column's own up)
			const int64_t ux = at.x >> 16, uy = at.y >> 16, uz = at.z >> 16;
			const int64_t nx = (sy * uz - sz * uy) >> 14, ny = (sz * ux - sx * uz) >> 14, nz = (sx * uy - sy * ux) >> 14;
			const int64_t across = ((px * nx + py * ny + pz * nz) / len) << (FRAC - NEAR);
			dist = std::abs(across + side);
			// the line swung toward the outside of its bend: there the water
			// cuts the bank, on the inside it slows and lays its load down
			const int64_t rel = across + side;
			bend = rel == 0 || side == 0 ? 0 : (rel > 0) == (side > 0) ? 1 : -1;
			// a bar is a crescent: widest at the bend's apex, nothing where the line crosses back
			bar = reach_side > 0 ? width * std::abs(side) / reach_side : 0;
		}
		if (dist - half < best_gap) {
			best_gap = dist - half;
			best_half = half;
			best_dist = dist;
			best_bar = bar;
			best_bend = bend;
			int32_t from = waterBlocks(g.filled[i]) - 1;
			int32_t to = g.base[r] < 0 ? 0 : waterBlocks(g.filled[r]) - 1;
			best_level = std::max<int32_t>(0, floorInt(lerp(toFix(from), toFix(to), t)));
		}
	}
	// a point bar: inside a bend, as wide as the river, at the water's level
	// (gravel and sand: a beach of the river's)
	if (best_bend > 0 && best_dist >= best_half && best_dist < best_half + best_bar && out.kind == Water::None &&
			out.terrain > best_level) {
		out.terrain = best_level;
		if (m_code.processes.life)
			out.column.biome = Biome::Beach;
	} else if (best_gap < toFix(BANK)) {
		if (best_dist < best_half) {
			// the channel: a bed under the water, deepest mid-stream
			int32_t depth = 1 + (int32_t)std::min<int64_t>(3, (((best_half - best_dist) >> 1) + HALF) >> FRAC);
			out.terrain = std::min(out.terrain, best_level - depth);
			if (out.kind == Water::None || best_level > out.water) {
				out.water = best_level;
				out.kind = Water::River;
			}
		} else if (out.terrain < best_level + 1 + (best_bend < 0 ? 2 : 0)) {
			// the bank holds the river in; outside a bend it's cut steep
			out.terrain = best_level + 1 + (best_bend < 0 ? 2 : 0);
		}
	}
	// a bank or a lake's rim raised over the sea's level: dry ground, not sea
	if (out.kind == Water::Sea && out.terrain >= out.water)
		out.kind = Water::None;
	return out;
}

} // namespace terragen
