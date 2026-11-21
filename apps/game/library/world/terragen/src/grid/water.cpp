// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/water.h"
#include "library/world/terragen/src/grid/drainage.h"
#include "library/world/terragen/src/grid/grid.h"
#include "library/world/terragen/src/grid/climate.h"
#include <algorithm>
#include <tuple>

namespace terragen::water {

namespace {

// Water deeper than a block (at 160 blocks per unit) counts as a lake…
const fix LAKE_DEPTH = ratio(1, 160);
// and dries back from; Bonneville's flats were its ice-age lake's floor)
const int64_t PAN_WET = 3;

} // namespace

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
void stand(Grid &g, const std::vector<fix> &rain, const std::vector<fix> &melted)
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

} // namespace terragen::water
