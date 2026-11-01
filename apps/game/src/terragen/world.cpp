// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world.h"
#include "climate.h"
#include "planet.h"
#include "noise.h"
#include "sun.h"
#include <algorithm>
#include <cmath>

namespace terragen {

const char *const BIOME_NAMES[(int)Biome::Count] = {
	"deep", "ocean", "seaice", "beach", "desert", "savanna", "grassland",
	"forest", "jungle", "taiga", "tundra", "rock", "snow", "monsoon", "mediterranean",
};

World::World(const WorldCode &code) :
	m_code(code), m_grid(gridFor(code)), m_radius(radiusOf(code)),
	m_cell_blocks(cellBlocksOf(code)),
	// rivers' width: by the planet's size against the default's (16 384 round), never narrower
	m_river_scale(std::max<int64_t>(ONE, ((int64_t)code.size << FRAC) / 16384)),
	// meanders: about a wave a cell, counted round the equator as noise is
	m_meander((int32_t)std::max<int64_t>(8, ((int64_t)TWO_PI << FRAC) / gridSphere().spacing() >> FRAC))
{
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
	{ ratio(35, 100), ANY, -ANY, -ratio(15, 100), -ANY, ANY, Biome::Desert },
	{ ratio(35, 100), ANY, -ANY, ANY, -ANY, ANY, Biome::Savanna },
	{ 0, ratio(35, 100), -ratio(60, 100), ratio(40, 100), -ANY, -ratio(30, 100), Biome::Mediterranean },
	{ -ANY, ANY, ratio(5, 100), ANY, -ANY, ANY, Biome::Forest },
	{ -ANY, ANY, -ANY, ANY, -ANY, ANY, Biome::Grassland },
};

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

void World::tectonic(const Dir &d, fix &belt, fix &relief) const
{
	belt = between(m_grid->tectonics.belt, d);
	relief = between(m_grid->tectonics.relief, d);
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
	const fix u = this->u(d), v = this->v(d);
	fix belt, relief;
	tectonic(d, belt, relief);
	fix e = elevation(c, d, m_grid->sea_offset, belt, relief);
	if (e >= 0) // only the land wears down; the sea floor stays as made
		e = std::max<fix>(e + between(m_grid->eroded, d) - between(m_grid->base, d), 0);
	fix heat = heatAt(c, u, v, sun, e) + between(m_grid->sea_heat, d);
	// the climate's rain here, in each season; the hemisphere decides which is summer
	fix north = between(m_grid->rain_season[0], d), south = between(m_grid->rain_season[1], d);
	fix rain = (north + south) / 2;
	fix summer = v < HALF ? north : south, winter = v < HALF ? south : north;
	fix season = div(summer - winter, summer + winter);
	// as the grid reads it (grid.cpp): the seasons' fields are interpolated and
	// can dip to nothing in the driest places; below 1/100 it's as dry as dry
	// gets (−1) either way
	fix wet = clamp(log2(maxx(rain, ratio(1, 100))) / 2, -ONE, ONE);

	Biome biome = classify(e, heat, wet, season);
	// a barren body: one bare biome over all of it, sea floor and poles too
	switch (c.surface) {
	case Surface::Living: break;
	case Surface::Rock: biome = Biome::Rock; break;
	case Surface::Sand: biome = Biome::Desert; break;
	case Surface::Ice: biome = Biome::Snow; break;
	}
	return { e, heat, wet, rain, season, biome };
}

// °C for a heat, fitted to the biome thresholds in classify(): snow below
// about −10 °C, taiga and tundra below 0 °C, jungle and desert above 24 °C.
static fix celsius(fix heat)
{
	return toFix(11) + heat * 37;
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
	return celsius(c.heat) + daily;
}

int32_t World::surface(fix e, const Place &d) const
{
	// The coast follows the land between cell centres, which alone would
	// draw it in the cells' straight edges: it wanders by noise about a cell
	// long (a few blocks of height, so flat coasts wander far, cliffs not).
	fix scale = e >= 0 ? toFix(LAND_SCALE) : toFix(SEA_SCALE);
	fix shore = fbm(d, m_code.seed_hash + SHORE, m_meander, 5, HALF);
	fix base = mul(e, scale) + shore * 3;
	// The fine detail fades to nothing at the waterline (never more than half
	// the height over or under it), so it never flips land and sea column by
	// column: the coastline is the base's, a smooth line.
	fix detail = fbm(d, m_code.seed_hash + DETAIL, m_code.size / 64, 3, HALF);
	fix amplitude = std::min<fix>(base >= 0 ? toFix(5) : toFix(2), std::abs(base) / 2);
	return floorInt(base + mul(detail, amplitude));
}

// A water surface in blocks: lakes and rivers sit at the grid's filled level.
static int32_t waterBlocks(fix filled)
{
	return std::max<int32_t>(0, floorInt(filled * LAND_SCALE));
}

// Rivers: segments from each river cell's centre to the centre of the cell it
// drains into. These say how wide, and how high the banks stand over the water.
static const int32_t BANK = 6;   // blocks beside the channel kept above the water
static const fix MAX_WIDTH = toFix(24);

Ground World::ground(const Place &at, fix sun) const
{
	const Grid &g = *m_grid;
	const Sphere &sphere = gridSphere();
	Column c = column(at, sun);
	Ground out{ surface(c.elevation, at), 0, Water::None, c };
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
	if (m_code.surface != Surface::Living)
		return out; // no water on a barren body: its basins stand dry
	if (out.terrain < 0) {
		out.kind = Water::Sea;
		out.water = 0;
	}

	// Lakes: the grid's lake mask, between cell centres. Inside the outline the
	// floor drops under the lake's level; on the rim the ground holds it in.
	// The mask is read a little way off (up to a cell, by noise about a
	// cell long, the place warped), and its half-way line wanders by finer
	// noise: alone, the outline would be the cells' triangles.
	Place warped = at;
	{
		std::array<int64_t, 3> w;
		for (int k = 0; k < 3; k++)
			w[k] = ((int64_t)fbm(at, m_code.seed_hash + SHORE + 2 + (uint32_t)k, m_meander, 3, HALF) * sphere.spacing()) >> 1;
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
		}
	}

