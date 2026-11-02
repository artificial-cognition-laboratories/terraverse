// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "world.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace terragen {

/*
	The model sampled on a resolution × resolution grid (row 0 = the north
	edge), quantized for the page: elevation × 10000 (int16), heat, wetness
	(log₂ of the year's rain, halved) and season (when the rain falls: 1
	summer .. −1 winter) × 100 (int8, clamped), fertility × 255 (uint8), the ocean currents' heat × 100 (int8), the
	biome index (see BIOME_NAMES), and a lake flag. Rivers come as segments
	between the coarse grid's cells, 9 bytes each: where it runs from and to
	(u, v each, uint16 little-endian, 0..65535 across the map) and strength
	(1..255: √ of flow over the river threshold, ×16).
*/
struct Preview
{
	int32_t resolution;
	std::vector<int16_t> elevation;
	std::vector<int8_t> heat;
	std::vector<int8_t> wet;
	std::vector<int8_t> season;
	std::vector<uint8_t> fertility;
	std::vector<int8_t> current; // what the ocean currents add to the heat × 100
	std::vector<uint8_t> biome;
	std::vector<uint8_t> lake;
	std::vector<uint8_t> rivers;  // 9 bytes per segment, as above
	// geology, per pixel: the province (PROVINCE_NAMES), the plate, and the
	// boundary kind where a plate edge runs through (0 none, 1 converging,
	// 2 spreading, 3 sliding); volcanoes as 5 bytes each: u and v (uint16,
	// little-endian, 0..65535 across the world) and 1 if active
	std::vector<uint8_t> province;
	std::vector<uint8_t> plate;
	std::vector<uint8_t> boundary;
	std::vector<uint8_t> volcanoes;
	// what happened to the plates: million years before today, and the kind
	struct Moment { int32_t ago; History::Kind kind; };
	std::vector<Moment> history;
};

// `date` (0..1) shows that day's climate; none shows the yearly mean.
Preview preview(const World &world, std::optional<fix> date, int32_t resolution);

} // namespace terragen
