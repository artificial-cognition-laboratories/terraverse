// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "library/world/terragen/src/code/code.h"
#include "library/world/terragen/src/grid/geology.h"

/*
	The grid's crust before the rivers (grid.cpp builds it): the land the
	plates' history left, with the sea placed, and the impacts on it.
*/
namespace terragen {

struct Grid;

namespace crust {

// The base heights from the history (the sea's offset with them)
void lay(const WorldCode &code, Grid &g, const History &past);
// The craters wider than two cells, into the base (and the maria's lava over the old)
void strike(const WorldCode &code, Grid &g);

} // namespace crust

} // namespace terragen
