// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"

/*
	Maria (planning/sprint-95-maria.md): lava that welled up through a lid's
	crust where it's thin and a basin dug deep into it, and spread level
	over the low ground round it, burying the craters older than it. Run on
	the grid between the old craters and the young (grid.cpp).
*/
namespace terragen {

struct Grid;

// How thin each cell's crust is (Grid::thin), before anything lies on it.
void thinCrust(const WorldCode &code, Grid &g);

// Floods `g.base` (the old craters in it) to Processes::flooding of the
// surface, filling Grid::mare, mare_level and titanium.
void flood(const WorldCode &code, Grid &g);

} // namespace terragen
