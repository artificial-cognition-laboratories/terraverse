// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "craters.h"
#include "fixed.h"
#include "geology.h"
#include "lid.h"
#include "sphere.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace terragen {

// The coarse grid: the sphere's cells at this frequency (sphere.h), 64 002
// of them, about as many as the flat map's 256² had (planning/sprint-86-sphere.md).
constexpr int32_t GRID_FREQUENCY = 80;
// That sphere, built once and shared.
const Sphere &gridSphere();

/*
	The whole world at coarse resolution, with its water worked out: where
	every cell drains, how much rain passes through it, where it pools into
	lakes, and how far running water has worn the land down. Built once per
	world code (see gridFor) and shared. Cells are gridSphere()'s.
*/
struct Grid
{
	fix sea_offset;                // subtracted from the history's heights: the Land share is exact
	std::vector<fix> base;         // elevation after the sea offset, before erosion
	std::vector<fix> eroded;       // after erosion
	// what erosion changed on the land (eroded − base; 0 under the sea, where
	// a column keeps the floor as made): read between cells, so a coast
	// beside a shelf filled deep doesn't step
	std::vector<fix> worn;
	// what rivers laid down on each cell (elevation, so 1/160 is a block on
	// land): floodplains, fans, filled lakes, the shelf off a river's mouth
	std::vector<fix> sediment;
	std::vector<fix> placer;       // how rich what was laid here is in heavy grains (0..1): placer gold and tin
	int64_t to_deep = 0;           // what rivers carried past the shelf (elevation × fixed area)
	int64_t uplifted = 0;          // what the plates pushed up meanwhile (the same units)
	int64_t hollows = 0;           // what fills hollows too small to be lakes (the same units; not carried)
	std::vector<fix> filled;       // the water surface: = eroded on dry land, above it on lakes
	std::vector<int32_t> receiver; // the cell this one drains into; −1 = the sea (or a polar wall)
	std::vector<int64_t> drained;  // rain-weighted area draining through, in fixed cell units
	std::vector<uint8_t> lake;     // 1 where water pools at least a block deep
	// On lake cells, the lake's one surface: its outlet, the lowest filled
	// level in the basin. (The fill's jitter leaves the rest a hair higher,
	// which in blocks would be steps that spill across the lake.)
	std::vector<fix> lake_level;
	// the climate (climate.h), per cell: rain relative to the land's median in
	// each season ([0] the north's summer, [1] the south's), and how fertile
	// the ground is (0..1: what rock and rivers supply, less what rain washes out)
	std::vector<fix> rain_season[2];
	std::vector<fix> sea_heat; // what the ocean currents add to the heat (negative: cold water), coasts included
	// the yearly mean heat at sea level (currents in it), and what it gains per
	// unit of sunlight above the year's (the seasons; climate.h)
	std::vector<fix> heat, heat_gain;
	fix ice = 0; // the share of the planet frozen over
	std::vector<fix> fertility;
	int64_t river_threshold;       // drained area that makes a river (fixed mean-cell units)
	int64_t stream_threshold;      // …and a stream: four times as much of the land carries one

	// geology (geology.h): the plates under it, each cell's province, the
	// volcanoes, and which volcanoes' slopes reach each cell
	// (volcano_list[volcano_start[i] .. volcano_start[i + 1]])
	Tectonics tectonics;
	std::vector<History::Event> history; // what happened to the plates (history.cpp), oldest first
	std::vector<Province> province;
	std::vector<Volcano> volcanoes;
	// a lid's rise and the canyons split from it (lid.h); no rise: at (0, 0, 0)
	Dir rise{ 0, 0, 0 };
	std::vector<Canyon> canyons;
	std::vector<uint32_t> volcano_start;
	std::vector<uint16_t> volcano_list;
	// impacts (craters.h): craters wider than two cells, stamped into `base`
	// (crater_stamp, elevation) and indexed per cell like the volcanoes; a
	// column reads them exactly instead, and classes from crater_from itself
	std::vector<Crater> craters;
	std::vector<fix> crater_stamp, crater_young; // the old ones' (under the lava), the young's
	std::vector<uint32_t> crater_start, crater_list;
	int crater_from = 0;
	// maria (planning/sprint-95-maria.md): lava that welled up through thin
	// crust into the basins and spread, level. Per cell: how deep it lies
	// (0: none), its surface (where none, the ground before it), its
	// titanium (0..1, each pool its own), and how thin the crust is (0..1)
	std::vector<fix> mare, mare_level, titanium, thin;

	fix rain(int32_t i) const { return (rain_season[0][i] + rain_season[1][i]) / 2; }
	bool river(int32_t i) const { return base[i] >= 0 && drained[i] >= river_threshold; }
	bool stream(int32_t i) const { return base[i] >= 0 && drained[i] >= stream_threshold; } // rivers included
	int32_t size() const { return (int32_t)base.size(); }
};

// The grid for a world code, built on first use and shared after (thread-safe).
std::shared_ptr<const Grid> gridFor(const WorldCode &code);
// Keep built grids in `dir` as well, so a world opened before needn't be
// worked out again. `build` names the generator: only its own files are read
// (a code means the same world, but a changed build may not reach it the same
// way), the others are removed. Not called: nothing is kept (tools, tests of terragen alone).
void keepGridsIn(const std::string &dir, const std::string &build);

// Every cell, lowest first (level with level: lower index first).
std::vector<int32_t> lowestFirst(const std::vector<fix> &height);

// The landmass field: where land is, before the sea is placed.
fix landmass(const WorldCode &code, const Dir &d);

// Where a range is pushed up harder than this (Grid::tectonics.belt), its
// core holds the veins that rivers wear placers from (and none lie there).
constexpr fix VEIN_CORE = ratio(1, 2);

// How much of a column's ground is what rivers laid down (0..1): a
// sediment thickness (Grid::sediment), full at ten blocks.
inline fix alluvium(fix sediment) { return clamp(sediment * 16, 0, ONE); }

// Distinct noise streams, so the fields are independent.
enum Stream : uint32_t { CONTINENT = 0, COAST = 3, RIDGE = 7, HEAT = 13, DETAIL = 41, SOIL_NOISE = 53, BLEND = 59, MEANDER = 71, SHORE = 79, SCOURED = 83 };

} // namespace terragen
