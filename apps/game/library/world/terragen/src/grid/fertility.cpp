// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/fertility.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/grid/climate.h"
#include "library/world/terragen/src/base/noise.h"

namespace terragen {

namespace {

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

} // namespace

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

} // namespace terragen
