// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/run/river.h"
#include "src/shared/base/exceptions.h"
#include "library/world/terragen/src/world.h"
#include <cmath>

namespace planetarium {

// (a broad search: the shots want land, a river and light, nothing more exact)
std::pair<double, double> riverToShow(const terragen::WorldCode &code, double date, double sun)
{
	const terragen::World world(code);
	constexpr int ROWS = 120, COLS = 240;
	int64_t widest = -1;
	std::pair<double, double> best{ 0, 0 };
	for (int j = ROWS / 6; j < ROWS * 5 / 6; j++)
		for (int i = 0; i < COLS; i++) {
			const double longitude = 2 * M_PI * (2 * i + 1) / (2.0 * COLS) - M_PI;
			if (std::abs(std::remainder(longitude - sun, 2 * M_PI)) > 1)
				continue;
			const terragen::fix u = terragen::ratio(2 * i + 1, 2 * COLS), v = terragen::ratio(2 * j + 1, 2 * ROWS);
			const terragen::Place at = terragen::World::place(u, v);
			const terragen::Ground g = world.ground(at, world.sunlight(at, (terragen::fix)(date * terragen::ONE)));
			// on the river's line, on land
			if (g.terrain <= 0 || g.river_width <= widest || g.river_dist > g.river_width)
				continue;
			widest = g.river_width;
			// as Painter reads a place: u east from −π, v south from the pole
			best = { M_PI / 2 - M_PI * (2 * j + 1) / (2.0 * ROWS), longitude };
		}
	if (widest < 0)
		throw BaseException("planetarium: no river on land for the ladder");
	return best;
}

} // namespace planetarium