	// Rivers: the nearest river segment within reach of this column, from
	// the cells within three spacings of it. Positions are blocks from this
	// column, straight through the planet (this near, the same as along it),
	// to 1/256 of a block: three spacings on a large planet are tens of
	// thousands of blocks, whose squares wouldn't fit 64 bits at 1/65 536.
	constexpr int NEAR = 8; // fractional bits of a position here
	const int64_t reach = 3 * m_cell_blocks + toFix(4);
	// Rivers wander: the column is measured from a little way off, by smooth
	// noise (a third of a cell at most, waves about a cell long), so the
	// straight runs between cell centres meander and their banks still meet.
	std::array<int64_t, 3> wander;
	for (int k = 0; k < 3; k++)
		wander[k] = (mulShift(fbm(at, m_code.seed_hash + MEANDER + (uint32_t)k, m_meander, 2, HALF), m_cell_blocks / 3, FRAC))
				>> (FRAC - NEAR);
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
	// (in a lake or the sea, the river is the water there: no channel, no banks)
	const bool open = out.kind != Water::Lake && out.kind != Water::Sea;
	for (int32_t i : sphere.around(cell(at), [&](int32_t k) {
			return open && blocksFine(chord(Place(sphere.centre(k)), at)) <= reach; })) {
		int32_t r = g.receiver[i];
		if (!open || !g.river(i) || r < 0)
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
		const int64_t dist = (int64_t)isqrt((uint64_t)(px * px + py * py + pz * pz)) << (FRAC - NEAR);
		// wider as more water passes: 3 blocks at the threshold, √ of the flow
		// beyond it, on the default planet; a larger planet's rivers drain
		// more (its cells are larger), and width goes as √ of the flow, so
		// with the planet's size
		// (in 64 bits; never wider than a cell, the land it drains between)
		const int64_t width = std::min({ mulShift(toFix(3) + 2 * sqrtWide64((g.drained[i] << FRAC) / g.river_threshold),
				m_river_scale, FRAC), mulShift(MAX_WIDTH, m_river_scale, FRAC), m_cell_blocks });
		const int64_t half = width / 2;
		if (dist - half < best_gap) {
			best_gap = dist - half;
			best_half = half;
			best_dist = dist;
			int32_t from = waterBlocks(g.filled[i]) - 1;
			int32_t to = g.base[r] < 0 ? 0 : waterBlocks(g.filled[r]) - 1;
			best_level = std::max<int32_t>(0, floorInt(lerp(toFix(from), toFix(to), t)));
		}
	}
	if (best_gap < toFix(BANK)) {
		if (best_dist < best_half) {
			// the channel: a bed under the water, deepest mid-stream
			int32_t depth = 1 + (int32_t)std::min<int64_t>(3, (((best_half - best_dist) >> 1) + HALF) >> FRAC);
			out.terrain = std::min(out.terrain, best_level - depth);
			if (out.kind == Water::None || best_level > out.water) {
				out.water = best_level;
				out.kind = Water::River;
			}
		} else if (out.terrain < best_level + 1) {
			out.terrain = best_level + 1; // the bank holds the river in
		}
	}
	// a bank or a lake's rim raised over the sea's level: dry ground, not sea
	if (out.kind == Water::Sea && out.terrain >= out.water)
		out.kind = Water::None;
	return out;
}

} // namespace terragen
