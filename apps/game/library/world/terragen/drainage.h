// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/fixed.h"
#include <cstdint>
#include <vector>

/*
	How water runs over the grid's cells: the erosion (erosion.h), the
	finished land's rivers and lakes (grid.cpp) and the plates' history
	(history.cpp) all use it. Heights are relative to the sea: below 0 is sea.
*/
namespace terragen {

// Each cell's steepest strictly-lower neighbour on `surface` (the sea's at
// its surface, 0); −1 for the sea and for pits.
std::vector<int32_t> receivers(const std::vector<fix> &surface);
// Cells ordered so each comes after the cell it drains into.
std::vector<int32_t> upstreamOrder(const std::vector<int32_t> &receiver);
// Rain-weighted area draining through each cell.
std::vector<int64_t> accumulate(const std::vector<int32_t> &receiver, const std::vector<int32_t> &order,
		const std::vector<fix> &rain);

/*
	Water's way over land with pits in it, without filling them (Cordonnier,
	Bovy & Braun 2019): each cell drains by steepest descent; each pit's
	basin is joined to the sea by a minimum spanning tree over the passes
	between basins, and the path from its pit up to its pass is reversed, so
	every cell reaches the sea. A pit's basin holds a lake to its pass (or to
	the lake it spills into, if that stands higher): `surface`.

	With no sea at all, the lowest pit is where everything ends.

	With a water balance, a basin spills only if the water reaching it
	(its own runoff and what spills from upstream) outruns what its lake
	gives back to the air before it reaches the pass. One that doesn't is
	closed: its pit is where its water ends, and its lake stands where
	inflow and evaporation match.
*/
struct Balance
{
	const std::vector<fix> &runoff;      // what each cell sheds, in rain's units (1: the land's typical rain)
	const std::vector<fix> &evaporation; // what open water over it gives back, in the same units
};
struct Routing
{
	std::vector<int32_t> receiver; // −1: where water ends (the sea, a closed basin's pit)
	std::vector<int32_t> order;    // each cell after its receiver
	std::vector<fix> surface;      // the land, or the lake standing over it
};
Routing route(const std::vector<fix> &h, const Balance *balance = nullptr);

} // namespace terragen
