// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

// terragen's tests keep their grids where a source build of the game does
// (~/.terraverse-dev/cache/engine/grids):
// a planet's worked out once, by whichever ran first, then read in moments.
#define DOCTEST_CONFIG_IMPLEMENT
#include "tests/library/check.h"
#include "library/world/terragen/src/grid/grid.h"

int main(int argc, char **argv)
{
	terragen::keepGridsIn(TERRAGEN_TEST_GRIDS, terragen::TERRAGEN_SOURCES, terragen::GRID_SOURCES);
	return doctest::Context(argc, argv).run();
}
