// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/base/fixed.h"
#include <vector>

/*
	The coasts the sea has cut (sprint 120, phase 5,
	.agents/planning/sprint-120-land-with-a-history.md). Waves wear a coast
	where its rock is weaker than they are strong: soft beds, low plains,
	rifted and broken crust go, hard and high ground stands as headlands.
	A bay it cuts shelters itself: the further in, the less open sea a cell
	faces and the weaker the waves that reach it, so the wearing stops by
	itself. That damping is what makes a coast fractal (Sapoval, Baldassarri
	and Gabrielli 2004, "Self-stabilized fractality of seacoasts through
	damped erosion": it settles at the percolation hull's dimension, 4/3).
*/
namespace terragen {

struct Grid;

namespace coast {

// The grid's base heights (relative to the sea) with the coasts cut; the
// land's share of the planet is kept, the sea's level moved to keep it.
std::vector<fix> cut(const WorldCode &code, const Grid &g);
// How rough each coastal land cell's waves are (0..1): the fetch upwind, and how open it is
std::vector<fix> surf(const WorldCode &code, const Grid &g);

} // namespace coast

} // namespace terragen
