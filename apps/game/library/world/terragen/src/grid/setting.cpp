// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library/world/terragen/src/grid/setting.h"
#include "library/world/terragen/src/base/noise.h"
#include "library/world/terragen/src/grid/grid.h"
#include <queue>

namespace terragen {

// (name, province, still, rise, sink, hard, worn, sea)
const SettingRow SETTINGS[(int)Setting::Count] = {
	{ "craton", Province::Shield, ratio(8, 100), 0, 0, ratio(50, 100), ratio(90, 100), 0 },
	{ "platform", Province::Basin, ratio(15, 100), 0, 0, ratio(130, 100), ratio(90, 100), ratio(50, 100) },
	{ "sag basin", Province::Basin, ratio(10, 100), 0, ratio(70, 100), ratio(150, 100), ratio(85, 100), ONE },
	{ "swell", Province::Shield, ratio(30, 100), ratio(18, 100), 0, ratio(70, 100), ratio(70, 100), 0 },
	{ "flood basalt", Province::Volcanic, ratio(25, 100), ratio(10, 100), 0, ratio(45, 100), ratio(60, 100), 0 },
	{ "rift", Province::Rift, ONE, 0, 0, ONE, 0, ONE },
	{ "failed rift", Province::Basin, ratio(20, 100), 0, ratio(70, 100), ratio(130, 100), ratio(60, 100), ONE },
	{ "margin", Province::Basin, ratio(50, 100), ratio(6, 100), 0, ratio(110, 100), ratio(40, 100), ONE },
	{ "young belt", Province::Orogen, ONE, 0, 0, ONE, 0, 0 },
	{ "old belt", Province::Orogen, ratio(35, 100), ratio(4, 100), 0, ratio(55, 100), ratio(50, 100), 0 },
	{ "foreland", Province::Basin, ratio(15, 100), 0, ratio(50, 100), ratio(160, 100), ratio(60, 100), ONE },
	{ "suture", Province::Orogen, ratio(35, 100), ratio(2, 100), 0, ratio(60, 100), ratio(50, 100), 0 },
	{ "arc", Province::Arc, ONE, 0, 0, ratio(90, 100), 0, 0 },
	{ "volcanic", Province::Volcanic, ONE, 0, 0, ratio(80, 100), 0, 0 },
	{ "ridge", Province::Rift, ONE, 0, 0, ONE, 0, 0 },
	{ "ocean floor", Province::OceanFloor, ONE, 0, 0, ONE, 0, 0 },
	{ "highland", Province::Highland, ONE, 0, 0, ONE, 0, 0 },
	{ "mare", Province::Mare, ONE, 0, 0, ONE, 0, 0 },
};

namespace {

// Noise stream for the sags a plume didn't leave (geology.cpp and plates.h have the others)
const uint32_t SAG = 449;
// Steps (of 15 Myr) a plume's swell lasts once it's carried off, until it
// has cooled into a sag; and how long a sag goes on sinking
const int16_t SWELL_STAYS = 4, SAG_ENDS = 24;
// A flood basalt is worn away in this many steps since its plume
const int16_t FLOOD_LASTS = 16;
// Cells from the sea that a passive edge's coastal plain reaches, and from a
// young belt that its foreland trough does
const int32_t MARGIN = 3, FORELAND = 3;
// Cells in from a converging edge that are its belt now (a range is 2 to 4
// cells, 200 to 400 km, across: the Andes, the Himalaya without Tibet)
const int32_t BELT_EDGE = 2;
// A range that rose within this many steps (of 15 Myr) is young: older, it's
// worn to an old belt (the Alps and Andes are young, the Appalachians old)
const int16_t YOUNG_STEPS = 8;
// Below this (of 5 km), lowland is low enough for a foreland's trough (or wetter, a little higher)
const fix LOW = ratio(12, 100), WET_LOW = ratio(22, 100);

// Each cell's distance in cells (to `reach` + 1) from the cells `from` says, over land
template <typename From>
std::vector<int32_t> spread(const std::vector<fix> &height, int32_t reach, From from)
{
	const Sphere &sphere = gridSphere();
	const int32_t n = sphere.size();
	std::vector<int32_t> out(n, reach + 1);
	std::queue<int32_t> open;
	for (int32_t i = 0; i < n; i++)
		if (from(i)) {
			out[i] = 0;
			open.push(i);
		}
	while (!open.empty()) {
		const int32_t i = open.front();
		open.pop();
		if (out[i] >= reach)
			continue;
		for (const int32_t *p = sphere.neighboursBegin(i); p != sphere.neighboursEnd(i); p++)
			if (height[*p] >= 0 && out[*p] > out[i] + 1) {
				out[*p] = out[i] + 1;
				open.push(*p);
			}
	}
	return out;
}

// What its history made a land cell, but for the troughs and plains that
// depend on its neighbours (the later passes)
Setting own(const WorldCode &c, const Grid &g, int32_t i)
{
	const Tectonics &t = g.tectonics;
	const Past &past = t.past[i];
	const int32_t d = t.distance[i];
	const bool young = t.age[i] < YOUNG_STEPS;
	// (the side going under only its thrust front)
	const bool converging = t.boundary[i] == Boundary::Convergent && d <= (t.side[i] == Side::Subducting ? BELT_EDGE / 2 : BELT_EDGE);
	if ((t.formed[i] == Formed::Arc && young) || (converging && t.side[i] == Side::Overriding))
		return Setting::Arc;
	if ((t.formed[i] == Formed::Orogen && young) || converging)
		return Setting::YoungBelt;
	if ((t.formed[i] == Formed::Rift && t.age[i] < 8) || (t.boundary[i] == Boundary::Divergent && d <= 3))
		return Setting::Rift;
	if (past.flooded && past.plumed <= FLOOD_LASTS)
		return Setting::FloodBasalt;
	if (past.plumed <= SWELL_STAYS)
		return Setting::Swell;
	if (past.suture)
		return Setting::Suture;
	if (t.formed[i] == Formed::Orogen || t.formed[i] == Formed::Arc)
		return Setting::OldBelt;
	if (past.stretched < Past::NEVER)
		return Setting::FailedRift;
	if (past.plumed <= SAG_ENDS ||
			// (and sags whose plume came before the history: broad, far inland)
			(d > 4 && noise(gridSphere().centre(i), c.seed_hash + SAG, 4) > ratio(40, 100)))
		return Setting::Sag;
	return Setting::Craton;
}

} // namespace

std::vector<Setting> settings(const WorldCode &c, const Grid &g, const std::vector<fix> &height)
{
	const Sphere &sphere = gridSphere();
	const Tectonics &t = g.tectonics;
	const int32_t n = sphere.size();
	std::vector<Setting> out(n);
	for (int32_t i = 0; i < n; i++) {
		if (c.processes.eruptions > 0)
			out[i] = Setting::Volcanic;
		else if (c.processes.regime == Regime::Lid)
			out[i] = g.mare[i] > 0 ? Setting::Mare : Setting::Highland;
		else if (height[i] < 0)
			out[i] = t.boundary[i] == Boundary::Divergent && t.distance[i] <= 2 ? Setting::Ridge : Setting::OceanFloor;
		else
			out[i] = own(c, g, i);
	}
	if (c.processes.eruptions > 0 || c.processes.regime == Regime::Lid)
		return out;
	// the quiet land by what's near: the trough before a young range, the
	// coastal plain of an edge an ocean opened from, a platform's cover
	const std::vector<int32_t> to_belt = spread(height, FORELAND, [&](int32_t i) { return out[i] == Setting::YoungBelt; });
	const std::vector<int32_t> to_sea = spread(height, MARGIN, [&](int32_t i) { return height[i] < 0; });
	for (int32_t i = 0; i < n; i++) {
		const Setting s = out[i];
		if (height[i] < 0 || (s != Setting::Craton && s != Setting::Sag && s != Setting::OldBelt))
			continue;
		const fix e = height[i];
		const bool low = e < LOW || (e < WET_LOW && g.rain(i) > ONE);
		if (to_belt[i] <= FORELAND && to_belt[i] > 0 && low)
			out[i] = Setting::Foreland;
		else if (to_sea[i] <= MARGIN && s == Setting::Craton)
			out[i] = Setting::Margin;
		else if (t.past[i].covered && s == Setting::Craton)
			out[i] = Setting::Platform;
	}
	return out;
}

} // namespace terragen
