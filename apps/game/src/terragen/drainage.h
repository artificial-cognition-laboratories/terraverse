// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include <cstdint>
#include <vector>

/*
	How water runs over the grid's cells (grid.cpp): the erosion of the
	finished land and the plates' history (history.cpp) both use it. Heights
	are relative to the sea: below 0 is sea.
*/
namespace terragen {

// A small per-cell step (under a fifth of a block), so filled flats meander.
fix jitter(int32_t i);
// Priority-flood: every land cell ends strictly above the cell that reached it.
std::vector<fix> fill(const std::vector<fix> &h);
// Each cell's steepest strictly-lower neighbour on `surface` (or, with a `wander` seed, one
// drawn by slope: grid.cpp); −1 for the sea and pits.
std::vector<int32_t> receivers(const std::vector<fix> &surface, uint32_t wander = 0);
// Cells ordered so each comes after the cell it drains into.
std::vector<int32_t> upstreamOrder(const std::vector<int32_t> &receiver);
// Rain-weighted area draining through each cell.
std::vector<int64_t> accumulate(const std::vector<int32_t> &receiver, const std::vector<int32_t> &order,
		const std::vector<fix> &rain);

} // namespace terragen
