// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "noise.h"
#include "world.h"
#include <algorithm>

/*
	Soil: what the ground is made of under the grass, as a composition rather
	than a kind. Its mineral part is a point on the soil texture triangle
	(sand, silt and clay), and it holds some organic matter. Where a column
	lands comes from why soils differ:

	- the parent rock (its province): granite weathers sandy, shale and
	  volcanic rock to clay;
	- weathering: warm, wet climates turn minerals to clay (the red clays of
	  the tropics); arid ones leave sand;
	- rivers lay silt and clay on their floodplains;
	- steep slopes keep only the coarse part (the fine washes downhill);
	- cold, wet ground keeps its dead plants: peat.

	The 198 compositions a block can hold are a grid: sand and clay in tenths
	(silt the rest), at three levels of organic matter.
*/
namespace terragen {

namespace {

// the parent material's texture, by province (sand, silt, clay)
struct Texture
{
	fix sand, silt, clay;
};
const Texture PARENT[(int)Province::Count] = {
	{ ratio(55, 100), ratio(30, 100), ratio(15, 100) }, // shield: granite and gneiss
	{ ratio(45, 100), ratio(35, 100), ratio(20, 100) }, // orogen
	{ ratio(30, 100), ratio(35, 100), ratio(35, 100) }, // arc: andesite, tuff
	{ ratio(40, 100), ratio(35, 100), ratio(25, 100) }, // rift
	{ ratio(25, 100), ratio(45, 100), ratio(30, 100) }, // basin: shale, limestone, old sea beds
	{ ratio(30, 100), ratio(30, 100), ratio(40, 100) }, // volcanic: basalt weathers to clay
	{ ratio(30, 100), ratio(40, 100), ratio(30, 100) }, // ocean floor (islands, shores)
	{ ratio(50, 100), ratio(40, 100), ratio(10, 100) }, // highland: anorthosite and breccia
	{ ratio(30, 100), ratio(35, 100), ratio(35, 100) }, // mare: basalt
};

const fix ORGANIC_LEVELS[SOIL_ORGANIC] = { 0, ratio(8, 100), ratio(40, 100) }; // mineral, ordinary topsoil, peat

const int32_t TRIANGLE = 66; // sand and clay in tenths with sand + clay ≤ 10

int32_t triangle(int32_t sand, int32_t clay)
{
	return sand * 11 - sand * (sand - 1) / 2 + clay;
}

} // namespace

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
	const Column &c = ground.column;
	Texture t = PARENT[(int)strata.province];
	fix sand = t.sand, silt = t.silt, clay = t.clay;

	// weathering: warmth and water make clay of the minerals; dryness leaves sand
	fix warm_wet = mul(clamp(c.wet + ratio(2, 10), 0, ONE), clamp(c.heat + ratio(3, 10), 0, ONE));
	clay += mul(warm_wet, ratio(35, 100));
	sand -= mul(warm_wet, ratio(25, 100));
	fix dry = clamp(-c.wet, 0, ONE);
	sand += mul(dry, ratio(30, 100));
	clay -= mul(dry, ratio(15, 100));

	// floodplains: rivers and lakes lay silt and clay beside them on low ground
	Cover cover = this->cover(at, ground);
	if (cover.bank && c.elevation < ratio(15, 100)) {
		silt += ratio(30, 100);
		clay += ratio(10, 100);
	}
	// and wherever they laid sediment down (floodplains, fans, filled lakes)
	fix laid = alluvium(between(m_grid->sediment, at));
	silt += mul(laid, ratio(30, 100));
	clay += mul(laid, ratio(10, 100));
	// steep ground keeps only the coarse part
	fix steep = clamp(cover.slope - ratio(4, 10), 0, ONE);
	sand += mul(steep, ratio(40, 100));
	clay -= mul(steep, ratio(20, 100));

	// a little variety from place to place
	fix wobble = mul(noise(at, m_code.seed_hash + SOIL_NOISE, m_code.size / 48), ratio(8, 100));
	sand += wobble;
	clay -= wobble;

	sand = maxx(sand, 0);
	silt = maxx(silt, 0);
	clay = maxx(clay, 0);
	// organic matter: what plants leave, kept where it's cold and wet (peat), burned off where it's hot and dry
	fix organic = mul(cover.density, ratio(8, 100))
			+ mul(clamp(c.wet, 0, ONE), mul(clamp(-c.heat, 0, ONE), ratio(80, 100)));
	return { sand, silt, clay, clamp(organic, 0, ratio(60, 100)) };
}

uint8_t World::blend(const Place &d, int32_t y) const
{
	// smooth patches a few dozen blocks across, through depth too
	const std::array<int32_t, 3> p = solid(d, y);
	fix n = value3(p[0], p[1], p[2], m_code.seed_hash + BLEND, 24); // −1..1
	return (uint8_t)std::clamp(floorInt(mul(n + ONE, toFix(8))), 0, ROCK_BLENDS - 1);
}

} // namespace terragen
