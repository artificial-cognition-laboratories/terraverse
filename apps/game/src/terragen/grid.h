// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include "geology.h"
#include "sphere.h"
#include <cstdint>
#include <memory>
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
	fix sea_offset;                // subtracted from raw elevation: the Land share is exact
	std::vector<fix> base;         // elevation after the sea offset, before erosion
	std::vector<fix> eroded;       // after erosion
	// what rivers laid down on each cell (elevation, so 1/160 is a block on
	// land): floodplains, fans, filled lakes, the shelf off a river's mouth
	std::vector<fix> sediment;
	int64_t to_deep = 0;           // what rivers carried past the shelf (elevation × fixed area)
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
	std::vector<fix> fertility;
	int64_t river_threshold;       // drained area that makes a river (fixed mean-cell units)

	// geology (geology.h): the plates under it, each cell's province, the
	// volcanoes, and which volcanoes' slopes reach each cell
	// (volcano_list[volcano_start[i] .. volcano_start[i + 1]])
	Tectonics tectonics;
	std::vector<Province> province;
	std::vector<Volcano> volcanoes;
	std::vector<uint32_t> volcano_start;
	std::vector<uint16_t> volcano_list;

	fix rain(int32_t i) const { return (rain_season[0][i] + rain_season[1][i]) / 2; }
	bool river(int32_t i) const { return base[i] >= 0 && drained[i] >= river_threshold; }
	int32_t size() const { return (int32_t)base.size(); }
};

// The grid for a world code, built on first use and shared after (thread-safe).
std::shared_ptr<const Grid> gridFor(const WorldCode &code);

// The landmass field: where land is, before the sea is placed.
fix landmass(const WorldCode &code, const Dir &d);
// Elevation with the sea at `sea_offset` of the landmass field: 0 is the
// coastline, mountains rise from it inland.
// `belt` (0..1) is how strongly mountains rise here, `relief` what plate
// boundaries add (rifts, ridges, trenches); both from Tectonics.
fix elevation(const WorldCode &code, const Dir &d, fix sea_offset, fix belt, fix relief);

// Distinct noise streams, so the fields are independent.
enum Stream : uint32_t { CONTINENT = 0, COAST = 3, RIDGE = 7, HEAT = 13, DETAIL = 41, SOIL_NOISE = 53, BLEND = 59, MEANDER = 71, SHORE = 79 };

} // namespace terragen
