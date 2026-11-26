// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/base/fixed.h"
#include <vector>

/*
	Water on the land (the grid's: grid.cpp builds it): how much open water
	gives back and rain runs off, the lakes and pans that stand on the
	finished land, and how much water makes a river.
*/
namespace terragen {

struct Grid;

namespace water {

// Open water's evaporation a year, in rain's units, by the yearly heat
fix evaporation(fix heat);
// What rain runs off the land (Turc–Pike)
fix runoff(fix rain, fix heat);
// The water on the finished land: its ways, lakes and salt pans
void stand(Grid &g, const std::vector<fix> &rain, const std::vector<fix> &melted);
// How much gathered rain makes a river (cells of it), by the Rivers setting
int64_t riverThreshold(const WorldCode &code);

} // namespace water

} // namespace terragen
